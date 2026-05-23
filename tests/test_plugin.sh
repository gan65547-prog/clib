#!/bin/sh
# test_plugin.sh — Minimal plugin for mlfw plugin lifecycle testing.
# Supports: start, stop, restart, reset
#
# Usage: sh test_plugin.sh <command> <workspace> <tempspace>

PIDFILE=""

case "$1" in
    start)
        echo "test_plugin: start on $2 (temp=$3)"
        ;;
    stop)
        echo "test_plugin: stop"
        ;;
    restart)
        echo "test_plugin: restart on $2 (temp=$3)"
        ;;
    reset)
        echo "test_plugin: reset on $2"
        ;;
    *)
        echo "test_plugin: unknown command '$1'"
        exit 1
        ;;
esac
exit 0
