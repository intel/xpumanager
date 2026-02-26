# Verify Signature

## DEB package:
```sh
gpg --import intel_graphics_linux_pgp.pem
apt-get install dpkg-sig
dpkg-sig --verify xpumanager.xxx.deb
#should display "GOODSIG…"
```

## ZIP package:
```sh
#install openssl
openssl pkcs7 -print_certs -inform der -in xpu-smi-xxx.sig > certs.pem
openssl smime -verify -in xpu-smi-xxx.sig -inform der -content xpu-smi-xxx.zip -CAfile certs.pem > temp.txt
#should display "Verification successful"
```