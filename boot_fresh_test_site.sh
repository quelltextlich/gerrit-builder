#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

WAR_FILE_ABS="$1"
RUN_ID="$2"

if [ -z "$WAR_FILE_ABS" ]
then
    error "No war file given"
fi

if [ "${WAR_FILE_ABS:0:1}" != "/" ]
then
    WAR_FILE_ABS="$ORIG_DIR_ABS/$WAR_FILE_ABS"
fi

if [ ! -e "$WAR_FILE_ABS" ]
then
    error "War file '$WAR_FILE_ABS' does not exist"
fi

if [ -z "$RUN_ID" ]
then
    RUN_ID="$(date +%s).$$"
fi

run_war() {
    local COMMAND="$1"
    shift
    LC_ALL=C java -jar "$WAR_FILE_ABS" "$COMMAND" -d "$TEST_SITE_DIR_ABS" "$@"
}

declare -A TEST_SITE_SESSIONS=()

setup_ssh_user() {
    local USER_NAME="$1"
    curl_as "$USER_NAME" --header 'Accept: application/json' --data @"$TEST_SITE_USER_CREDENTIALS_DIR_ABS/$USER_NAME/id_rsa.pub" "$TEST_SITE_URL/a/accounts/self/sshkeys"
}

"$SCRIPT_DIR_ABS"/stop_test_site.sh

rm -rf "$TEST_SITE_DIR_ABS"

section "Setting up vanilla test site"
run_war init --skip-plugins --no-auto-start --batch

section "Adapting config"
set_config_value "gerrit.canonicalWebUrl" "$TEST_SITE_URL"
set_config_value "auth.type" "DEVELOPMENT_BECOME_ANY_ACCOUNT"
set_config_value "sendemail.enable" "false"
set_config_value "sshd.listenAddress" "127.0.0.1:$TEST_SITE_SSH_PORT"
set_config_value "httpd.listenUrl" "$TEST_SITE_URL"
set_config_value "plugins.allowRemoteAdmin" "true"

section "Init with adapted config"
run_war init --skip-plugins --no-auto-start --batch

section "Re-indexing"
run_war reindex

section "Starting gerrit"
info "Run id: $RUN_ID"
rm -f "$TEST_SITE_GERRIT_RUN_FILE_ABS"
run_war daemon --run-id "$RUN_ID" </dev/null &>/dev/null &
GERRIT_PID="$!"

ITERATION_COUNT=0
MAX_ITERATION_COUNT=150
GERRIT_IS_READY=no

while [ -e "/proc/$GERRIT_PID" \
    -a $ITERATION_COUNT -lt $MAX_ITERATION_COUNT \
    -a ! -e "$TEST_SITE_GERRIT_RUN_FILE_ABS" ]
do
    sleep 0.2s
    ITERATION_COUNT=$((ITERATION_COUNT+1))
done

if [ ! -e "$TEST_SITE_GERRIT_RUN_FILE_ABS" ]
then
    error "gerrit did not properly start within $((MAX_ITERATION_COUNT/5)) seconds"
fi

if [ ! -e "/proc/$GERRIT_PID" ]
then
    error "gerrit died away"
fi

info "gerrit is up"

section "Setting up admin user"

cp -a "$USER_CREDENTIALS_DIR_ABS" "$TEST_SITE_USER_CREDENTIALS_DIR_ABS"
setup_ssh_user admin

finalize
