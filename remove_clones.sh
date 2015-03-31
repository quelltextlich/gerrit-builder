#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

remove_clone() {
    CLONE_DIR_RELS="$1"
    section "Removing $CLONE_DIR_RELS"
    rm -rf "$CLONE_DIR_RELS"
}


for REPO in \
    "watchman" \
    "buck" \
    "gerrit" \
    "${DEFAULT_EXTERNAL_PLUGINS[@]/#/plugins/}" \

do
    remove_clone "$REPO"
done

finalize