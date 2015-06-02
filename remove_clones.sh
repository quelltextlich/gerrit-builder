#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

remove_clone() {
    REPO="$1"
    section "Removing $CLONE_DIR_RELS"
    local REPO_DIR_RELS
    plugin_name_to_REPO_DIR_RELS "$REPO"
    rm -rf "$REPO_DIR_RELS"
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