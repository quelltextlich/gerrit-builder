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
#---------------------------------------------------------------------
BUILD_ARTIFACTS=yes
LEGAL_URL_RELS="legal.url"
#---------------------------------------------------------------------
BRANCH="$(basename "$SCRIPT_DIR_ABS" | cut -f 1,2 -d - | cut -f 1 -d _ )"
if [[ "$BRANCH" != "master" && ! ( "$BRANCH" =~ stable-[0-9]+.[0-9]+ ) ]]
then
    BRANCH="master"
fi

add_to_path() {
    local PATH_DIR_ABS="$1"
    PATH="$PATH_DIR_ABS:$PATH"
}

add_to_path "$JAVA_DIR_ABS/bin"
add_to_path "$ANT_DIR_ABS/bin"
add_to_path "$BUCK_DIR_ABS/bin"
add_to_path "$WATCHMAN_DIR_ABS"
add_to_path "$MAVEN_DIR_ABS/bin"

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
in_array() {
    local NEEDLE="$1"
    shift
    local HAY
    for HAY in "$@"
    do
        if [ "$HAY" = "$NEEDLE" ]
        then
            return 0
        fi
    done
    return 1
}

#---------------------------------------------------------------------
post_parameter_parsing_setup() {
    OVERVIEW_DIR_ABS="$ARTIFACTS_NIGHTLY_DIR_ABS/$BRANCH"
}

# Calling once already upon sourcing, put scripts are encouraged to
# call it again once they are finished with parameter parsing
post_parameter_parsing_setup

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
    local EXIT_CODE="$1"
    if [ -z "$EXIT_CODE" ]
    then
        EXIT_CODE=0
    fi
    info "pass." "Arguments:" "${SCRIPT_ARGUMENTS[@]}"
    exit "$EXIT_CODE"
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
timestamp() {
    date --utc +'%Y-%m-%d %H:%M:%S'
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
run_buck_logged() {
    local ASPECT="$1"
    # no shift ^, as the buck command (build/test) is getting used as
    # ASPECT.

    local RETURN_CODE=0

    local LOG_FILE_ABS="$GERRIT_DIR_ABS/buck-out/log/buck-0.log"

    rm -f "$LOG_FILE_ABS"

    if ! run_buck "$@" 2> >(tee "$TARGET_FILE_ABS.$ASPECT.stderr.txt" >&2) | tee "$TARGET_FILE_ABS.$ASPECT.stdout.txt"
    then
        RETURN_CODE=1
    fi

    if [ -e "$LOG_FILE_ABS" ]
    then
        cp "$LOG_FILE_ABS" "$TARGET_FILE_ABS.$ASPECT.buck_log.txt"
    fi
    return "$RETURN_CODE"
}

#---------------------------------------------------------------------
run_buck_build() {
    local DESCRIPTION="$1"
    local BUCK_TARGET="$2"
    local BUCK_GENERATED_FILE_RELBG="$3"
    local ARTIFACT_KIND="$4"
    local INSERT_BEFORE="$5"
    local STATUS="failed"
    local ARTIFACT_BASENAME="$(basename "$BUCK_GENERATED_FILE_RELBG")"

    if [ -n "$LIMIT_TO" -a "$LIMIT_TO" != "$ARTIFACT_BASENAME" ]
    then
        return
    fi

    local SOURCE_FILE_ABS="$GERRIT_DIR_ABS/buck-out/gen/$BUCK_GENERATED_FILE_RELBG"
    local TARGET_FILE_ABS="$TARGET_DIR_ABS/$ARTIFACT_BASENAME"

    local LOG_FILE_ABS="$GERRIT_DIR_ABS/buck-out/log/buck-0.log"

    section "Building $DESCRIPTION"

    if test "$BUILD_ARTIFACTS" = "no" || run_buck_logged build "$BUCK_TARGET"
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

    if [ "$STATUS" = "ok" -a "$TEST" = "yes" -a -n "${TARGET_TEST_LABELS["$BUCK_TARGET"]}" ]
    then
        if ! run_buck_logged test --include "${TARGET_TEST_LABELS["$BUCK_TARGET"]}" then
        then
            STATUS="broken"
        fi
    fi

    echo_file_target_html "$STATUS" "$ARTIFACT_BASENAME" "$BUCK_TARGET" "$INSERT_BEFORE" "$ARTIFACT_KIND"
}

#---------------------------------------------------------------------
set_STATUS_TEXT() {
    local ASPECT="$1"
    if [ -z "$ASPECT" ]
    then
        ASPECT="counted"
    fi

    STATUS_TEXT="$2"
    if [ -z "$STATUS_TEXT" ]
    then
        STATUS_TEXT="$STATUS"
    fi


    local COUNT=""
    case "$STATUS_TEXT" in
        "failed" )
            STATUS_TEXT="failed&#160;build"
            COUNT="$ARTIFACTS_FAILED"
            ;;
        "broken" )
            STATUS_TEXT="broken&#160;test"
            COUNT="$ARTIFACTS_BROKEN"
            ;;
    esac

    if [ "$ASPECT" = "counted" -a -n "$COUNT" ]
    then
        STATUS_TEXT="$COUNT $STATUS_TEXT"
        if [ "$COUNT" != "1" ]
        then
            STATUS_TEXT="${STATUS_TEXT}s"
        fi
    fi
}
#---------------------------------------------------------------------
echo_logged_build_cell() {
    local ARTIFACT_FILE_RELT="$1"
    local ASPECT="$2"
    local CONNECTOR=""

    local LOG=
    for LOG in stderr stdout buck_log
    do
        local LOG_FILE_RELT="$ARTIFACT_FILE_RELT.$ASPECT.$LOG.txt"
        if [ -e "$TARGET_DIR_ABS/$LOG_FILE_RELT" ]
        then
            echo -n "$CONNECTOR<a href=\"$LOG_FILE_RELT\">$LOG</a>"
            CONNECTOR=", "
        fi
    done
    if [ -z "$CONNECTOR" ]
    then
        echo -n "---"
    fi
}

#---------------------------------------------------------------------
cat_file_header_target_html() {
    cat_target_html <<EOF
  <tr>
    <th>Kind</th>
    <th>Status</th>
    <th>Artifact</th>
    <th>Size</th>
    <th>Build logs</th>
    <th>Test logs</th>
    <th>Repository</th>
    <th>Description</th>
    <th>Commit</th>
    <th colspan="3">Changes</th>
  </tr>
EOF
}

#---------------------------------------------------------------------
echo_file_target_html() {
    local STATUS="$1"
    local ARTIFACT_FILE_RELT="$2"
    local BUCK_TARGET="$3"
    local INSERT_BEFORE="$4"
    local ARTIFACT_KIND="$5"

    if [ -z "$ARTIFACT_KIND" ]
    then
        ARTIFACT_KIND="info"
    fi

    if [ "$MAIN_ARTIFACT_TABLE_LAST_KIND" != "$ARTIFACT_KIND" -a -z "$INSERT_BEFORE" ]
    then
        cat_file_header_target_html
    fi
    MAIN_ARTIFACT_TABLE_LAST_KIND="$ARTIFACT_KIND"

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

    local BUILD_LOG_CELL="---"
    local TEST_LOG_CELL="---"
    local REPO_CELL="---"
    local DESCRIPTION_CELL="---"
    local COMMIT_CELL="---"
    local DASHBOARD_CELL="---"
    local OPEN_CHANGES_CELL="---"
    local CLOSED_CHANGES_CELL="---"
    if [ -n "$BUCK_TARGET" ]
    then
        BUILD_LOG_CELL="$(echo_logged_build_cell "$ARTIFACT_FILE_RELT" "build")"
        TEST_LOG_CELL="$(echo_logged_build_cell "$ARTIFACT_FILE_RELT" "test")"

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
        DASHBOARD_CELL="<a href=\"https://gerrit-review.googlesource.com/#/projects/$REPO,dashboards/default\">dashboard</a>"
        OPEN_CHANGES_CELL="<a href=\"https://gerrit-review.googlesource.com/#/q/project:$REPO+branch:$BRANCH+status:open\">open</a>"
        CLOSED_CHANGES_CELL="<a href=\"https://gerrit-review.googlesource.com/#/q/project:$REPO+branch:$BRANCH+status:closed\">closed</a>"
    fi

    local STATUS_TEXT=
    set_STATUS_TEXT "uncounted"

    local HTML="
  <!-- Artifact: $ARTIFACT_FILE_RELT -->
  <tr class=\"$STATUS\">
    <td>$ARTIFACT_KIND</td>
    <td><img src=\"$IMAGE_BASE_URL/$STATUS.png\" alt=\"Build $STATUS\" />&#160;$STATUS_TEXT</td>
    <th class=\"left th-semi-dark th-$STATUS\">$ARTIFACT_CELL</th>
    <td class=\"right\">$SIZE_CELL</td>
    <td>$BUILD_LOG_CELL</td>
    <td>$TEST_LOG_CELL</td>
    <td>$REPO_CELL</td>
    <td>$DESCRIPTION_CELL</td>
    <td>$COMMIT_CELL</td>
    <td>$DASHBOARD_CELL</td>
    <td>$OPEN_CHANGES_CELL</td>
    <td>$CLOSED_CHANGES_CELL</td>
  </tr>"

    if [ -z "$INSERT_BEFORE" ]
    then
        echo_target_html "$HTML"
    else
        HTML="${HTML//
/\\n}"
        sed -i -e '/Artifact: '"$INSERT_BEFORE"'/s@^@'"${HTML//&/\\&}"'\n@' "$TARGET_HTML_FILE_ABS"
    fi

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
set_LEGAL_URL() {
    if [ -z "$LEGAL_URL" ]
    then
        pushd "$SCRIPT_DIR_ABS" >/dev/null
        if [ -e "$LEGAL_URL_RELS" ]
        then
            LEGAL_URL="$(cat "$LEGAL_URL_RELS")"
            if [ -z "$LEGAL_URL" ]
            then
                error "'$SCRIPT_DIR_ABS/$LEGAL_URL_RELS' is empty. Please make that file contain the 'Contact / Legal' URL"
            fi
        else
            error "'$SCRIPT_DIR_ABS/$LEGAL_URL_RELS' does not exist. Please make that file contain the 'Contact / Legal' URL"
        fi
        popd >/dev/null
    fi
}

#---------------------------------------------------------------------
cat_html_header_target_html() {
    local TITLE="$1"
    local DESCRIPTION="$2"
    local KEYWORDS="$3"
    local H1="$4"

    set_LEGAL_URL

    cat_target_html <<EOF
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
.broken, .th-broken {
  background-color: #ffccaa;
}
.legal-footer {
  display: block;
  margin-left: auto;
  margin-right: auto;
  text-align: right;
}
  </style>
</head>
<body>

<h1>$H1</h1>
EOF
    echo_html_split_target_html
}

#---------------------------------------------------------------------
cat_html_footer_target_html() {
    echo_html_split_target_html

    cat_target_html <<EOF

<p class="legal-footer">Last update: $(timestamp)<br/><a href="$LEGAL_URL">Contact / Legal</a></p>
</body>
</html>
EOF
}
#---------------------------------------------------------------------
echo_html_split_target_html() {
    local SPLIT="<p>- "
    if [ "$SKIP_PARENT_LINK" != "yes" ]
    then
        SPLIT="$SPLIT<a href=\"../index.html\">View parent directory</a> - "
    fi
    SPLIT="$SPLIT<a href=\".\">View raw listing</a> -</p>"
    echo_target_html "$SPLIT"
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