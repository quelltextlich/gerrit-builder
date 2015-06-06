#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

print_help() {
    cat <<EOF
$0 ARGUMENTS

ARGUMENTS:
  --help             - prints this page
EOF
}

while [ $# -gt 0 ]
do
    ARGUMENT="$1"
    shift
    case "$ARGUMENT" in
        "--help" | "-h" | "-?" )
            print_help
            exit 0
            ;;
        * )
            error "Unknown argument '$ARGUMENT'"
            ;;
    esac
done

link_plugins() {
    for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
    do
        EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"

        section "$EXTRA_PLUGIN_NAME"

        ln -f -s "$EXTRA_PLUGIN_DIR_ABS" "$GERRIT_DIR_ABS/plugins"
    done
}

link_plugins

finalize