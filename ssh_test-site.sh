#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

USER_NAME="$1"
shift

ssh \
    -i "$TEST_SITE_USER_CREDENTIALS_DIR_ABS/$USER_NAME/id_rsa" \
    -o "UserKnownHostsFile $TEST_SITE_USER_CREDENTIALS_DIR_ABS/$USER_NAME/known_hosts" \
    -o "StrictHostKeyChecking no" \
    -p "$TEST_SITE_SSH_PORT" \
    "$USER_NAME@$TEST_SITE_HOST" \
    "$@"

finalize
