#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

REPO="$1"

if [ "$#" != "1" ]
then
    error "$0 expects exacty 1 argument, which is the repo to setup (like 'gerrit', or 'plugins/its-base')"
fi

setup_repo() {
    local REPO="$1"
    section "Setting up $REPO"
    mkdir -p "$(dirname "$REPO")"
    cd "$(dirname "$REPO")"
    if [ -e "$(basename "$REPO")" ]
    then
        cd "$(basename "$REPO")"
        run_git fetch origin
    else
        run_git clone --recurse-submodules "https://gerrit.googlesource.com/$REPO"
        cd "$(basename "$REPO")"
    fi
    run_git checkout "$BRANCH"

    if [ -e ".gitmodules" ]
    then
        run_git submodule update
    fi

    setup_git_hooks

    cd "$SCRIPT_DIR_ABS"
}

setup_repo "$REPO"
