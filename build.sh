#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

DATE="$(date +'%Y-%m-%d')"

FORCE=no
PULL=yes
CHECKOUT=yes

print_help() {
    cat <<EOF
$0 ARGUMENTS

ARGUMENTS:
  --help             - prints this page
  --branch BRANCH    - Build branch BRANCH instead of master
  --force            - Overwrite eventual existing artifacts target directory
  --no-checkout      - Don't 'git checkout' before building
  --no-pull          - Don't 'git pull' before building
  --no-repo-mangling - Neither 'git checkout' nor 'git pull' before building
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
        "--force" )
            FORCE=yes
            ;;
        "--branch" )
            [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
            BRANCH="$1"
            shift || true
            ;;
        "--no-checkout" )
            CHECKOUT=no
            ;;
        "--no-pull" )
            PULL=no
            ;;
        "--no-repo-mangling" )
            CHECKOUT=no
            PULL=no
            ;;
        * )
            error "Unknown argument '$ARGUMENT'"
            ;;
    esac
done

TARGET_DIR_ABS="$ARTIFACTS_NIGHTLY_DIR_ABS/$BRANCH/$DATE"

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
if [ "$CHECKOUT" = "yes" ]
then
    run_git checkout "$BRANCH"
fi
if [ "$PULL" = "yes" ]
then
    run_git pull --recurse-submodules=yes
fi
describe_repo "withdocs.war"


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
    if [ "$CHECKOUT" = "yes" ]
    then
        run_git checkout "$BRANCH"
    fi
    if [ "$PULL" = "yes" ]
    then
        run_git pull
    fi
    describe_repo "$EXTRA_PLUGIN_NAME.jar"
    popd >/dev/null

    if [ -h "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME" ]
    then
        rm "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME"
    fi

    if [ ! -e "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME" ]
    then
        ln -s "$EXTRA_PLUGIN_DIR_ABS" "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME"
    fi
done

cat >"$TARGET_DIR_ABS/build_description.json" <<EOF
{
$REPO_DESCRIPTIONS
}
EOF

run_buck_build "gerrit" "//:withdocs" "withdocs.war"

for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
do
    EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"

    run_buck_build "$EXTRA_PLUGIN_NAME" "plugins/$EXTRA_PLUGIN_NAME:$EXTRA_PLUGIN_NAME" "plugins/$EXTRA_PLUGIN_NAME/$EXTRA_PLUGIN_NAME.jar"
done

pushd "$TARGET_DIR_ABS" >/dev/null
sha1sum * >sha1sums.txt
popd >/dev/null

#section "Building api"
#echo run_buck build api

finalize
