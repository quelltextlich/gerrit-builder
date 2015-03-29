#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

cd ..

for LEGAL_URL_FILE_RELC in */legal.url
do
    REPO_DIR_ABS="$PWD/$(dirname "$LEGAL_URL_FILE_RELC")"

    section "$(basename "$REPO_DIR_ABS")"

    pushd "$REPO_DIR_ABS" >/dev/null

    git fetch
    git reset --hard origin/master

    popd >/dev/null
done

finalize
