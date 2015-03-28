#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

setup_watchman() {
    section "Setting up Watchman"
    if [ ! -d "watchman" ]
    then
        run_git clone https://github.com/facebook/watchman.git
        cd watchman
        ./autogen.sh
        ./configure
        make
        cd "$SCRIPT_DIR_ABS"
    fi
}

setup_buck() {
    section "Setting up Buck"
    if [ ! -d "buck" ]
    then
        run_git clone https://gerrit.googlesource.com/buck
        cd buck
        ant
        cd "$SCRIPT_DIR_ABS"
    fi
}

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

    cd "$SCRIPT_DIR_ABS"
}


setup_watchman
setup_buck
for REPO in \
    gerrit \
    plugins/its-base \
    plugins/its-bugzilla \
    plugins/its-jira \
    plugins/its-phabricator \
    plugins/its-rtc \
    plugins/its-storyboard \

do
    setup_repo "$REPO"
done

# Installing hooks
for REPO_DIR_ABS in \
    "$GERRIT_DIR_ABS"* \
    "$GERRIT_DIR_ABS/plugins/"* \
    "$EXTRA_PLUGINS_DIR_ABS/"* \

do
    if [ -d "$REPO_DIR_ABS" ]
    then
        echo "$REPO_DIR_ABS"
        pushd "$REPO_DIR_ABS" >/dev/null
        setup_git_hooks
        popd >/dev/null
    fi
done