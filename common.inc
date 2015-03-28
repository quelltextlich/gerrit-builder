#!/bin/bash
set -e
set -o pipefail

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

TEST_OUTPUT_SUFFIX=".test_output.txt"
#---------------------------------------------------------------------
BUILD_ARTIFACTS=yes

#---------------------------------------------------------------------
BRANCH="$(basename "$SCRIPT_DIR_ABS" | cut -f 1,2 -d - | cut -f 1 -d _ )"
if [[ "$BRANCH" != "master" && ! ( "$BRANCH" =~ stable-[0-9]+.[0-9]+ ) ]]
then
    BRANCH="master"
fi

ARTIFACTS_TOTAL=0
ARTIFACTS_OK=0
ARTIFACTS_BROKEN=0
ARTIFACTS_FAILED=0

REPO_NAMES=()
LAST_REPO_NAME=
declare -A REPO_DESCRIPTIONS=()
declare -A REPO_ARTIFACTS=()

declare -A TARGET_TEST_LABELS=()
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
    local CURRENT_REPO_NAME="$(run_git config --get 'remote.origin.url' | cut -f 4- -d /)"

    local REPO_NAME=
    local FOUND_REPO=no
    for REPO_NAME in "${REPO_NAMES[@]}"
    do
        if [ "$CURRENT_REPO_NAME" = "$REPO_NAME" ]
        then
            FOUND_REPO=yes
        fi
    done
    if [ "$FOUND_REPO" != "yes" ]
    then
        REPO_NAMES=( "${REPO_NAMES[@]}" "$CURRENT_REPO_NAME" )
        LAST_REPO_NAME="$CURRENT_REPO_NAME"
    fi

    local REPO_DESCRIPTION="$(run_git describe --dirty --long 2>/dev/null || true)"
    if [ -z "$REPO_DESCRIPTION" ]
    then
        REPO_DESCRIPTION="$(run_git describe --dirty --long --all 2>/dev/null || true)"
    fi
    if [ -z "$REPO_DESCRIPTION" ]
    then
        error "Could not describe $(pwd)"
    fi

    REPO_DESCRIPTIONS["$CURRENT_REPO_NAME"]="$REPO_DESCRIPTION"
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
    local TARGET_FILE_ABS="$TARGET_DIR_ABS/$(basename "$BUCK_GENERATED_FILE_RELBG")"

    local LOG_FILE_ABS="$GERRIT_DIR_ABS/buck-out/log/buck-0.log"

    section "Building $DESCRIPTION"

    if test "$BUILD_ARTIFACTS" = "no" || "$BUCK_DIR_ABS/bin/buck" build "$BUCK_TARGET"
    then
        if [ -e "$SOURCE_FILE_ABS" ]
        then
            cp "$SOURCE_FILE_ABS" "$TARGET_FILE_ABS"
            STATUS="ok"
            BUCK_BUILD_FAILED=no
        else
            if [ "$BUILD_ARTIFACTS" != "no" ]
            then
                error "Could not find artifact $SOURCE_FILE_ABS for $DESCRIPTION"
            fi
        fi
    fi

    if [ -e "$LOG_FILE_ABS" ]
    then
        cp "$LOG_FILE_ABS" "$TARGET_FILE_ABS.buck_log.txt"
    else
        if [ "$BUILD_ARTIFACTS" != "no" ]
        then
            error "Could not build $DESCRIPTION, but log file '$LOG_FILE_ABS' does not exist either"
        fi
    fi

    if [ "$STATUS" = "ok" -a "$TEST" = "yes" -a -n "${TARGET_TEST_LABELS["$BUCK_TARGET"]}" ]
    then
        if ! run_buck test --include "${TARGET_TEST_LABELS["$BUCK_TARGET"]}" 2>&1 | tee "$TARGET_FILE_ABS$TEST_OUTPUT_SUFFIX"
        then
            STATUS="broken"
        fi
    fi

    echo_file_target_html "$STATUS" "$(basename "$TARGET_FILE_ABS")" "$BUCK_TARGET"
}

#---------------------------------------------------------------------
echo_file_target_html() {
    local STATUS="$1"
    local ARTIFACT_FILE_RELT="$2"
    local BUCK_TARGET="$3"

    local SIZE_CELL="---"
    local ARTIFACT_CELL=
    if [ -e "$TARGET_DIR_ABS/$ARTIFACT_FILE_RELT" ]
    then
        ARTIFACT_CELL="<a href=\"$ARTIFACT_FILE_RELT\">$ARTIFACT_FILE_RELT</a>"
        local SIZE="$(stat -c%s "$TARGET_DIR_ABS/$ARTIFACT_FILE_RELT")"
        if [ -n "$SIZE" ]
        then
            if [ "$SIZE" -lt 2000000 ]
            then
                SIZE_CELL="$((SIZE / 1000 + 1))&#160;KB"
            else
                SIZE_CELL="$((SIZE / 1000 / 1000 + 1))&#160;MB"
            fi
        fi
    else
        ARTIFACT_CELL="$ARTIFACT_FILE_RELT"
    fi

    local BUCK_LOG_CELL="---"
    local REPO_CELL="---"
    local DESCRIPTION_CELL="---"
    local COMMIT_CELL="---"
    if [ -n "$BUCK_TARGET" ]
    then
        BUCK_LOG_CELL="<a href=\"$ARTIFACT_FILE_RELT.buck_log.txt\">build</a>"
        if [ -e "$TARGET_DIR_ABS/$ARTIFACT_FILE_RELT$TEST_OUTPUT_SUFFIX" ]
        then
            BUCK_LOG_CELL="$BUCK_LOG_CELL,&#160;<a href=\"$ARTIFACT_FILE_RELT$TEST_OUTPUT_SUFFIX\">test</a>"
        fi

        local REPO="$(cut -f 1 -d : <<<"$BUCK_TARGET")"
        if [ "${REPO:0:8}" != "plugins/" ]
        then
            REPO="gerrit"
            pushd "$GERRIT_DIR_ABS" >/dev/null
        else
            pushd "$GERRIT_DIR_ABS/$REPO" >/dev/null
        fi
        local COMMIT_HASH="$(run_git rev-parse HEAD)"
        popd  >/dev/null
        REPO_CELL="<a href=\"https://gerrit-review.googlesource.com/#/admin/projects/$REPO\">$REPO</a>"
        DESCRIPTION_CELL="<a href=\"https://gerrit.googlesource.com/$REPO/+/$COMMIT_HASH\">${REPO_DESCRIPTIONS["$REPO"]}</a>"
        COMMIT_CELL="<a href=\"https://gerrit.googlesource.com/$REPO/+/$COMMIT_HASH\"><code>${COMMIT_HASH:0:8}</code></a>"
        if [ -n "${REPO_ARTIFACTS["$REPO"]}" ]
        then
            REPO_ARTIFACTS["$REPO"]="${REPO_ARTIFACTS["$REPO"]},"
        fi
        REPO_ARTIFACTS["$REPO"]="${REPO_ARTIFACTS["$REPO"]}$ARTIFACT_FILE_RELT"
    fi

    cat_target_html <<EOF

  <tr class="$STATUS">
    <td><img src="$IMAGE_BASE_URL/$STATUS.png" alt="Build $STATUS" />&#160;$STATUS</td>
    <th class="left th-semi-dark th-$STATUS">$ARTIFACT_CELL</th>
    <td class="right">$SIZE_CELL</td>
    <td>$BUCK_LOG_CELL</td>
    <td>$REPO_CELL</td>
    <td>$DESCRIPTION_CELL</td>
    <td>$COMMIT_CELL</td>
  </tr>
EOF

    ARTIFACTS_TOTAL=$((ARTIFACTS_TOTAL+1))
    if [ "$STATUS" = "ok" ]
    then
        ARTIFACTS_OK=$((ARTIFACTS_OK+1))
    elif [ "$STATUS" = "broken" ]
    then
        ARTIFACTS_BROKEN=$((ARTIFACTS_BROKEN+1))
    else
        ARTIFACTS_FAILED=$((ARTIFACTS_FAILED+1))
    fi
}

#---------------------------------------------------------------------
set_target_html_file_abs() {
    TARGET_HTML_FILE_ABS="$1"
    rm -f "$TARGET_HTML_FILE_ABS"
}

#---------------------------------------------------------------------
echo_target_html() {
    echo "$@" | cat_target_html  >>"$TARGET_HTML_FILE_ABS"
}

#---------------------------------------------------------------------
cat_target_html() {
    cat >>"$TARGET_HTML_FILE_ABS"
}

#---------------------------------------------------------------------
cat_html_head() {
    local TITLE="$1"
    local DESCRIPTION="$2"
    local KEYWORDS="$3"
    cat <<EOF
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
      <html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
  <title>$TITLE</title>
  <meta http-equiv="Content-type" content="text/html;charset=UTF-8" />
  <meta name="description" content="$DESCRIPTION" />
  <meta name="keywords" content="$KEYWORDS" />
  <link rel="shortcut icon" href="/favicon.ico" />
  <style type="text/css">
.left {
  text-align: left;
}
.right {
  text-align: right;
}
table, tr, th, td {
  border-collapse: collapse;
  border: 1px solid black;
}
table {
  margin-left: 1em;
}
th, td {
  padding-left: 0.4em;
  padding-right: 0.4em;
}
th {
  background-color: #ddd;
}
.th-semi-dark {
  background-color: #eee;
}
.failed, .th-failed {
  background-color: #ffaaaa;
}
  </style>
</head>
<body>

EOF
}

#---------------------------------------------------------------------
cat_html_tail() {
    cat <<EOF

</body>
</html>
EOF
}

#---------------------------------------------------------------------
setup_git_hooks() {
    local REPO_DIR_ABS="$(git rev-parse --git-dir)"
    # Hook to ensure Change-Id
    cp "$GERRIT_DIR_ABS/gerrit-server/src/main/resources/com/google/gerrit/server/tools/root/hooks/commit-msg" "$REPO_DIR_ABS/hooks/commit-msg"
    chmod 755 "$REPO_DIR_ABS/hooks/commit-msg"

    # Hook to guard against spaces
    cp "$REPO_DIR_ABS/hooks/pre-commit.sample" "$REPO_DIR_ABS/hooks/pre-commit"
    chmod 755 "$REPO_DIR_ABS/hooks/pre-commit"
}