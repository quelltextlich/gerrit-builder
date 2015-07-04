#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

cd ..

for MARKER_FILE_RELC in */write_builds_overview_index_html.sh
do
    REPO_DIR_ABS="$PWD/$(dirname "$MARKER_FILE_RELC")"

    section "$(basename "$REPO_DIR_ABS")"

    pushd "$REPO_DIR_ABS" >/dev/null

    git fetch
    git reset --hard origin/master

    popd >/dev/null
done

finalize
