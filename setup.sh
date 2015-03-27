#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

setup_watchman() {
    section "Setting up Watchman"
    run_git clone https://github.com/facebook/watchman.git
    cd watchman
    ./autogen.sh
    ./configure
    make
    cd "$SCRIPT_DIR_ABS"
}

setup_buck() {
    section "Setting up Buck"
    git clone https://gerrit.googlesource.com/buck
    cd buck
    ant
    cd "$SCRIPT_DIR_ABS"
}

setup_repo() {
    local REPO="$1"
    section "Setting up $REPO"
    mkdir -p "$(dirname "$REPO")"
    cd "$(dirname "$REPO")"
    git clone "https://gerrit.googlesource.com/$REPO"
    cd "$(basename "$REPO")"
    git checkout "$BRANCH"
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
