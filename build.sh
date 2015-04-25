#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

DATE="$(date --utc +'%Y-%m-%d')"

FORCE=no
PULL=yes
CHECKOUT=yes
CLEAN=yes
TEST_UNIT=yes
TEST_SYSTEM=yes
STATUS=died
PRINT_VERSIONS=yes
LIMIT_TO=


print_help() {
    cat <<EOF
$0 ARGUMENTS

ARGUMENTS:
  --help             - prints this page
  --branch BRANCH    - Build branch BRANCH instead of the default, which is
                       inferred from the basename of the directory, with
                       "master" as fallback.
  --building         - Build artifacts (On per default)
  --checkout         - 'git checkout' before building (On per default)
  --clean            - clean before building (On per default)
  --force            - Overwrite eventual existing artifacts target directory
  --ignore-plugin PLUGIN
                     - Don't build, test, ... the plugin PLUGIN
  --no-building      - Don't build artifacts
  --no-checkout      - Don't 'git checkout' before building
  --no-clean         - Don't clean before building
  --no-pull          - Don't 'git pull' before building
  --no-repo-mangling - Neither 'git checkout' nor 'git pull' before building
  --no-system-testing
                     - Don't run system tests
  --no-testing       - Don't run any tests
  --no-unit-testing  - Don't run unit tests
  --no-versions      - Don't print version information of helper programs
  --nothing          - Don't run things that can be turned off
  --only-artifact ARTIFACT
                     - Build only the artifact ARTIFACT
  --pull             - 'git pull' before building (On per default)
  --system-testing   - Run system tests on artifacts (On per default)
  --unit-testing     - Run unit tests on artifacts (On per default)
  --versions         - Print version information of helper programs (On per
                       default)
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
        "--ignore-plugin" )
            [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
            IGNORED_PLUGINS=( "${IGNORED_PLUGINS[@]}" "$1" )
            shift || true
            ;;
        "--branch" )
            [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
            BRANCH="$1"
            shift || true
            ;;
        "--building" )
            BUILD_ARTIFACTS=yes
            ;;
        "--checkout" )
            CHECKOUT=yes
            ;;
        "--clean" )
            CLEAN=yes
            ;;
        "--no-building" )
            BUILD_ARTIFACTS=no
            ;;
        "--no-checkout" )
            CHECKOUT=no
            ;;
        "--no-clean" )
            CLEAN=no
            ;;
        "--no-pull" )
            PULL=no
            ;;
        "--no-repo-mangling" )
            CHECKOUT=no
            PULL=no
            ;;
        "--no-system-testing" )
            TEST_SYSTEM=no
            ;;
        "--no-testing" )
            TEST_UNIT=no
            TEST_SYSTEM=no
            ;;
        "--no-unit-testing" )
            TEST_UNIT=no
            ;;
        "--no-versions" )
            PRINT_VERSIONS=no
            ;;
        "--nothing" )
            BUILD_ARTIFACTS=no
            CHECKOUT=no
            CLEAN=no
            PRINT_VERSIONS=no
            PULL=no
            TEST_SYSTEM=no
            TEST_UNIT=no
            ;;
        "--only-artifact" )
            [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
            LIMIT_TO="$1"
            shift || true
            ;;
        "--pull" )
            PULL=yes
            ;;
        "--system-testing" )
            TEST_SYSTEM=yes
            ;;
        "--unit-testing" )
            TEST_UNIT=yes
            ;;
        "--versions" )
            PRINT_VERSIONS=yes
            ;;
        * )
            error "Unknown argument '$ARGUMENT'"
            ;;
    esac
done

post_parameter_parsing_setup

TARGET_DIR_ABS="$OVERVIEW_DIR_ABS/$DATE"

if [ -e "$TARGET_DIR_ABS" -a "$FORCE" = yes ]
then
    rm -rf "$TARGET_DIR_ABS"
fi

if [ -e "$TARGET_DIR_ABS" ]
then
    error "'$TARGET_DIR_ABS' already exists"
fi

mkdir -p "$TARGET_DIR_ABS"

compute_checksums() {
    pushd "$TARGET_DIR_ABS" >/dev/null
    rm -f sha1sums.txt
    sha1sum * >sha1sums.txt
    popd >/dev/null
}

dump_status() {
    echo "$STATUS" >"$TARGET_DIR_ABS/status.txt"
}

dump_status

info "Date: $DATE"
info "Branch: $BRANCH"

set_target_html_file_abs "$TARGET_DIR_ABS/index.html"

cat_html_header_target_html \
    "$DATE gerrit $BRANCH build" \
    "Build of $BRANCH commitish of gerrit from $DATE" \
    "gerrit, jar, $BRANCH" \
    "$DATE build of $BRANCH of gerrit"

cat_target_html <<EOF
<h2 id="summary">Build summary</h2>

<table>
<tr class="$STATUS"><th class="th-$STATUS">Build status</th><td><img src="$IMAGE_BASE_URL/$STATUS.png" alt="Build $STATUS" />&#160;$STATUS</td></tr>
<tr><th>Build start</th><td>$(timestamp)</td></tr>
<tr><th>Build end</th><td>---</td></tr>
<tr><th>Commitish</th><td>$BRANCH</td></tr>
<tr><th>Description</th><td>---</td></tr>
<tr><th>API version</th><td>---</td></tr>
<tr><th>DB schema version</th><td>---</td></tr>
</table>

<h2 id="artifacts">Artifacts</h2>
EOF

section "Updating gerrit"
cd "$GERRIT_DIR_ABS"
if [ "$CHECKOUT" = "yes" ]
then
    run_git checkout "$BRANCH"
    if [ -e ".gitmodules" ]
    then
        run_git submodule update --recursive
    fi
fi
if [ "$PULL" = "yes" ]
then
    run_git pull --recurse-submodules=yes
fi

API_VERSION="$(grep ^GERRIT_VERSION "$GERRIT_DIR_ABS"/VERSION | cut -f 2 -d \')"
info "API version: $API_VERSION"
echo "$API_VERSION" >"$TARGET_DIR_ABS/api_version.txt"

DB_SCHEMA_VERSION="$(grep 'static.*final.*Class.*Schema.*C.*Schema_[0-9]\+.class' "$GERRIT_DIR_ABS"/gerrit-server/src/main/java/com/google/gerrit/server/schema/SchemaVersion.java | head -n 1 | sed 's/^.*_\([0-9]*\)\.class;$/\1/')"
if [[ ! "$DB_SCHEMA_VERSION" =~ ^[0-9]+$ ]]
then
    error "Extracted database schema version is not a number, but '$DB_SCHEMA_VERSION'"
fi
info "Database schema version: $DB_SCHEMA_VERSION"
echo "$DB_SCHEMA_VERSION" >"$TARGET_DIR_ABS/db_schema_version.txt"

describe_repo

info "Description: ${REPO_DESCRIPTIONS["gerrit"]}"
echo "${REPO_DESCRIPTIONS["gerrit"]}" >"$TARGET_DIR_ABS/gerrit_description.txt"

BUCK_WANTED_VERSION="$(cat "$GERRIT_DIR_ABS/.buckversion")"

if [ ! -z "$(which buckd)" ]
then
    buckd --kill || true
fi

if [ "$BUILD_ARTIFACTS" = "yes" ]
then
    if [ "$BUCK_WANTED_VERSION" != "$(run_buck --version 2>/dev/null | cut -f 3 -d ' ')" ]
    then
        section "Rebuilding buck"
        pushd "$BUCK_DIR_ABS" >/dev/null
        run_git checkout master
        run_git pull
        run_git checkout "$BUCK_WANTED_VERSION"
        ant
        popd >/dev/null
    fi
fi

pushd "$GERRIT_DIR_ABS" >/dev/null
GERRIT_EXCLUDE_FILE_ABS="$(git rev-parse --git-dir)/info/exclude"
popd >/dev/null

remove_plugin_links() {
    local EXTRA_PLUGIN_DIR_ABS=
    for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
    do
        local EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"
        if [ -h "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME" ]
        then
            rm "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME"
        fi
    done
}

add_plugin_link() {
    local EXTRA_PLUGIN_NAME="$1"
    local EXTRA_PLUGIN_DIR_ABS="$EXTRA_PLUGINS_DIR_ABS/$EXTRA_PLUGIN_NAME"

    if [ ! -e "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME" ]
    then
        ln -s "$EXTRA_PLUGIN_DIR_ABS" "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME"

        if ! grep --quiet '^/plugins/'"$EXTRA_PLUGIN_NAME"'\( \|$\)' "$GERRIT_EXCLUDE_FILE_ABS" &>/dev/null
        then
            echo "/plugins/$EXTRA_PLUGIN_NAME" >>"$GERRIT_EXCLUDE_FILE_ABS"
        fi
    fi
}

build_plugin() {
    local PLUGIN_NAME="$1"
    local PLUGIN_GROUP="$2"
    if is_ignored_plugin "$PLUGIN_NAME"
    then
        info "Skipped, as $PLUGIN_NAME is ignored"
        continue
    fi

    run_buck_build "$PLUGIN_NAME" "plugins/$PLUGIN_NAME:$PLUGIN_NAME" "plugins/$PLUGIN_NAME/$PLUGIN_NAME.jar" "$PLUGIN_GROUP"
}

# Pulling new commits for extra plugins
for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
do
    EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"
    section "Updating $EXTRA_PLUGIN_NAME"

    if is_ignored_plugin "$EXTRA_PLUGIN_NAME"
    then
        info "Skipped, as $EXTRA_PLUGIN_NAME is ignored"
        continue
    fi

    pushd "$EXTRA_PLUGIN_DIR_ABS" >/dev/null
    if [ "$CHECKOUT" = "yes" ]
    then
        run_git checkout "$BRANCH"
    fi
    if [ "$PULL" = "yes" ]
    then
        run_git pull
    fi
    popd >/dev/null
done

remove_plugin_links

# Describe repos an find test labels
for PLUGIN_DIR_ABS in "$GERRIT_DIR_ABS/plugins"/* "$EXTRA_PLUGINS_DIR_ABS"/*
do
    if [ -d "$PLUGIN_DIR_ABS" ]
    then
        PLUGIN_NAME="$(basename "$PLUGIN_DIR_ABS")"

        section "Reading $PLUGIN_NAME"

        if is_ignored_plugin "$PLUGIN_NAME"
        then
            info "Skipped, as $PLUGIN_NAME is ignored"
            continue
        fi

        pushd "$PLUGIN_DIR_ABS" >/dev/null
        describe_repo

        # Find test label
        TEST_LABEL=
        if [ -n "$(find -type d -name test)" ]
        then
            if [ -e "BUCK" ]
            then
                TEST_LABEL=$(grep 'labels[[:space:]]*=.*'\' "BUCK" | cut -f 2 -d "'" || true)
            fi
        fi
        TARGET_TEST_LABELS["plugins/$PLUGIN_NAME:$PLUGIN_NAME"]="$TEST_LABEL"

        popd >/dev/null
    fi
done

echo_build_description_json() {
    cat <<EOF
{
  "commitish": "$BRANCH",
  "api_version": "$API_VERSION",
  "db_schema_version": $DB_SCHEMA_VERSION,
EOF
    echo -n "  \"artifacts-groups\": {"
    local ARTIFACT_GROUP=
    local CONNECTOR=""
    for ARTIFACT_GROUP in "${!ARTIFACT_GROUP_STATUS[@]}"
    do
        echo "$CONNECTOR"
        echo -n "    \"$ARTIFACT_GROUP\": { "
        echo -n "\"status\": \"${ARTIFACT_GROUP_STATUS[$ARTIFACT_GROUP]}\", "
        echo -n "\"status_count\": \"${ARTIFACT_GROUP_STATUS_COUNT[$ARTIFACT_GROUP]}\", "
        echo -n "\"total_count\": \"${ARTIFACT_GROUP_TOTAL_COUNT[$ARTIFACT_GROUP]}\""
        echo -n "}"
        CONNECTOR=","
    done
    echo
    cat <<EOF
  },
  "repositories": {
EOF
    local REPO_NAME=
    for REPO_NAME in "${REPO_NAMES[@]}"
    do
        echo -n "    \"$REPO_NAME\": { "
        echo -n "\"commit\": \"${REPO_DESCRIPTIONS["$REPO_NAME"]}\", "
        echo -n "\"expected_artifacts\": ["
        if [ -n "${REPO_ARTIFACTS["$REPO_NAME"]}" ]
        then
            echo -n "\""
            echo -n "${REPO_ARTIFACTS["$REPO_NAME"]}" | sed -e 's/,/", "/g'
            echo -n "\""
        fi
        echo -n "]"
        echo -n " }"
        if [ "$REPO_NAME" != "$LAST_REPO_NAME" ]
        then
            echo -n ","
        fi
        echo
    done
    echo "  }"
    echo "}"
}

echo_build_description_json_file() {
    echo_build_description_json >"$TARGET_DIR_ABS/build_description.json"
}

echo_artifacts_group_numbers_txt() {
    for ARTIFACT_GROUP in "${!ARTIFACT_GROUP_STATUS[@]}"
    do
        echo -n "$ARTIFACT_GROUP,"
        echo -n "${ARTIFACT_GROUP_TOTAL_COUNT[$ARTIFACT_GROUP]},"
        echo -n "${ARTIFACT_GROUP_STATUS[$ARTIFACT_GROUP]},"
        echo "${ARTIFACT_GROUP_STATUS_COUNT[$ARTIFACT_GROUP]}"
    done | sort --field-separator=',' --key=1,1 --key=2,2n
}

echo_artifacts_group_numbers_txt_file() {
    echo_artifacts_group_numbers_txt >"$TARGET_DIR_ABS/artifacts_group_numbers.txt"
}

echo_build_description_json_file

cat >"$GERRIT_DIR_ABS/.buckconfig.local" <<EOF
[cache]
  mode = dir
  dir = $GERRIT_DIR_ABS/buck-cache/internally-built-artifacts
EOF

if [ "$CLEAN" = "yes" ]
then
    run_buck clean
    rm -rf "$GERRIT_DIR_ABS/buck-out"
    rm -rf "$GERRIT_DIR_ABS/buck-cache"
fi

# Building WARs that do not depend on plugins
run_buck_build "gerrit, gerrit.war" "//:gerrit" "gerrit.war" "war"
run_buck_build "gerrit, withdocs.war" "//:withdocs" "withdocs.war" "war"

#Building api
for API in \
    "gerrit-extension-api:extension-api" \
    "gerrit-plugin-api:plugin-api" \
    "gerrit-plugin-gwtui:gwtui-api" \

do
    for ASPECT in '' '-src' '-javadoc'
    do
        if [ "$API$ASPECT" = "gerrit-extension-api:extension-api-src" ]
        then
            EXPECTED_JAR="$(sed -e 's@^\([^:-]*-\([^:]*\)\):\(.*\)$@\1/lib__\2'"$ASPECT"'__output/\2@' <<<"$API")$ASPECT.jar"
        else
            EXPECTED_JAR="${API//://}$ASPECT.jar"
        fi
        run_buck_build "gerrit, $(cut -f 2 -d : <<<"$API")$ASPECT" "//$API$ASPECT" "$EXPECTED_JAR" "api"
    done
done

run_buck_build "gerrit, api" "api" "api.zip" "api"

# Building bundled plugins
for PLUGIN_DIR_ABS in "$GERRIT_DIR_ABS/plugins"/*
do
    if [ -d "$PLUGIN_DIR_ABS" ]
    then
        PLUGIN_NAME="$(basename "$PLUGIN_DIR_ABS")"
        build_plugin "$PLUGIN_NAME" "bundled"
    fi
done

# Building release
#
# This is after the bundled plugins, to avoid that building the
# release.war would warm the caches for the bundled plugins (and
# thereby stealing logs)
run_buck_build "gerrit, release.war" "//:release" "release.war" "war" "gerrit.war"


# Building extra plugins
for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
do
    EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"

    # We skip early, to not remove the plugin link again, if only a
    # single plugin is getting built.
    if [ -n "$LIMIT_TO" -a "$LIMIT_TO" != "$EXTRA_PLUGIN_NAME.jar" ]
    then
        continue
    fi

    # Setup plugin links as minimal as possible, to avoid plugins with
    # broken BUCK files getting in the way of other plugins
    remove_plugin_links
    add_plugin_link "$EXTRA_PLUGIN_NAME"
    case "$EXTRA_PLUGIN_NAME" in
        "its-bugzilla" \
            | "its-jira" \
            | "its-phabricator" \
            | "its-rtc" \
            | "its-storyboard" \
            )
            add_plugin_link "its-base"
            ;;
    esac

    build_plugin "$EXTRA_PLUGIN_NAME" "separate"
done

# All buck artifacts have been built here -------------------------------------

echo_build_description_json_file

compute_checksums

echo "$ARTIFACTS_FAILED" >"$TARGET_DIR_ABS/failure_count.txt"
echo "$ARTIFACTS_BROKEN" >"$TARGET_DIR_ABS/broken_count.txt"

echo_artifacts_group_numbers_txt_file

echo_file_target_html "ok" "artifacts_group_numbers.txt"
echo_file_target_html "ok" "build_description.json"
echo_file_target_html "ok" "api_version.txt"
echo_file_target_html "ok" "broken_count.txt"
echo_file_target_html "ok" "db_schema_version.txt"
echo_file_target_html "ok" "failure_count.txt"
echo_file_target_html "ok" "gerrit_description.txt"
echo_file_target_html "ok" "sha1sums.txt"
echo_file_target_html "ok" "status.txt"


echo_artifacts_group_numbers_txt_file
echo_build_description_json_file
compute_checksums

echo_target_html "</table>"

if [ "$ARTIFACTS_TOTAL" = "0" ]
then
    STATUS=failed
else
    if [ "$ARTIFACTS_FAILED" = "0" ]
    then
        if [ "$ARTIFACTS_BROKEN" = "0" ]
        then
            STATUS=ok
        else
            STATUS=broken
        fi
    else
        STATUS=failed
    fi
fi

cat_artifacts_summary_row_target_html() {
    local STATUS="$1"
    local COUNT="$2"

    local STATUS_TEXT=
    set_STATUS_TEXT "uncounted"

    local ATTRIBUTE=
    if [ "$COUNT" -gt 0 ]
    then
        ATTRIBUTE=" class=\"$STATUS\""
    fi

    echo_target_html "<tr><td$ATTRIBUTE>${STATUS_TEXT^}</td><td$ATTRIBUTE>$COUNT</td></tr>"
}

cat_artifacts_summary_target_html() {
    cat_target_html <<EOF

<h3>Artifacts summary</h3>
<table>
  <tr><th>Artifacts</th><th>Count</th></tr>
EOF

    cat_artifacts_summary_row_target_html "failed" "$ARTIFACTS_FAILED"
    cat_artifacts_summary_row_target_html "broken" "$ARTIFACTS_BROKEN"
    cat_artifacts_summary_row_target_html "ok" "$ARTIFACTS_OK"
    cat_artifacts_summary_row_target_html "total" "$ARTIFACTS_TOTAL"

    cat_target_html <<EOF
</table>
EOF
}

cat_artifacts_summary_target_html

if [ "$PRINT_VERSIONS" = "yes" ]
then
    cat_target_html <<EOF

<h2>Build environment</h2>

<table>
  <tr><th>Ant</th><td>$(ant -version | head -n 1 | sed -e 's/^.*version \(.*\) compiled.*/\1/')</td></tr>
  <tr><th>Buck</th><td>$(buck --version 2>/dev/null | head -n 1 | sed -e 's/^.* version //')</td></tr>
  <tr><th>Java</th><td>$(java -version 2>&1 | head -n 1 | cut -f 2 -d '"')</td></tr>
  <tr><th>Maven</th><td>$(mvn -version | head -n 1 | sed -e 's/^Apache Maven \(.*\) (.*/\1/')</td></tr>
  <tr><th>Watchman</th><td>$(watchman --version)</td></tr>
</table>
EOF
fi

echo_target_html "<p>Unless otherwise noted or implied from the sources, the artifacts are provided under the <a href=\"http://www.apache.org/licenses/LICENSE-2.0\">Apache License, Version 2.0</a>.</p>"

cat_html_footer_target_html

set_STATUS_TEXT

FINAL_STATUS_MARKUP=${STATUS_TEXT//&/\\&}

case "$STATUS" in
    "failed" )
        FINAL_STATUS_MARKUP="<a href=\"#$FIRST_FAILED_ARTIFACT\">$FINAL_STATUS_MARKUP</a>"
        ;;
    "broken" )
        FINAL_STATUS_MARKUP="<a href=\"#$FIRST_BROKEN_ARTIFACT\">$FINAL_STATUS_MARKUP</a>"
        ;;
esac

sed -i \
    -e '/Build.status/s/died/'"$STATUS"'/g' \
    -e '/Build.status/s@'"$STATUS"'\(<\)@'"$FINAL_STATUS_MARKUP"'\1@' \
    -e '/Build.end.*---/s/---/'"$(timestamp)"'/g' \
    -e '/Description.*---/s/---/'"${REPO_DESCRIPTIONS["gerrit"]}"'/g' \
    -e '/API version/s/---/'"$API_VERSION"'/g' \
    -e '/DB schema version/s/---/'"$DB_SCHEMA_VERSION"'/g' \
    "$TARGET_HTML_FILE_ABS"

dump_status
compute_checksums

"$SCRIPT_DIR_ABS/write_overview_index_html.sh"

case "$STATUS" in
    "ok" )
        EXIT_CODE=0
        ;;
    "broken" )
        EXIT_CODE=10
        ;;
    "failed" )
        EXIT_CODE=11
        ;;
    "died" )
        EXIT_CODE=12
        ;;
    * )
        EXIT_CODE=13
        ;;
esac

finalize "$EXIT_CODE"
