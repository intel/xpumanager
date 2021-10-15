#!/bin/bash

WORK=`dirname "$0"`
export HOME=`cd ${WORK} && pwd`

function Start()
{
  cd ${HOME}
  ( nohup gunicorn --certfile=cert.pem --keyfile=key.pem --ssl-version TLSv1_2 --bind=0.0.0.0:30000 --worker-class=gevent --worker-connections=1000 -w 3 DGM:app & ) >/dev/null 2>&1
}

function Stop()
{
  mypids=$( ps ax | grep gunicorn | awk '{print $1}' )  
  if [ -n "$mypids" ]; then
 	  kill -9 $mypids
  fi
}

function Status()
{
  mypids=$( ps ax | grep gunicorn | awk '{print $1}' )  
  if [ -n "$mypids" ]; then
 	  echo "XPUM Rest service is running"
  else
    echo "XPUM Rest service is stopped"
  fi
}

case "$1" in
  start)
        Start
        ;;
  stop)
        Stop
        ;;
  restart|reload)
        Stop

        Start
        ;;
  status)
		Status
	;;
  *)
        echo $"Usage: $0 {start|stop|restart|status}"
        exit 1
esac
