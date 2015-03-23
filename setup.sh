#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

section "Setting up Watchman"
run_git clone https://github.com/facebook/watchman.git
cd watchman
./autogen.sh
./configure
make
cd "$SCRIPT_DIR_ABS"


section "Setting up Buck"
git clone https://gerrit.googlesource.com/buck
cd buck
ant
cd "$SCRIPT_DIR_ABS"

for REPO in \
    gerrit \
    plugins/its-base \
    plugins/its-bugzilla \
    plugins/its-jira \
    plugins/its-phabricator \
    plugins/its-rtc \
    plugins/its-storyboard \

do
    section "Setting up $REPO"
    mkdir -p "$(dirname "$REPO")"
    cd "$(dirname "$REPO")"
    git clone "https://gerrit.googlesource.com/$REPO"
    cd "$(basename "$REPO")"
    git checkout "$BRANCH"
    cd "$SCRIPT_DIR_ABS"
done
