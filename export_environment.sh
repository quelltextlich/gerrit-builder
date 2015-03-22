#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

ENVIRONMENT_FILE_ABS="$SCRIPT_DIR_ABS/environment.inc"

echo "# $(date)" >"$ENVIRONMENT_FILE_ABS"

for VAR in PATH JAVA_HOME ANT_HOME
do
    eval echo "export $VAR=\"\$$VAR\"" >>"$ENVIRONMENT_FILE_ABS"
done

echo "source $ENVIRONMENT_FILE_ABS"
