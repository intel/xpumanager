#!/bin/bash

LOG_FILE="/dev/null"

function CreateUser()
{
  useradd "$1" -s /bin/sh >> ${LOG_FILE} 2>&1
  if [ $? -ne 0 ]; then
   	echo "CreateUser $1 failed!"
  fi
}

CreateUser xpum

if [ -n "${RPM_INSTALL_PREFIX}" ]; then
  mv -f ${RPM_INSTALL_PREFIX}/xpum.service /lib/systemd/system/xpum.service
  mv -f ${RPM_INSTALL_PREFIX}/xpum_rest.service /lib/systemd/system/xpum_rest.service

  sed -e "s:@CPACK_PACKAGING_INSTALL_PREFIX@:${RPM_INSTALL_PREFIX}:g" /usr/lib/systemd/system/xpum.service > /usr/lib/systemd/system/xpum.service.n
  mv -f /usr/lib/systemd/system/xpum.service.n /usr/lib/systemd/system/xpum.service

  sed -e "s:@CPACK_PACKAGING_INSTALL_PREFIX@:${RPM_INSTALL_PREFIX}:g" /usr/lib/systemd/system/xpum_rest.service > /usr/lib/systemd/system/xpum_rest.service.n
  mv -f /usr/lib/systemd/system/xpum_rest.service.n /usr/lib/systemd/system/xpum_rest.service 

  chown -R xpum ${RPM_INSTALL_PREFIX}
  chmod g+x ${RPM_INSTALL_PREFIX}/keytool.sh
  ln -s ${RPM_INSTALL_PREFIX}/bin/xpu-smi /usr/bin/xpu-smi  
else
  mv -f /opt/xpum/xpum.service /lib/systemd/system/xpum.service
  mv -f /opt/xpum/xpum_rest.service /lib/systemd/system/xpum_rest.service
  chown -R xpum /opt/xpum
  chmod g+x /opt/xpum/keytool.sh
  ln -s /opt/xpum/bin/xpu-smi /usr/bin/xpu-smi
fi

sed -e "s:@CPACK_XPUM_LIB_INSTALL_DIR@:/usr/lib:g" /usr/lib/systemd/system/xpum.service > /usr/lib/systemd/system/xpum.service.n
mv -f /usr/lib/systemd/system/xpum.service.n /usr/lib/systemd/system/xpum.service

# Start xpum service when systemctl is available
if [ -x "$(command -v systemctl)" ]; then
  systemctl daemon-reload >> ${LOG_FILE} 2>&1
  systemctl enable xpum >> ${LOG_FILE} 2>&1 
  systemctl start xpum
fi
