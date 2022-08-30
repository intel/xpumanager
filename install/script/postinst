#!/bin/bash

if [ -n "${RPM_INSTALL_PREFIX}" ]; then
  ln -s ${RPM_INSTALL_PREFIX}/bin/xpu-smi /usr/bin/xpu-smi  
else
  ln -s /opt/xpum/bin/xpu-smi /usr/bin/xpu-smi
fi
