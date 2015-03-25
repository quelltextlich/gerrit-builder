#!/bin/bash
set -e

#---------------------------------------------------------------------
ORIG_DIR_ABS="$PWD"
cd "$(dirname "$0" )"

SCRIPT_DIR_ABS="$PWD"
SCRIPT_ARGUMENTS=( "$@" )
#---------------------------------------------------------------------

ANT_DIR_ABS="$SCRIPT_DIR_ABS/apache-ant"
BUCK_DIR_ABS="$SCRIPT_DIR_ABS/buck"
GERRIT_DIR_ABS="$SCRIPT_DIR_ABS/gerrit"
JAVA_DIR_ABS="$SCRIPT_DIR_ABS/jdk"
WATCHMAN_DIR_ABS="$SCRIPT_DIR_ABS/watchman"
MAVEN_DIR_ABS="$SCRIPT_DIR_ABS/apache-maven"

EXTRA_PLUGINS_DIR_ABS="$SCRIPT_DIR_ABS/plugins"

ARTIFACTS_DIR_ABS="$SCRIPT_DIR_ABS/artifacts"
ARTIFACTS_NIGHTLY_DIR_ABS="$ARTIFACTS_DIR_ABS/nightly"

ANT_HOME="$ANT_DIR_ABS"
JAVA_HOME="$JAVA_DIR_ABS"

PATH="$PATH:$JAVA_DIR_ABS/bin"
PATH="$PATH:$ANT_DIR_ABS/bin"
PATH="$PATH:$BUCK_DIR_ABS/bin"
PATH="$PATH:$WATCHMAN_DIR_ABS"
PATH="$PATH:$MAVEN_DIR_ABS/bin"

#---------------------------------------------------------------------
BRANCH=master




#---------------------------------------------------------------------
mkdir -p "$ARTIFACTS_DIR_ABS"
mkdir -p "$ARTIFACTS_NIGHTLY_DIR_ABS"

#---------------------------------------------------------------------
log() {
    echo " * $(date --utc +'%Y-%m-%d %H:%M:%S.%3N')" "$@" >&2
}

#---------------------------------------------------------------------
error() {
    log "ERR :" "$@"
    exit 1
}

#---------------------------------------------------------------------
info() {
    log "INFO:" "$@"
}

#---------------------------------------------------------------------
finalize() {
    info "pass." "Arguments:" "${SCRIPT_ARGUMENTS[@]}"
}

#---------------------------------------------------------------------
section() {
    local MSG="$1"
    local MSG_LENGTH="${#MSG}"
    local DASHES="----------"
    DASHES="$DASHES$DASHES"
    DASHES="$DASHES$DASHES"
    DASHES="$DASHES$DASHES"
    info "--" "$MSG" "${DASHES:0:$((44-MSG_LENGTH))}"
}

#---------------------------------------------------------------------
run_git() {
    git "$@"
}

#---------------------------------------------------------------------
describe_repo() {
    local ARTIFACT="$1"
    local REPO_DESCRIPTION="$(run_git describe --dirty --long 2>/dev/null || true)"
    if [ -z "$REPO_DESCRIPTION" ]
    then
        REPO_DESCRIPTION="$(run_git describe --dirty --long --all 2>/dev/null || true)"
    fi
    if [ -z "$REPO_DESCRIPTION" ]
    then
        error "Could not describe $(pwd)"
    fi

    if [ -n "$REPO_DESCRIPTIONS" ]
    then
        REPO_DESCRIPTIONS="$REPO_DESCRIPTIONS,
"
    fi

    local BLANK="          "
    BLANK="$BLANK$BLANK$BLANK$BLANK$BLANK$BLANK$BLANK$BLANK"

    local REPO_NAME="$(git config --get 'remote.origin.url' | cut -f 4- -d /)"
    local REPO_NAME_PAD="${BLANK:0:$((24-${#REPO_NAME}))}"
    local REPO_DESCRIPTION_PAD="${BLANK:0:$((24-${#REPO_DESCRIPTION}))}"

    local ARTIFACT_PAD="${BLANK:0:$((20-${#ARTIFACT}))}"
    REPO_DESCRIPTIONS="$REPO_DESCRIPTIONS  \"$REPO_NAME\": $REPO_NAME_PAD { \"commit\": \"$REPO_DESCRIPTION\",$REPO_DESCRIPTION_PAD \"expected_artifacts\": [ \"$ARTIFACT\" ]$ARTIFACT_PAD}"
}

#---------------------------------------------------------------------
run_buck() {
    "$BUCK_DIR_ABS/bin/buck" "$@"
}

#---------------------------------------------------------------------
run_buck_build() {
    local DESCRIPTION="$1"
    local BUCK_TARGET="$2"
    local BUCK_GENERATED_FILE_RELBG="$3"

    local SOURCE_FILE_ABS="$GERRIT_DIR_ABS/buck-out/gen/$BUCK_GENERATED_FILE_RELBG"
    local TARGET_FILE_ABS="$TARGET_FILE_DIR_ABS/$(basename "$BUCK_GENERATED_FILE_RELBG")"

    local LOG_FILE_ABS="$GERRIT_DIR_ABS/buck-out/log/buck-0.log"

    section "Building $DESCRIPTION"
    if "$BUCK_DIR_ABS/bin/buck" build "$BUCK_TARGET"
    then
        if [ -e "$SOURCE_FILE_ABS" ]
        then
            cp "$SOURCE_FILE_ABS" "$TARGET_FILE_ABS"
        else
            error "Could not find artifact $SOURCE_FILE_ABS for $DESCRIPTION"
        fi
    else
        if [ -e "$LOG_FILE_ABS" ]
        then
            cp "$LOG_FILE_ABS" "$TARGET_FILE_ABS.error_log.txt"
        else
            error "Could not build $DESCRIPTION, but log file '$LOG_FILE_ABS' does not exist either"
        fi
    fi
}
