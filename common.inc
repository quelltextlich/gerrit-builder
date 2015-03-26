#!/bin/bash
set -e

#---------------------------------------------------------------------
ORIG_DIR_ABS="$PWD"
cd "$(dirname "$0" )"

SCRIPT_DIR_ABS="$PWD"
SCRIPT_ARGUMENTS=( "$@" )
#---------------------------------------------------------------------

IMAGE_BASE_URL="http://builds.quelltextlich.at/images"

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
BRANCH="$(basename "$SCRIPT_DIR_ABS" | cut -f 1,2 -d - | cut -f 1 -d _ )"
if [[ "$BRANCH" != "master" && ! ( "$BRANCH" =~ stable-[0-9]+.[0-9]+ ) ]]
then
    BRANCH="master"
fi

ARTIFACTS_TOTAL=0
ARTIFACTS_OK=0
ARTIFACTS_FAILED=0

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
    local REPO_NAME_PAD="${BLANK:0:$((30-${#REPO_NAME}))}"
    local REPO_DESCRIPTION_PAD="${BLANK:0:$((30-${#REPO_DESCRIPTION}))}"

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
    local STATUS="failed"

    local SOURCE_FILE_ABS="$GERRIT_DIR_ABS/buck-out/gen/$BUCK_GENERATED_FILE_RELBG"
    local TARGET_FILE_ABS="$TARGET_FILE_DIR_ABS/$(basename "$BUCK_GENERATED_FILE_RELBG")"

    local LOG_FILE_ABS="$GERRIT_DIR_ABS/buck-out/log/buck-0.log"

    section "Building $DESCRIPTION"

    if "$BUCK_DIR_ABS/bin/buck" build "$BUCK_TARGET"
    then
        if [ -e "$SOURCE_FILE_ABS" ]
        then
            cp "$SOURCE_FILE_ABS" "$TARGET_FILE_ABS"
            STATUS="ok"
            BUCK_BUILD_FAILED=no
        else
            error "Could not find artifact $SOURCE_FILE_ABS for $DESCRIPTION"
        fi
    fi

    if [ -e "$LOG_FILE_ABS" ]
    then
        cp "$LOG_FILE_ABS" "$TARGET_FILE_ABS.buck_log.txt"
    else
        error "Could not build $DESCRIPTION, but log file '$LOG_FILE_ABS' does not exist either"
    fi

    echo_file_target_html "$STATUS" "$(basename "$TARGET_FILE_ABS")" "$BUCK_TARGET"
}

#---------------------------------------------------------------------
echo_file_target_html() {
    local STATUS="$1"
    local ARTIFACT_FILE_RELF="$2"
    local BUCK_TARGET="$3"

    local SIZE_CELL="---"
    local ARTIFACT_CELL=
    if [ -e "$TARGET_DIR_ABS/$FILES_DIR_RELT/$ARTIFACT_FILE_RELF" ]
    then
        ARTIFACT_CELL="<a href=\"$FILES_DIR_RELT/$ARTIFACT_FILE_RELF\">$ARTIFACT_FILE_RELF</a>"
        local SIZE="$(stat -c%s "$TARGET_DIR_ABS/$FILES_DIR_RELT/$ARTIFACT_FILE_RELF")"
        if [ -n "$SIZE" ]
        then
            if [ "$SIZE" -lt 2000000 ]
            then
                SIZE_CELL="$((SIZE / 1000 + 1)) KB"
            else
                SIZE_CELL="$((SIZE / 1000 / 1000 + 1)) MB"
            fi
        fi
    else
        ARTIFACT_CELL="$ARTIFACT_FILE_RELF"
    fi

    local BUCK_LOG_CELL="---"
    local REPO_CELL="---"
    local COMMIT_CELL="---"
    if [ -n "$BUCK_TARGET" ]
    then
        BUCK_LOG_CELL="<a href=\"$FILES_DIR_RELT/$ARTIFACT_FILE_RELF.buck_log.txt\">View buck log</a>"

        local REPO="$(cut -f 1 -d : <<<"$BUCK_TARGET")"
        if [ "$REPO" = "//" ]
        then
            REPO="gerrit"
        fi
        pushd "$SCRIPT_DIR_ABS/$REPO" >/dev/null
        local COMMIT_HASH="$(git rev-parse HEAD)"
        popd  >/dev/null
        REPO_CELL="<a href=\"https://gerrit-review.googlesource.com/#/admin/projects/$REPO\">$REPO</a>"
        COMMIT_CELL="<a href=\"https://gerrit.googlesource.com/$REPO/+/$COMMIT_HASH\"><code>${COMMIT_HASH:0:8}</code></a>"
    fi

    cat_target_html <<EOF

  <tr class="$STATUS">
    <td><img src="$IMAGE_BASE_URL/$STATUS.png" alt="Build $STATUS" /> $STATUS</td>
    <th class="left th-semi-dark th-$STATUS">$ARTIFACT_CELL</th>
    <td class="right">$SIZE_CELL</td>
    <td>$BUCK_LOG_CELL</td>
    <td>$REPO_CELL</td>
    <td>$COMMIT_CELL</td>
  </tr>
EOF

    ARTIFACTS_TOTAL=$((ARTIFACTS_TOTAL+1))
    if [ "$STATUS" = "ok" ]
    then
        ARTIFACTS_OK=$((ARTIFACTS_OK+1))
    else
        ARTIFACTS_FAILED=$((ARTIFACTS_FAILED+1))
    fi
}

#---------------------------------------------------------------------
echo_target_html() {
    echo "$@" | cat_target_html  >>"$TARGET_DIR_ABS/index.html"
}

#---------------------------------------------------------------------
cat_target_html() {
    cat >>"$TARGET_DIR_ABS/index.html"
}
