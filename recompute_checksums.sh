#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

print_help() {
    cat <<EOF
$0 ARGUMENTS TARGET_DIR

ARGUMENTS:
  --help             - prints this page

TARGET_DIR is the directory to compute the checksums for.
EOF
}

parse_arguments() {
    while [ $# -gt 0 ]
    do
        local ARGUMENT="$1"
        shift
        case "$ARGUMENT" in
            "--help" | "-h" | "-?" )
                print_help
                exit 0
                ;;
            * )
                [ $# -eq 0 ] || error "Unknown argument '$ARGUMENT'"
                TARGET_DIR_ABS="$ARGUMENT"
                ;;
        esac
    done
}

TARGET_DIR_ABS=
parse_arguments "$@"

if [ -z "$TARGET_DIR_ABS" ]
then
    error "No TARGET_DIR given"
fi


TARGET_DIR_ABS="${TARGET_DIR_ABS%/sha1sums.txt}"
if [ "${TARGET_DIR_ABS:0:1}" != "/" ]
then
    # TARGET_DIR_ABS is not yet absolute
    if [ -e "$ORIG_DIR_ABS/$TARGET_DIR_ABS" ]
    then
        TARGET_DIR_ABS="$ORIG_DIR_ABS/$TARGET_DIR_ABS"
    elif [ -e "$BUILDS_DIR_ABS/$TARGET_DIR_ABS" ]
    then
        TARGET_DIR_ABS="$BUILDS_DIR_ABS/$TARGET_DIR_ABS"
    elif [ -e "$PWD/$TARGET_DIR_ABS" ]
    then
        TARGET_DIR_ABS="$PWD/$TARGET_DIR_ABS"
    else
        error "Could not find existing target directory '$TARGET_DIR'. \
Please provide the absolute path to it"
    fi
fi

if [ ! -e "$TARGET_DIR_ABS" ]
then
    error "Target $TARGET_DIR_ABS does not exist"
fi

if [ ! -d "$TARGET_DIR_ABS" ]
then
    error "Target $TARGET_DIR_ABS is not a directory"
fi

compute_checksums

finalize 0