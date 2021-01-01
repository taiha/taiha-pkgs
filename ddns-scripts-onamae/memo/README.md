メモ

## 成功

```
tofu@Tofu-B460W10:~$ echo -e "LOGIN\nUSERID:*******\nPASSWORD:*************\n.\nMODIP\nDOMNAME:taiha.net\nIPV4:49.129.168.128\n.\nLOGOUT\n.\n" | openssl s_client --connect ddnsclient.onamae.com:65010 -quiet
depth=2 OU = GlobalSign Root CA - R3, O = GlobalSign, CN = GlobalSign
verify return:1
depth=1 C = BE, O = GlobalSign nv-sa, CN = GlobalSign GCC R3 DV TLS CA 2020
verify return:1
depth=0 CN = *.onamae.com
verify return:1
000 COMMAND SUCCESSFUL
.
000 COMMAND SUCCESSFUL
.
000 COMMAND SUCCESSFUL
.
000 COMMAND SUCCESSFUL
.
tofu@Tofu-B460W10:~$ echo -e "LOGIN\nUSERID:*******\nPASSWORD:**************\n.\nMODIP\nDOMNAME:taiha.net\nIPV4:49.129.168.128\n.\nLOGOUT\n.\n" | openssl s_client --connect ddnsclient.onamae.com:65010 -quiet 2> /dev/null
000 COMMAND SUCCESSFUL
.
000 COMMAND SUCCESSFUL
.
000 COMMAND SUCCESSFUL
.
000 COMMAND SUCCESSFUL
.
tofu@Tofu-B460W10:~$

```

## 不正なIPv4アドレス形式

### 無効な文字を含む場合

```
tofu@Tofu-B460W10:~$ echo -e $UPDATE_DDNS
LOGIN
USERID:*******
PASSWORD:************
.
MODIP
DOMNAME:taiha.net
IPV4:49.x29.168.128
.
LOGOUT
.

tofu@Tofu-B460W10:~$ echo -e $UPDATE_DDNS | openssl s_client --connect ddnsclient.onamae.com:65010 -quiet 2> /dev/null
000 COMMAND SUCCESSFUL
.
000 COMMAND SUCCESSFUL
.
tofu@Tofu-B460W10:~$
```

### ブロック数または数値範囲が不正

```
004 IPADDRESS ERROR
```

```
tofu@Tofu-B460W10:~$ echo -e $UPDATE_DDNS
LOGIN
USERID:*******
PASSWORD:**************
.
MODIP
DOMNAME:taiha.net
IPV4:12.168.128
.
LOGOUT
.

tofu@Tofu-B460W10:~$ echo -e $UPDATE_DDNS | openssl s_client --connect ddnsclient.onamae.com:65010 -quiet 2> /dev/null
000 COMMAND SUCCESSFUL
.
000 COMMAND SUCCESSFUL
.
004 IPADDRESS ERROR
.
000 COMMAND SUCCESSFUL
.
tofu@Tofu-B460W10:~$ echo -e $UPDATE_DDNS
LOGIN
USERID:*******
PASSWORD:**************
.
MODIP
DOMNAME:taiha.net
IPV4:49.268.168.128
.
LOGOUT
.

tofu@Tofu-B460W10:~$ echo -e $UPDATE_DDNS | openssl s_client --connect ddnsclient.onamae.com:65010 -quiet 2> /dev/null
000 COMMAND SUCCESSFUL
.
000 COMMAND SUCCESSFUL
.
004 IPADDRESS ERROR
.
000 COMMAND SUCCESSFUL
.
tofu@Tofu-B460W10:~$
```

## ログイン失敗

```
002 LOGIN ERROR
```

```
tofu@Tofu-B460W10:~$ echo -e $UPDATE_DDNS
LOGIN
USERID:*******
PASSWORD:(invalid password)
.
MODIP
DOMNAME:taiha.net
IPV4:49.129.168.128
.
LOGOUT
.

tofu@Tofu-B460W10:~$ echo -e $UPDATE_DDNS | openssl s_client --connect ddnsclient.onamae.com:65010 -quiet 2> /dev/null                           000 COMMAND SUCCESSFUL
.
000 COMMAND SUCCESSFUL
.
002 LOGIN ERROR
.
000 COMMAND SUCCESSFUL
.
tofu@Tofu-B460W10:~$
```
