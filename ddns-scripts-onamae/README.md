## 設定例

```
config service "onamae_example"
	option service_name	"onamae.com"		# 更新スクリプトのサービス名（"onamae.com" を指定）
	option lookup_host	"yourhost.example.com"	# 更新確認を行うドメイン（基本的に更新対象ドメインと同じ）
	option domain		"yourhost.example.com"	# 更新対象ドメイン
	option username		"your_username"		# お名前.comの登録ユーザー名
	option password		"your_password"		# お名前.comの登録パスワード
	option interface	"wan"		# ddns-scriptsをトリガするインターフェース
	option ip_source	"network"	# IPアドレス取得ソースの種類
	option ip_network	"wan"		# IPアドレス取得ソースの仮想インターフェース（sourceが "network" の場合）
	option dns_server	"01.dnsv.jp"	# 更新確認問い合わせ先サーバ
```