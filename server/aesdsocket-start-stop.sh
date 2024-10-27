#!/bin/bash

# Path to the aesdsocket binary
DAEMON_PATH="/home/dleevm/assignment-1-tale1433/server/aesdsocket"
DAEMON_NAME="aesdsocket"
PIDFILE="/var/run/${DAEMON_NAME}.pid"

case "$1" in
    start)
        echo "Starting ${DAEMON_NAME}..."
        if [ -f "$PIDFILE" ]; then
            echo "${DAEMON_NAME} is already running."
        else
            start-stop-daemon --start --background --make-pidfile --pidfile "$PIDFILE" --exec "$DAEMON_PATH" -- -d
            echo "${DAEMON_NAME} started."
        fi
        ;;
    
    stop)
        echo "Stopping ${DAEMON_NAME}..."
        if [ -f "$PIDFILE" ]; then
            start-stop-daemon --stop --pidfile "$PIDFILE" --signal SIGTERM
            rm "$PIDFILE"
            echo "${DAEMON_NAME} stopped."
        else
            echo "${DAEMON_NAME} is not running."
        fi
        ;;
    
    restart)
        echo "Restarting ${DAEMON_NAME}..."
        $0 stop
        $0 start
        ;;
    
    status)
        if [ -f "$PIDFILE" ]; then
            echo "${DAEMON_NAME} is running with PID $(cat "$PIDFILE")."
        else
            echo "${DAEMON_NAME} is not running."
        fi
        ;;
    
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac

exit 0
