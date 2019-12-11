#!/usr/bin/env bash

if [ $EUID -ne 0 ]; then
    echo "Please run as sudo"
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PROG="c-loopback"

function usage {
    echo "Usage: $0 [do|undo] [] <prog-args>"
    echo "  mode: 'daemon' to execute in the background"
}

function cleanup {
    cd -
} &> /dev/null

function install_daemon {
        nohup ./c-loopback $@ > /dev/null 2>&1 &
}

function install {
        ./c-loopback $@
}

function uninstall {
    for p in $(ps aux | grep "$PROG" | awk '{print $2}'); do
       kill -9 $p &> /dev/null
    done 
}

# Register cleanup tasks on exit
trap cleanup EXIT
cd $SCRIPT_DIR

if [ $# -lt 1 ]; then
    usage
    exit 1
fi

ACTION="$1"

if [ "$ACTION" == "do" ]; then
    # Compile prog if needed
    if [ ! -f "$PROG" ]; then
        gcc -o c-loopback loopback.c     
    fi
   
    # Run in daemon mode
    if [ "$2" == "daemon" ]; then
        shift 2
        install_daemon $@
    else
        # Run directly
        shift 1
        install $@
    fi
elif [ "$ACTION" == "undo" ]; then
    uninstall
else
    echo "Unknown action '$ACTION'"
    usage
fi
