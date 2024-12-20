#!/bin/sh

# Path to the aesdsocket executable on the target system
: <<'END' AESDSOCKET_PATH="/usr/bin/aesdsocket"
PID_FILE="/var/run/aesdsocket.pid"

start() {
    echo "Starting aesdsocket..."
    start-stop-daemon --start --quiet --background --make-pidfile --pidfile $PID_FILE --exec $AESDSOCKET_PATH -- -d
    if [ $? -eq 0 ]; then
        echo "aesdsocket started."
    else
        echo "Failed to start aesdsocket."
        exit 1
    fi
}

stop() {
    echo "Stopping aesdsocket..."
    start-stop-daemon --stop --quiet --pidfile $PID_FILE --signal SIGTERM
    if [ $? -eq 0 ]; then
        echo "aesdsocket stopped."
    else
        echo "Failed to stop aesdsocket."
        exit 1
    fi
    # Clean up PID file
    if [ -f $PID_FILE ]; then
        rm -f $PID_FILE
    fi
}
END
case "$1" in
    start)
        start-stop-daemon -S -n aesdsocket -a aesdsocket -- -d
        ;;
    stop)
        start-stop-daemon -K -n aesdsocket
        ;;
    restart)
        stop
        start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0
