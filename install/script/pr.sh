#!/bin/bash

LOG_FILE="/dev/null"

function RemoveUser()
{
  group=$( userdel -rf ${1} 2>&1 | grep "group ${1} not removed" )    
  if [ -n "$group" ]; then
	 	groupdel xpum -f >> ${LOG_FILE} 2>&1	
	fi
}

systemctl stop xpum >> ${LOG_FILE} 2>&1 
systemctl disable xpum >> ${LOG_FILE} 2>&1 
systemctl stop xpum_rest >> ${LOG_FILE} 2>&1 
systemctl disable xpum_rest >> ${LOG_FILE} 2>&1 
rm -rf /lib/systemd/system/xpum.service
rm -rf /lib/systemd/system/xpum_rest.service
rm -rf /usr/bin/xpu-smi
systemctl daemon-reload
RemoveUser xpum
