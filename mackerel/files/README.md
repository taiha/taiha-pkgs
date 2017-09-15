# mackerel-agent package
[mackerel-agent](https://github.com/mackerelio/mackerel-agent) package for OpenWrt/LEDE-Project.

## Note
Now, It operates correctly only in the environment of MIPS32 (LITTLE/BIG ENDIAN).

Targets confirmed operation (LEDE-Project):

- ar71xx
- ramips


## Prepare
Install golang compiler v1.8.3 with soft-float support for MIPS32.

Refer to [mackerel-agentをLinux/MIPS32環境で動かしてみた - Qiita](http://qiita.com/hnw/items/a1faee61fc1a47cba5c9)

## Feed
Copy feeds.conf.default to feeds.conf, and add following line:

```src-git taiha https://github.com/taiha/taiha-pkgs.git;mackerel```

and update feeds.

```$ ./scripts/feeds update -a```

```$ ./scripts/feeds install -a```

## menuconfig
Select and check the following package:

```(root) -> Administration -> mackerel-agent```