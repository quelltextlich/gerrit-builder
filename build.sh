#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

DATE="$(date +'%Y-%m-%d')"

TARGET_DIR_ABS="$ARTIFACTS_NIGHTLY_DIR_ABS/$BRANCH/$DATE"

FORCE=no

if [ "$1" = "--force" ]
then
    FORCE=yes
fi

if [ -e "$TARGET_DIR_ABS" -a "$FORCE" = yes ]
then
    rm -rf "$TARGET_DIR_ABS"
fi

if [ -e "$TARGET_DIR_ABS" ]
then
    error "'$TARGET_DIR_ABS' already exists"
fi

mkdir -p "$TARGET_DIR_ABS"

info "Date: $DATE"
info "Branch: $BRANCH"

section "Updating gerrit"
cd "$GERRIT_DIR_ABS"
run_git checkout "$BRANCH"
run_git pull --recurse-submodules=yes
describe_repo


BUCK_WANTED_VERSION="$(cat "$GERRIT_DIR_ABS/.buckversion")"

if [ "$BUCK_WANTED_VERSION" != "$(run_buck --version 2>/dev/null | cut -f 3 -d ' ')" ]
then
    section "Rebuilding buck"
    pushd "$BUCK_DIR_ABS" >/dev/null
    git checkout master
    git pull
    git checkout "$BUCK_WANTED_VERSION"
    ant
    popd >/dev/null
fi

for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
do
    EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"
    section "Updating $EXTRA_PLUGIN_NAME"

    pushd "$EXTRA_PLUGIN_DIR_ABS" >/dev/null
    run_git checkout "$BRANCH"
    run_git pull
    describe_repo
    popd >/dev/null

    if [ ! -e "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME" ]
    then
        ln -s "$EXTRA_PLUGIN_DIR_ABS" "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME"
    fi
done

cat >"$TARGET_DIR_ABS/build_description.txt" <<EOF
#Project	commit
$REPO_DESCRIPTIONS
EOF

run_buck_build "gerrit" "//:withdocs" "withdocs.war"

for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
do
    EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"

    run_buck_build "$EXTRA_PLUGIN_NAME" "plugins/$EXTRA_PLUGIN_NAME:$EXTRA_PLUGIN_NAME" "plugins/$EXTRA_PLUGIN_NAME/$EXTRA_PLUGIN_NAME.jar"
done

pushd "$TARGET_DIR_ABS" >/dev/null
sha1sum *.war *.jar *.txt >sha1sums.txt
popd >/dev/null

#section "Building api"
#echo run_buck build api

finalize "$0"


