#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

checked_kill() {
    local PID="$1"
    shift

    if [ -e "/proc/$PID" ]
    then
        kill "$@" "$PID"
        COUNT=0
        while [ -e "/proc/$PID" -a "$COUNT -lt 10" ]
        do
            sleep 1
            COUNT=$((COUNT+1))
        done
    fi
}

if [ -e "$TEST_SITE_GERRIT_RUN_FILE_ABS" ]
then
    RUN_ID="$(cat "$TEST_SITE_GERRIT_RUN_FILE_ABS")"
    GERRIT_PID=$(ps -C java -o pid,cmd | grep -e "--run-id $RUN_ID" | sed -e 's/^[[:space:]]*\([0-9]\+\)[[:space:]].*/\1/')

    if [ -n "$GERRIT_PID" ]
    then
        checked_kill "$GERRIT_PID"
        checked_kill "$GERRIT_PID"
        checked_kill "$GERRIT_PID" "-KILL"

        if [ -e "/proc/$GERRIT_PID" ]
        then
            error "Gerrit pid $GERRIT_PID still alive"
        fi
    fi
fi

finalize
