if [ -f ./tmp/supervisord.pid ]; then
    PID=$(cat ./tmp/supervisord.pid)
    if ps -p $PID > /dev/null
    then
        echo "xpum is already running"
        exit 0
    fi
fi

source ./xpumvenv/bin/activate

supervisord -c supervisord.conf

deactivate