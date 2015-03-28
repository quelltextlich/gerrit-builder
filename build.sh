#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

DATE="$(date --utc +'%Y-%m-%d')"

FORCE=no
PULL=yes
CHECKOUT=yes
CLEAN=yes
TEST=yes
STATUS=died

print_help() {
    cat <<EOF
$0 ARGUMENTS

ARGUMENTS:
  --help             - prints this page
  --branch BRANCH    - Build branch BRANCH instead of the default, which is
                       inferred from the basename of the directory, with
                       "master" as fallback.
  --force            - Overwrite eventual existing artifacts target directory
  --no-building      - Don't build artifacts
  --no-checkout      - Don't 'git checkout' before building
  --no-clean         - Don't clean before building
  --no-pull          - Don't 'git pull' before building
  --no-repo-mangling - Neither 'git checkout' nor 'git pull' before building
  --no-testing       - Don't run tests
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
        "--no-testing" )
            TEST=no
            ;;
        * )
            error "Unknown argument '$ARGUMENT'"
            ;;
    esac
done

OVERVIEW_DIR_ABS="$ARTIFACTS_NIGHTLY_DIR_ABS/$BRANCH"
OVERVIEW_HTML_FILE_ABS="$OVERVIEW_DIR_ABS/index.html"

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
<tr><th>Build date</th><td>$DATE</td></tr>
<tr><th>Commitish</th><td>$BRANCH</td></tr>
<tr><th>Description</th><td>---</td></tr>
<tr><th>API version</th><td>---</td></tr>
<tr><th>DB schema version</th><td>---</td></tr>
</table>

<h2 id="artifacts">Artifacts</h2>

<table>
<tr>
<th>Status</th>
<th>Artifact</th>
<th>Size</th>
<th>Logs</th>
<th>Repository</th>
<th>Description</th>
<th>Commit</th>
</tr>
EOF

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
    popd >/dev/null

    if [ -h "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME" ]
    then
        rm "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME"
    fi

    if [ ! -e "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME" ]
    then
        ln -s "$EXTRA_PLUGIN_DIR_ABS" "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME"

        if ! grep --quiet '^/plugins/'"$EXTRA_PLUGIN_NAME"'\( \|$\)' "$GERRIT_EXCLUDE_FILE_ABS" &>/dev/null
        then
            echo "/plugins/$EXTRA_PLUGIN_NAME" >>"$GERRIT_EXCLUDE_FILE_ABS"
        fi
    fi
done

for PLUGIN_DIR_ABS in "$GERRIT_DIR_ABS/plugins"/*
do
    if [ -d "$PLUGIN_DIR_ABS" ]
    then
        PLUGIN_NAME="$(basename "$PLUGIN_DIR_ABS")"

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
    echo "{"
    echo "  commitish: \"$BRANCH\","
    echo "  api_version: \"$API_VERSION\","
    echo "  db_schema_version: $DB_SCHEMA_VERSION,"
    echo "  repositories: {"
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

run_buck_build "gerrit, gerrit.war" "//:gerrit" "gerrit.war"
run_buck_build "gerrit, withdocs.war" "//:withdocs" "withdocs.war"
run_buck_build "gerrit, release.war" "//:release" "release.war"

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
        run_buck_build "gerrit, $(cut -f 2 -d : <<<"$API")$ASPECT" "//$API$ASPECT" "$EXPECTED_JAR"
    done
done

run_buck_build "gerrit, api" "api" "api.zip"

for PLUGIN_DIR_ABS in "$GERRIT_DIR_ABS/plugins"/*
do
    if [ -d "$PLUGIN_DIR_ABS" ]
    then
        PLUGIN_NAME="$(basename "$PLUGIN_DIR_ABS")"
        run_buck_build "$PLUGIN_NAME" "plugins/$PLUGIN_NAME:$PLUGIN_NAME" "plugins/$PLUGIN_NAME/$PLUGIN_NAME.jar"
    fi
done

echo_build_description_json_file

pushd "$TARGET_DIR_ABS" >/dev/null
sha1sum * >sha1sums.txt
popd >/dev/null

echo "$ARTIFACTS_FAILED" >"$TARGET_DIR_ABS/failure_count.txt"
echo "$ARTIFACTS_BROKEN" >"$TARGET_DIR_ABS/broken_count.txt"

echo_file_target_html "ok" "build_description.json"
echo_file_target_html "ok" "api_version.txt"
echo_file_target_html "ok" "broken_count.txt"
echo_file_target_html "ok" "db_schema_version.txt"
echo_file_target_html "ok" "failure_count.txt"
echo_file_target_html "ok" "gerrit_description.txt"
echo_file_target_html "ok" "sha1sums.txt"
echo_file_target_html "ok" "status.txt"

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

echo_target_html "<p>Unless otherwise noted or implied from the sources, the artifacts are provided under the <a href=\"http://www.apache.org/licenses/LICENSE-2.0\">Apache License, Version 2.0</a>.</p>"

cat_html_footer_target_html

set_STATUS_TEXT

sed -i \
    -e '/Build.status/s/died/'"$STATUS"'/g' \
    -e '/Build.status/s/'"$STATUS"'\(<\)/'"${STATUS_TEXT//&/\\&}"'\1/' \
    -e '/Description.*---/s/---/'"${REPO_DESCRIPTIONS["gerrit"]}"'/g' \
    -e '/API version/s/---/'"$API_VERSION"'/g' \
    -e '/DB schema version/s/---/'"$DB_SCHEMA_VERSION"'/g' \
    "$TARGET_HTML_FILE_ABS"

dump_status

set_target_html_file_abs "$OVERVIEW_DIR_ABS/index.html"

cat_html_header_target_html \
    "Gerrit $BRANCH builds" \
    "Gerrit $BRANCH builds" \
    "gerrit, jar, $BRANCH" \
    "Gerrit builds for $BRANCH"

cat_target_html <<EOF
<table>
  <tr>
    <th>Build</th>
    <th>Status</th>
    <th>Gerrit HEAD</th>
    <th>API version</th>
    <th>DB schema version</th>
  </tr>
EOF

pushd "$OVERVIEW_DIR_ABS" >/dev/null
for DIR_RELC in *
do
    if [ -d "$DIR_RELC" ]
    then
        DIR_STATUS=$(cat "$DIR_RELC/status.txt" || true)
        case "$DIR_STATUS" in
            "failed" )
                DIR_ARTIFACTS_FAILED=$(cat "$DIR_RELC/failure_count.txt" || true)
                if [ -z "$DIR_ARTIFACTS_FAILED" ]
                then
                    DIR_ARTIFACTS_FAILED="?"
                fi
                STATUS_CELL_TEXT="$DIR_ARTIFACTS_FAILED $DIR_STATUS"
                ;;
            "broken" )
                DIR_ARTIFACTS_BROKEN=$(cat "$DIR_RELC/broken_count.txt" || true)
                if [ -z "$DIR_ARTIFACTS_BROKEN" ]
                then
                    DIR_ARTIFACTS_BROKEN="?"
                fi
                STATUS_CELL_TEXT="$DIR_ARTIFACTS_BROKEN $DIR_STATUS"
                ;;
            "ok" | \
                "died" )
                STATUS_CELL_TEXT="$DIR_STATUS"
                ;;
            * )
                DIR_STATUS="died"
                STATUS_CELL_TEXT="$DIR_STATUS"
                ;;
        esac

        DIR_API_VERSION=$(cat "$DIR_RELC/api_version.txt" || true)
        if [ -z "$DIR_API_VERSION" ]
        then
            DIR_API_VERSION="---"
        fi

        DIR_DB_SCHEMA_VERSION=$(cat "$DIR_RELC/db_schema_version.txt" || true)
        if [ -z "$DIR_DB_SCHEMA_VERSION" ]
        then
            DIR_DB_SCHEMA_VERSION="---"
        fi

        DIR_REPO_DESCRIPTION=$(cat "$DIR_RELC/gerrit_description.txt" || true)
        if [ -z "$DIR_REPO_DESCRIPTION" ]
        then
            DIR_REPO_DESCRIPTION="---"
        fi

        cat_target_html <<EOF
  <tr>
    <td><a href="$DIR_RELC/index.html">$DIR_RELC</a></td>
    <td><img src="$IMAGE_BASE_URL/$DIR_STATUS.png" alt="Build $DIR_STATUS" />&#160;$STATUS_CELL_TEXT</td>
    <td>$DIR_REPO_DESCRIPTION</td>
    <td>$DIR_API_VERSION</td>
    <td>$DIR_DB_SCHEMA_VERSION</td>
  </tr>
EOF
    fi
done
popd >/dev/null

echo_target_html "</table>"

cat_html_footer_target_html

finalize
