#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

DISPATCH_DIR_ABS='/var/www/builds.quelltextlich.at/'

print_help() {
    cat <<EOF
$0 ARGUMENTS

Dispatches index*.html files from the current directory onto a public
directory

ARGUMENTS:
  --dispatch-dir DISPATCH_DIR_ABS
             -- The absolute directory to dispatch the files to
                E.g.: /var/www/builds.quelltextlich.at
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
        "--dispatch-dir" )
            [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
            DISPATCH_DIR_ABS="$1"
            shift || true
            ;;
        * )
            error "Unknown argument '$ARGUMENT'"
            ;;
    esac
done

if [ ! -d "$DISPATCH_DIR_ABS" ]
then
    error "The dispatch directory '$DISPATCH_DIR_ABS' does not exist"
fi

for SOURCE_FILE_RELS in index*.html
do
    section "Dispatching '$SOURCE_FILE_RELS'"
    TARGET_FILE_RELD="$SOURCE_FILE_RELS"
    TARGET_FILE_RELD="${TARGET_FILE_RELD%.html}"
    TARGET_FILE_RELD="${TARGET_FILE_RELD#index}"
    TARGET_FILE_RELD="${TARGET_FILE_RELD//_//}"
    TARGET_FILE_RELD="$TARGET_FILE_RELD/$INDEX_FILE_RELC"
    TARGET_FILE_ABS="$DISPATCH_DIR_ABS/$TARGET_FILE_RELD"
    info "$SOURCE_FILE_RELS -> $TARGET_FILE_ABS"
    mv "$SOURCE_FILE_RELS" "$TARGET_FILE_ABS"
done

finalize
