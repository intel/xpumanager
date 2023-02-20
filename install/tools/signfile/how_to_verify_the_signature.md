# Verify Signature

## Ubuntu:
```sh
gpg --import XPUM_PGP_KEY.pem
apt-get install dpkg-sig
dpkg-sig --verify xpumanager.xxx.deb
#should display "GOODSIG…"
```

## CentOS:
```sh
rpm --import XPUM_PGP_KEY.pem
rpm -K xpumanager.xxx.rpm
#should display "…digests signatures OK"
```

## amcmcli:
```sh
gpg --import XPUM_PGP_KEY.pem
gpg --verify amcmcli.sig amcmcli
#should display "gpg: Good signature from "CN=XPUM_PGP_KEY" [unknown]"
```