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

    # Hook to ensure Change-Id
    cp "$GERRIT_DIR_ABS/gerrit-server/src/main/resources/com/google/gerrit/server/tools/root/hooks/commit-msg" .git/hooks/commit-msg
    chmod 755 .git/hooks/commit-msg

    # Hook to guard against spaces
    cp .git/hooks/pre-commit.sample .git/hooks/pre-commit
    chmod 755 .git/hooks/pre-commit

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
