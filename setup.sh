#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

print_help() {
    cat <<EOF
$0 ARGUMENTS

ARGUMENTS:
  --help             - prints this page
  --ignore-plugin PLUGIN
                     - Don't setup the plugin PLUGIN
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
        "--ignore-plugin" )
            [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
            IGNORED_PLUGINS=( "${IGNORED_PLUGINS[@]}" "$1" )
            shift || true
            ;;
        * )
            error "Unknown argument '$ARGUMENT'"
            ;;
    esac
done

setup_watchman() {
    section "Setting up Watchman"
    if [ ! -d "watchman" ]
    then
        run_git clone https://github.com/facebook/watchman.git
        cd watchman
        ./autogen.sh
        ./configure --prefix="$(pwd)"
        mkdir -p "var/run/watchman"
        make
        cd "$SCRIPT_DIR_ABS"
    fi
}

setup_buck() {
    section "Setting up Buck"
    if [ ! -d "buck" ]
    then
        run_git clone https://gerrit.googlesource.com/buck
        cd buck
        ant
        cd "$SCRIPT_DIR_ABS"
    fi
}

setup_watchman
setup_buck
for REPO in \
    gerrit \
    "${DEFAULT_EXTERNAL_PLUGINS[@]}"
do
    if ! is_ignored_plugin "$REPO"
    then
        if [ "$REPO" != "gerrit" ]
        then
            REPO="plugins/$REPO"
        fi
        ./setup_repository.sh "$REPO"
    fi
done

# Installing hooks
for REPO_DIR_ABS in \
    "$GERRIT_DIR_ABS"* \
    "$GERRIT_DIR_ABS/plugins/"* \
    "$EXTRA_PLUGINS_DIR_ABS/"* \

do
    if [ -d "$REPO_DIR_ABS" ]
    then
        echo "$REPO_DIR_ABS"
        pushd "$REPO_DIR_ABS" >/dev/null
        setup_git_hooks
        popd >/dev/null
    fi
done