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

    local REPO_DIR_RELS
    if [ "${REPO:0:8}" = "plugins/" ]
    then
        plugin_name_to_REPO_DIR_RELS "$REPO"
    else
        REPO_DIR_RELS="$REPO"
    fi

    if [ -e "$REPO_DIR_RELS" ]
    then
        cd "$REPO_DIR_RELS"
        run_git fetch origin
    else
        mkdir -p "$REPO_DIR_RELS"
        cd "$REPO_DIR_RELS"
        run_git clone --recurse-submodules "https://gerrit.googlesource.com/$REPO" .
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
