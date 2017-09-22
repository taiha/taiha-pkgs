# mackerel-agent package
[mackerel-agent](https://github.com/mackerelio/mackerel-agent) package for OpenWrt/LEDE-Project.

## Note
Now, It may operates correctly only in the environment of MIPS32 (LITTLE/BIG ENDIANESS) and ARM32 (LITTLE ENDIANESS).

Targets confirmed operation (LEDE-Project):

- ar71xx (RouterBoard 750GL)
- ramips (Planex VR500)
- bcm53xx (Buffalo WZR-900DHP)


## Prepare
Install golang compiler v1.8.3 with soft-float support for MIPS32.

Refer to [mackerel-agentをLinux/MIPS32環境で動かしてみた - Qiita](http://qiita.com/hnw/items/a1faee61fc1a47cba5c9)

And, upx-ucl package is required on host environment.

```apt install upx-ucl```

## Feed
Copy feeds.conf.default to feeds.conf, and add following line:

```src-git taiha https://github.com/taiha/taiha-pkgs.git```

and update feeds.

```$ ./scripts/feeds update -a```

```$ ./scripts/feeds install -a```

## menuconfig
Select and check the following package:

```(root) -> Administration -> mackerel-agent```

## Plugins

- [mackerel-plugin-conntrack (official)](https://github.com/mackerelio/mackerel-agent-plugins/tree/master/mackerel-plugin-conntrack)
- [mackerel-plugin-linux (official)](https://github.com/mackerelio/mackerel-agent-plugins/tree/master/mackerel-plugin-linux)
- [mackerel-plugin-multicore (official)](https://github.com/mackerelio/mackerel-agent-plugins/tree/master/mackerel-plugin-multicore)
- [mackerel-plugin-uptime (official)](https://github.com/mackerelio/mackerel-agent-plugins/tree/master/mackerel-plugin-uptime)
- mackerel-plugin-temp (original)

  - Get system temperature (thermal_zone0) if available.

- mackerel-plugin-portlink (original)

  - Get link speed of ethernet ports on switch0 (vlans <= 5).


## Special thanks

- Shell command in Makefile [Makefile#L117](../Makefile#L117)

  - orumin (kotatsu_mi)