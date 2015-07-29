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

repo_path_check() {
    local REPO_NAME="$1"
    local FILE="$2"

    local URL="https://gerrit.googlesource.com/$REPO_NAME/+/$BRANCH$FILE"
    if wget -o /dev/null -O /dev/null "$URL"
    then
        return 0
    fi
    return 1
}

REPO_NAMES=$(wget -O - -o /dev/null 'https://gerrit-review.googlesource.com/projects/?prefix=plugins%2F&n=101&type=CODE' \
    | tr '\n' ' ' \
    | sed -e 's/\(   "plugins\)/\n\1/g' \
    | grep '"state": "ACTIVE"' \
    | cut -f 2 -d '"' \
    | sort \
    )

while read REPO_NAME
do
    section "$REPO_NAME"

    plugin_name_to_REPO_DIR_RELS "$REPO_NAME"
    # RELS matches where we'd link it underneath gerrit, hence the
    # RELS use makes sense
    BUNDLED_CANDIDATE_DIR_ABS="$GERRIT_DIR_ABS/$REPO_DIR_RELS"
    if [ ! -d "$BUNDLED_CANDIDATE_DIR_ABS" -o -h "$BUNDLED_CANDIDATE_DIR_ABS" ]
    then
        if repo_path_check "$REPO_NAME" ""
        then
            if repo_path_check "$REPO_NAME" "/BUCK"
            then
                ./setup_repository.sh "$REPO_NAME"
            else
                info "Skipping $REPO_NAME as it misses a BUCK file on ref $BRANCH"
            fi
        else
            info "Skipping $REPO_NAME as it misses a $BRANCH ref"
        fi
    else
        info "Skipping $REPO_NAME as it is a bundeled plugin"
    fi
done <<<"$REPO_NAMES"

finalize