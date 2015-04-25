#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

if [ -e "$TEST_SITE_GERRIT_RUN_FILE_ABS" ]
then
    RUN_ID="$(cat "$TEST_SITE_GERRIT_RUN_FILE_ABS")"
    checked_kill "java" "--run-id $RUN_ID"
fi

finalize
