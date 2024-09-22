/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Read/Write firmware information on FortiGate/FortiWiFi 30E/5xE
 *
 * Copyright (C) 2024 INAGAKI Hiroshi <musashino.open@gmail.com>
 *
 * based on zyxel-bootconfig.c
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

#define FWINFO_IMGNAME_LEN	0x20
#define FWINFO_MAGIC		"FSOC"
#define FWINFO_PARTN_ACTIVE	0x00000080

#define BLOCKSIZE		0x200	/* 512 bytes */
#define BL2BY(x)		(x * BLOCKSIZE)
#define BY2BL(x)		(x / BLOCKSIZE + ((x % BLOCKSIZE) ? 1 : 0))

struct part_map {
	unsigned int ofs;
	unsigned int len;
} __attribute__ ((packed));

struct fortigate_fwinfo {
	unsigned int unknown32_1[4];	/* 0x0-0xf */
	char image1_name[32];
	char image2_name[32];
	unsigned int unknown32_2[68];	/* 0x50-0x15f */
	char magic[4];
	unsigned int unknown32_3[3];	/* 0x164-0x16f */
	unsigned char active_image;
	unsigned char unknown8_1[3];	/* 0x171-0x173 */
	unsigned int dtb_offset;
	unsigned int fwinfo_offset;
	unsigned int unknown32_4;	/* 0x17c-0x17f */
	struct part_map image1_kernel;
	struct part_map image1_rootfs;
	struct part_map image2_kernel;
	struct part_map image2_rootfs;
	struct part_map _part1;
	unsigned char unknown8_2[22];	/* 0x1a8-0x1bd */
	unsigned int part1_flags;
	unsigned int unknown32_5;	/* 0x1c2-0x1c5 */
	struct part_map part1;
	unsigned int part2_flags;
	unsigned int unknown32_6;	/* 0x1d2-0x1d5 */
	struct part_map part2;
	unsigned int config_flags;
	unsigned int unknown32_7;	/* 0x1e2-0x1e5 */
	struct part_map config;
	unsigned char unknown8_3[18];	/* 0x1ee-0x1ff */
} __attribute__ ((packed));

struct fortigate_fwinfo_mtd {
	struct mtd_info_user mtd_info;
	int fd;
};

static void fortigate_fwinfo_mtd_close(struct fortigate_fwinfo_mtd *mtd) {
	close(mtd->fd);
}


static int fortigate_fwinfo_mtd_open(struct fortigate_fwinfo_mtd *mtd,
				     const char *mtd_name) {
	int ret = 0;

	mtd->fd = open(mtd_name, O_RDWR | O_SYNC);
	if (mtd->fd < 0) {
		fprintf(stderr, "Could not open mtd device: %s (%d)\n",
			mtd_name, mtd->fd);
		ret = -1;
		goto out;
	}

	if (ioctl(mtd->fd, MEMGETINFO, &mtd->mtd_info)) {
		fprintf(stderr, "Could not get MTD device info from %s\n",
			mtd_name);
		ret = -1;
		fortigate_fwinfo_mtd_close(mtd);
		goto out;
	}

out:
	return ret;
}

static void print_image_info(struct fortigate_fwinfo *fwinfo, int index)
{
	struct part_map *kern, *root, *part;
	char *name, buf[FWINFO_IMGNAME_LEN + 1];

	if (index == 0) {
		name = fwinfo->image1_name;
		kern = &fwinfo->image1_kernel;
		root = &fwinfo->image1_rootfs;
		part = &fwinfo->part1;
	} else {
		name = fwinfo->image2_name;
		kern = &fwinfo->image2_kernel;
		root = &fwinfo->image2_rootfs;
		part = &fwinfo->part2;
	}

	snprintf(buf, FWINFO_IMGNAME_LEN + 1, "%s", name);
	if (strlen(buf) == 0 || kern->len == 0) {
		printf("  %d: (empty)\n\n", index);
		return;
	}

	printf("%c %d: \"%s\"\n"
	       "\tkernel: 0x%08x@0x%08x\n"
	       "\trootfs: 0x%08x@0x%08x\n"
	       "\tdata  : 0x%08x@0x%08x\n\n",
	       fwinfo->active_image == index ? '*' : ' ', index, buf,
	       BL2BY(kern->len), BL2BY(kern->ofs),
	       BL2BY(root->len), BL2BY(root->ofs),
	       BL2BY(part->len), BL2BY(part->ofs));
}

static int fortigate_fwinfo_read(struct fortigate_fwinfo *fwinfo,
				 struct fortigate_fwinfo_mtd *mtd) {
	int ret;

	ret = pread(mtd->fd, fwinfo, sizeof(*fwinfo), 0);
	if (ret < 0)
		return errno;
	else if (ret != sizeof(*fwinfo))
		return -EIO;

	return 0;
}

static int fortigate_fwinfo_write(struct fortigate_fwinfo *fwinfo,
				  struct fortigate_fwinfo_mtd *mtd,
				  int index, char *name, bool active,
				  struct part_map *kmap, struct part_map *rmap,
				  struct part_map *pmap)
{
	struct erase_info_user ei;
	struct part_map *maps[] = {kmap, rmap, pmap};
	struct part_map *_maps[3];
	char *_name;
	int i, ret;

	if (index == 0) {
		_name = fwinfo->image1_name;
		_maps[0] = &fwinfo->image1_kernel;
		_maps[1] = &fwinfo->image1_rootfs;
		_maps[2] = &fwinfo->part1;
	} else {
		_name = fwinfo->image2_name;
		_maps[0] = &fwinfo->image2_kernel;
		_maps[1] = &fwinfo->image2_rootfs;
		_maps[2] = &fwinfo->part2;
	}

	if (name)
		strncpy(_name, name, FWINFO_IMGNAME_LEN);
	for (i = 0; i < 3; i++) {
		if (maps[i]->len != UINT_MAX)
			_maps[i]->len = maps[i]->len;
		if (maps[i]->ofs != UINT_MAX)
			_maps[i]->ofs = maps[i]->ofs;
	}
	if (active) {
		fwinfo->active_image = index;

		/* set active flags for part1/part2 */
		if (index == 0) {
			fwinfo->part1_flags |= FWINFO_PARTN_ACTIVE;
			fwinfo->part2_flags &= ~FWINFO_PARTN_ACTIVE;
		} else {
			fwinfo->part1_flags &= ~FWINFO_PARTN_ACTIVE;
			fwinfo->part2_flags |= FWINFO_PARTN_ACTIVE;
		}
	}

	ei.start = 0;
	ei.length = mtd->mtd_info.erasesize;
	ret = ioctl(mtd->fd, MEMERASE, &ei);
	if (ret < 0)
		return ret;

	ret = pwrite(mtd->fd, fwinfo, sizeof(*fwinfo), 0);
	if (ret < 0)
		return errno;
	else if (ret != sizeof(*fwinfo))
		return -EIO;

	return 0;
}

static int parse_partmap(struct part_map *map, char *str)
{
	char *next;
	int i;

	if (strlen(str) == 0) {
		errno = -EINVAL;
		goto exit;
	}

	for (i = sizeof(*map) / sizeof(unsigned int) - 1; i >= 0; i--) {
		next = strsep(&str, "@");
		if (!next || strlen(next) == 0) {
			*((unsigned int *)map + i) = UINT_MAX; /* don't update */
		} else {
			errno = 0;
			*((unsigned int *)map + i)
				= BY2BL(strtoul(next, NULL, 0));
			if (errno)
				goto exit;
		}
	}

exit:
	if (errno)
		fprintf(stderr, "Failed to parse partmap\n");
	return errno;
}

static void print_usage(char *program)
{
	printf("Usage: %s <mtd-device> [options]\n\n", program);
	printf("Options:\n\n"
	       "  -i <index>\t\tset image <index> for operation (0/1)\n"
	       "  -a\t\t\tmark the specified image as active\n"
	       "  -n <name>\t\tset <name> to the specified image\n"
	       "  -k <size>[@<offset>]\tset <size> (and <offset>) of kernel to the specified image\n"
	       "  -r <size>[@<offset>]\tset <size> (and <offset>) of rootfs to the specified image\n"
	       "  -d <size>[@<offset>]\tset <size> (and <offset>) of data to the specified image\n\n");
	printf("Note: <size> and <offset> will be aligned by 0x%x (%d bytes) and written to flash\n",
	       BLOCKSIZE, BLOCKSIZE);
}

int main(int argc, char *argv[])
{
	struct fortigate_fwinfo_mtd mtd;
	struct fortigate_fwinfo fwinfo;
	struct part_map kmap, rmap, pmap;
	const char *mtd_name;
	char *imgname = NULL;
	int ret, c, i = -1;
	bool update = false, active = false;

	if (argc < 2) {
		print_usage(argv[0]);
		return 1;
	}

	mtd_name = argv[1];

	if (fortigate_fwinfo_mtd_open(&mtd, mtd_name)) {
		fprintf(stderr, "Failed to open %s!\n", mtd_name);
		return 1;
	}

	ret = fortigate_fwinfo_read(&fwinfo, &mtd);
	if (ret) {
		fprintf(stderr, "Failed to read firmware information!\n");
		fortigate_fwinfo_mtd_close(&mtd);
		return ret;
	}

	if (strcmp(fwinfo.magic, FWINFO_MAGIC)) {
		fprintf(stderr,
			"No valid firmware information in %s!\n", mtd_name);
		fortigate_fwinfo_mtd_close(&mtd);
		return -EINVAL;
	}

	/* fill *map with 0xff to prevent updating by default */
	memset(&kmap, 0xff, sizeof(struct part_map));
	memset(&rmap, 0xff, sizeof(struct part_map));
	memset(&pmap, 0xff, sizeof(struct part_map));

	while ((c = getopt(argc, argv, "-hi:n:k:r:d:a")) != -1) {
		switch (c) {
		case 'i':
			errno = 0;
			i = strtol(optarg, NULL, 10);
			if (errno || (i != 0 && i != 1)) {
				printf("invalid image index specified!\n");
				fortigate_fwinfo_mtd_close(&mtd);
				return errno ? errno : -EINVAL;
			}
			break;
		case 'n':
			imgname = optarg;
			update = true;
			break;
		case 'k':
			ret = parse_partmap(&kmap, optarg);
			if (ret) {
				fortigate_fwinfo_mtd_close(&mtd);
				return ret;
			}
			update = true;
			break;
		case 'r':
			ret = parse_partmap(&rmap, optarg);
			if (ret) {
				fortigate_fwinfo_mtd_close(&mtd);
				return ret;
			}
			update = true;
			break;
		case 'd':
			ret = parse_partmap(&pmap, optarg);
			if (ret) {
				fortigate_fwinfo_mtd_close(&mtd);
				return ret;
			}
			update = true;
			break;
		case 'a':
			active = true;
			update = true;
			break;
		case '?':
			fortigate_fwinfo_mtd_close(&mtd);
			print_usage(argv[0]);
			return -EINVAL;
		}
	}

	if (!update && i == -1) {
		printf("Magic        : \"%s\"\n", fwinfo.magic);
		printf("DTB offset   : 0x%08x\n", BL2BY(fwinfo.dtb_offset));
		printf("FWinfo offset: 0x%08x\n", BL2BY(fwinfo.fwinfo_offset));
		printf("Config       : 0x%08x@0x%08x\n",
		       BL2BY(fwinfo.config.len),
		       BL2BY(fwinfo.config.ofs));
		printf("Block Size   : 0x%x\n\n", BLOCKSIZE);

		printf("Images:\n\n");
		print_image_info(&fwinfo, 1);
		print_image_info(&fwinfo, 2);
	} else if (i != -1) {
		if (update) {
			ret = fortigate_fwinfo_write(&fwinfo, &mtd,
						     i, imgname, active,
						     &kmap, &rmap, &pmap);
			if (ret) {
				fprintf(stderr,
					"Failed to write firmware information!\n");
				fortigate_fwinfo_mtd_close(&mtd);
				return ret;
			}
			fortigate_fwinfo_read(&fwinfo, &mtd);
		}
		print_image_info(&fwinfo, i);
	} else {
		printf("No image index specified!\n");
		return -EINVAL;
	}

	return 0;
}
