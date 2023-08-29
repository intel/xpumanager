# Verify Signature

## DEB package:
```sh
gpg --import intel_graphics_linux_pgp_2021.pem
apt-get install dpkg-sig
dpkg-sig --verify xpumanager.xxx.deb
#should display "GOODSIG…"
```

## RPM package:
```sh
rpm --import intel_graphics_linux_pgp_2021.pem
rpm -K xpumanager.xxx.rpm
#should display "…digests signatures OK"
```

## ZIP package:
```sh
#install openssl
openssl pkcs7 -print_certs -inform der -in xpu-smi-xxx.sig > certs.pem
openssl smime -verify -in xpu-smi-xxx.sig -inform der -content xpu-smi-xxx.zip -noverify certs.pem > temp.txt 
#should display "Verification successful"
```

## amcmcli:
```sh
gpg --import intel_graphics_linux_pgp_2021.pem
gpg --verify amcmcli.sig amcmcli
#should display "gpg: Good signature from "CN=XPUM_PGP_KEY" [unknown]"
```