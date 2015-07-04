#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------


print_help() {
    cat <<EOF
$0 ARGUMENTS

ARGUMENTS:
  --help             - prints this page
  --branch BRANCH    - Build branch BRANCH instead of the default, which is
                       inferred from the basename of the directory, with
                       "master" as fallback.
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
        "--branch" )
            [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
            BRANCH="$1"
            shift || true
            ;;
        * )
            error "Unknown argument '$ARGUMENT'"
            ;;
    esac
done

ASCENDING_FILE_RELB="index_asc.html"
DESCENDING_FILE_RELB="$INDEX_FILE_RELC"
ASCENDING_FILE_ABS="$BUILDS_DIR_ABS/$ASCENDING_FILE_RELB"
DESCENDING_FILE_ABS="$BUILDS_DIR_ABS/$DESCENDING_FILE_RELB"

# Ascending variant
"$SCRIPT_DIR_ABS"/write_builds_overview_index_html.sh \
    --build-sort-link "$DESCENDING_FILE_RELB" \
    --output "$ASCENDING_FILE_ABS"

# Descending variant
"$SCRIPT_DIR_ABS"/write_builds_overview_index_html.sh \
    --reverse \
    --build-sort-link "$ASCENDING_FILE_RELB" \
    --output "$DESCENDING_FILE_ABS"

finalize
