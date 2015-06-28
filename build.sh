#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

FORCE=no
PULL=yes
CHECKOUT=yes
CODE_COVERAGE=yes
CLEAN=yes
GENERATE_MANUAL=yes
GENERATE_JAVADOC=yes
IGNORED_UNIT_TESTS=()
REMOVE_LINKS=yes
MANAGE_LATEST_LINK=yes
STOP_TEST_SITE=yes
TEST_UNIT=yes
TEST_SYSTEM=yes
USE_JACOCO_TOOLBOX=yes
STATUS=died
PRINT_VERSIONS=yes
LIMIT_TO=()
TARGET_DIRECTORY_FORMAT="%Y-%m-%d"

DEFAULT_ARGUMENTS_FILE_RELS="build.sh.arguments"

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
  --code-coverage    - Generate code coverage analysis of unit tests.
                       Implies running unit tests.
                       (On per default)
  --documentation    - Build documentation (manual, javadoc, code coverage)
                       (On per default)
  --force            - Overwrite eventual existing artifacts target directory
  --jacoco-toolbox   - Generate coverage reports using the jacoco toolbox. This
                       allows for better titles in HTML code coverage reports.
  --javadoc          - Generate the javadoc documentation (On per default)
  --ignore-plugin PLUGIN
                     - Don't build, test, ... the plugin PLUGIN
  --ignore-unit-tests CLASS1,CLASS2,...
                     - Don't run unit tests in class CLASS1, CLASS2, ...
                       If this parameter is provided multiple times, the classes
                       are accumulated, and none of the unit tests in the
                       classes are run.
  --latest-linking   - Generate a 'latest' link in the artifacts directory
                       pointing to the latest build
  --link-removing    - Remove unneeded links to extra plugins underneath
                       "gerrit/plugins". (On per default)
  --manual           - Generate the manual. Implies running system tests. (On per default)
  --no-building      - Don't build artifacts
  --no-checkout      - Don't 'git checkout' before building
  --no-clean         - Don't clean before building
  --no-code-coverage - Don't generate code coverage analysis of unit tests.
  --no-documentation - Don't build documentation. No manual. No javadoc.
                       No code-coverage.
  --no-jacoco-toolbox
                     - Do not generate coverage reports using the jacoco
                       toolbox. But use only HTML coverage reports generated
                       by Buck.
  --no-javadoc       - Don't generate the javadoc documentation
  --no-manual        - Don't generate the manual
  --no-pull          - Don't 'git pull' before building
  --no-repo-mangling - Neither 'git checkout' nor 'git pull' before building
  --no-latest-linking
                     - Don't generate a 'latest' link pointing to the latest
                       build
  --no-link-removing - Don't remove unneeded links to extra plugins in
                       "gerrit/plugins".
  --no-system-testing
                     - Don't run system tests
  --no-test-site-stopping
                     - Don't stop test site for last system test
  --no-testing       - Don't run any tests
  --no-unit-testing  - Don't run unit tests
  --no-versions      - Don't print version information of helper programs
  --nothing          - Don't run things that can be turned off
  --only-artifact ARTIFACT
                     - Build only the artifact ARTIFACT
  --only-artifacts ARTIFACT1,ARTIFACT2,...
                     - Build only the artifacts ARTIFACT1, ARTIFACT2, ...
  --pull             - 'git pull' before building (On per default)
  --system-testing   - Run system tests on artifacts (On per default)
  --system-testing-war WAR_FILE
                     - Use WAR_FILE for system testing jars instead of this
                       build's gerrit.war.
  --target-directory-format FORMAT
                     - Format of the directory holding the built artifacts as
                       FORMAT. You can use any % specifiers of the POSIX date
                       utility to refer to now, and $BRANCH to refer to the
                       built branch (Be sure to escape the dollar sign!).
                       (Default: %Y-%m-%d)
  --test-site-stopping
                     - Stop test site for last system test (On per default)
  --unit-testing     - Run unit tests on artifacts (On per default)
  --versions         - Print version information of helper programs (On per
                       default)

If the file ${DEFAULT_ARGUMENTS_FILE_RELS} exists in the script's directory, each of its lines
treated as argument before the arguments supplied on the command line. You can
use that file to for example use '--force' per default in your local dev
environment.
EOF
}

parse_arguments() {
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
            "--ignore-unit-tests" )
                [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
                while read LINE
                do
                    IGNORED_UNIT_TESTS+=( "$LINE" )
                done < <(tr ',' '\n' <<<"$1")
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
            "--code-coverage" )
                CODE_COVERAGE=yes
                TEST_UNIT=yes
                ;;
            "--documentation" )
                CODE_COVERAGE=yes
                GENERATE_JAVADOC=yes
                GENERATE_MANUAL=yes
                TEST_SYSTEM=yes
                TEST_UNIT=yes
                ;;
            "--jacoco-toolbox" )
                USE_JACOCO_TOOLBOX=yes
                ;;
            "--javadoc" )
                GENERATE_JAVADOC=yes
                ;;
            "--latest-link" )
                MANAGE_LATEST_LINK=yes
                ;;
            "--link-removing" )
                REMOVE_LINKS=yes
                ;;
            "--manual" )
                GENERATE_MANUAL=yes
                TEST_SYSTEM=yes
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
            "--no-code-coverage" )
                CODE_COVERAGE=no
                ;;
            "--no-documentation" )
                CODE_COVERAGE=no
                GENERATE_JAVADOC=no
                GENERATE_MANUAL=no
                ;;
            "--no-jacoco-toolbox" )
                USE_JACOCO_TOOLBOX=no
                ;;
            "--no-latest-link" )
                MANAGE_LATEST_LINK=no
                ;;
            "--no-link-removing" )
                REMOVE_LINKS=no
                ;;
            "--no-javadoc" )
                GENERATE_JAVADOC=no
                ;;
            "--no-manual" )
                GENERATE_MANUAL=no
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
                GENERATE_MANUAL=no
                ;;
            "--no-test-site-stopping" )
                STOP_TEST_SITE=no
                ;;
            "--no-testing" )
                TEST_UNIT=no
                TEST_SYSTEM=no
                GENERATE_MANUAL=no
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
                GENERATE_JAVADOC=no
                GENERATE_MANUAL=no
                MANAGE_LATEST_LINK=no
                PRINT_VERSIONS=no
                PULL=no
                REMOVE_LINKS=no
                STOP_TEST_SITE=no
                TEST_SYSTEM=no
                TEST_UNIT=no
                ;;
            "--only-artifact" )
                [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
                LIMIT_TO=("$1")
                shift || true
                ;;
            "--only-artifacts" )
                [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
                LIMIT_TO=()
                while read LINE
                do
                    LIMIT_TO+=( "$LINE" )
                done < <(tr ',' '\n' <<<"$1")
                shift || true
                ;;
            "--pull" )
                PULL=yes
                ;;
            "--system-testing" )
                TEST_SYSTEM=yes
                ;;
            "--system-testing-war" )
                [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
                SYSTEM_TESTING_WAR="$1"
                shift || true
                ;;
            "--target-directory-format" )
                [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
                TARGET_DIRECTORY_FORMAT="$1"
                shift || true
                ;;
            "--test-site-stopping" )
                STOP_TEST_SITE=yes
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
}

DEFAULT_ARGUMENTS=()
DEFAULT_ARGUMENTS_FILE_ABS="$SCRIPT_DIR_ABS/$DEFAULT_ARGUMENTS_FILE_RELS"
if [ -f "$DEFAULT_ARGUMENTS_FILE_ABS" ]
then
    readarray -t DEFAULT_ARGUMENTS <"$DEFAULT_ARGUMENTS_FILE_ABS"
    parse_arguments "${DEFAULT_ARGUMENTS[@]}"
fi

parse_arguments "$@"

TARGET_DIR_RELA="$(date --utc +"$TARGET_DIRECTORY_FORMAT")"
TARGET_DIR_RELA=${TARGET_DIR_RELA//\$BRANCH/$BRANCH}
TARGET_DIR_ABS="$ARTIFACTS_DIR_ABS/$TARGET_DIR_RELA"

if [ -z "$SYSTEM_TESTING_WAR" ]
then
    SYSTEM_TESTING_WAR_FILE_ABS="$TARGET_DIR_ABS/gerrit.war"
    # No early checking for war file existence, as this war file will be built
    # by this run (given that "--only-artifact" or other means are not used).
else
    if [ "${SYSTEM_TESTING_WAR:0:1}" = "/" ]
    then
        SYSTEM_TESTING_WAR_FILE_ABS="$SYSTEM_TESTING_WAR"
    else
        SYSTEM_TESTING_WAR_FILE_ABS="$ORIG_DIR_ABS/$SYSTEM_TESTING_WAR"
    fi
    # Early warning against war not existing
    if [ ! -e "$SYSTEM_TESTING_WAR_FILE_ABS" ]
    then
        error "The WAR for system testing ($SYSTEM_TESTING_WAR_FILE_ABS) does not exist"
    fi
fi

if [ -e "$TARGET_DIR_ABS" -a "$FORCE" = yes ]
then
    rm -rf "$TARGET_DIR_ABS"
fi

if [ -e "$TARGET_DIR_ABS" ]
then
    error "'$TARGET_DIR_ABS' already exists.
(You can use '--force' to force over-writing the directory)"
fi

mkdir -p "$TARGET_DIR_ABS"

if [ "$MANAGE_LATEST_LINK" = "yes" ]
then
    LATEST_LINK_FILE_ABS="$ARTIFACTS_DIR_ABS/latest"

    if [ -h "$LATEST_LINK_FILE_ABS" ]
    then
        rm "$LATEST_LINK_FILE_ABS"
    fi

    if [ ! -e "$LATEST_LINK_FILE_ABS" ]
    then
        ln -s "$TARGET_DIR_RELA" "$LATEST_LINK_FILE_ABS"
    fi
fi

post_parameter_parsing_setup

cat_manual_index_header() {
    if [ "$GENERATE_MANUAL" = "yes" ]
    then
        local TARGET_HTML_FILE_ABS="$MANUAL_INDEX_FILE_ABS"

        cat_html_header_target_html \
            "Manual for all artifacts" \
            "Manual for all artifacts of $TARGET_DIR_RELA gerrit $BRANCH build" \
            "gerrit, manual, $BRANCH" \
            "Manual for all artifacts of $TARGET_DIR_RELA gerrit $BRANCH build"

        cat_target_html <<EOF
<h2>Chapters</h2>

<ol>
EOF
    fi
}

echo_manual_index() {
    if [ "$GENERATE_MANUAL" = "yes" ]
    then
        local TARGET_HTML_FILE_ABS="$MANUAL_INDEX_FILE_ABS"

        echo_target_html "$@"
    fi
}

cat_manual_index_footer() {
    if [ "$GENERATE_MANUAL" = "yes" ]
    then
        local TARGET_HTML_FILE_ABS="$MANUAL_INDEX_FILE_ABS"

        echo_target_html "</ol>"

        cat_html_footer_target_html
    fi
}

generate_docu_index() {
    if [ "$GENERATE_MANUAL" = "yes" \
        -o "$GENERATE_JAVADOC" = "yes" \
        -o "$CODE_COVERAGE" = "yes" \
        ]
    then
        local TARGET_HTML_FILE_ABS="$TARGET_DIR_ABS/$DOCS_DIR_RELT/$INDEX_FILE_RELC"

        cat_html_header_target_html \
            "Documentation for $TARGET_DIR_RELA gerrit $BRANCH build" \
            "Documentation for $TARGET_DIR_RELA gerrit $BRANCH build" \
            "gerrit, documentation, $BRANCH" \
            "Documentation for $TARGET_DIR_RELA gerrit $BRANCH build"

        echo_target_html "<ol>"

        if [ "$GENERATE_MANUAL" = "yes" ]
        then
            echo_target_html "<li><a href=\"$MANUAL_DIR_RELD/$INDEX_FILE_RELC\">Manual</a></li>"
        fi

        if [ "$GENERATE_JAVADOC" = "yes" ]
        then
            echo_target_html "<li><a href=\"$JAVADOC_DIR_RELD\">Javadoc</a></li>"
        fi
        if [ "$CODE_COVERAGE" = "yes" ]
        then
            echo_target_html "<li><a href=\"$COVERAGE_DIR_RELD\">Unit test code coverage</a></li>"
        fi

        echo_target_html "</ol>"
        cat_html_footer_target_html
    fi
}

if [ "$GENERATE_MANUAL" = "yes" ]
then
    mkdir -p "$DOC_MANUAL_DIR_ABS"
fi
if [ "$GENERATE_JAVADOC" = "yes" ]
then
    mkdir -p "$DOC_JAVADOC_DIR_ABS"
fi
if [ "$CODE_COVERAGE" = "yes" ]
then
    mkdir -p "$DOC_COVERAGE_DIR_ABS"
fi

generate_docu_index

compute_checksums() {
    pushd "$TARGET_DIR_ABS" >/dev/null
    rm -f sha1sums.txt
    find -- * -maxdepth 1 -type f | xargs sha1sum >sha1sums.txt
    popd >/dev/null
}

dump_status() {
    echo "$STATUS" >"$TARGET_DIR_ABS/status.txt"
}

generate_overall_docs() {
    if [ "$GENERATE_MANUAL" = "yes" -o "$GENERATE_JAVADOC" = "yes" ]
    then
        cat_target_html <<EOF

<h2>Documentation</h2>

<ul>
EOF

        if [ "$GENERATE_MANUAL" = "yes" ]
        then
            echo_target_html "<li><a href=\"$MANUAL_DIR_RELT/$INDEX_FILE_RELC\">Manual across all artifacts</a></li>"
        fi

        if [ "$GENERATE_JAVADOC" = "yes" ]
        then
            add_all_plugin_links
            generate_javadoc "overall" "." "yes"
            echo_target_html "<li><a href=\"$JAVADOC_DIR_RELT/overall/index.html\">Javadoc across all artifacts</a></li>"
        fi
        echo_target_html "</ul>"
    fi
}

dump_status

info "Target directory: $TARGET_DIR_RELA"
info "Branch: $BRANCH"

set_target_html_file_abs "$TARGET_DIR_ABS/index.html"

cat_html_header_target_html \
    "$TARGET_DIR_RELA gerrit $BRANCH build" \
    "Build of $BRANCH commitish of gerrit for $TARGET_DIR_RELA" \
    "gerrit, jar, $BRANCH" \
    "$TARGET_DIR_RELA build of $BRANCH of gerrit"

cat_target_html <<EOF
<h2 id="summary">Build summary</h2>

<div class="tablerow">

<h3>Key indicators</h3>
<table>
<tr class="$STATUS"><th class="th-$STATUS">Build status</th><td><img src="$IMAGE_BASE_URL/$STATUS.png" alt="Build $STATUS" />&#160;$STATUS</td></tr>
<tr><th>Build start</th><td>$(timestamp)</td></tr>
<tr><th>Build end</th><td>---</td></tr>
<tr><th>Commitish</th><td>$BRANCH</td></tr>
<tr><th>Description</th><td>---</td></tr>
<tr><th>API version</th><td>---</td></tr>
<tr><th>DB schema version</th><td>---</td></tr>
</table>

<h3>Artifacts</h3>
<!-- ARTIFACTS_TABLE -->

</div>

<h2 id="artifacts">Artifacts</h2>

<p>-
EOF

for ARTIFACT_GROUP in \
    war \
    api \
    bundled \
    separate \
    info \

do
    echo_target_html "<a href=\"#group-${ARTIFACT_GROUP}\">$(echo_artifact_group_name "$ARTIFACT_GROUP")</a> -"
done

echo_target_html "</p>"


kill_old_daemons() {
    checked_kill watchman "$SCRIPT_DIR_ABS"
    checked_kill java "$SCRIPT_DIR_ABS"
}

kill_old_daemons

describe_repo

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
    if [ -e ".gitmodules" ]
    then
        run_git submodule update --recursive
    fi
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
    if [ "$REMOVE_LINKS" = "yes" ]
    then
        for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
        do
            local EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"
            if [ -h "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME" ]
            then
                rm "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME"
            fi
        done
    fi
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

add_all_plugin_links() {
    for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
    do
        EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"

        # We skip early, to not remove the plugin link again, if only a
        # single plugin is getting built.
        if [ "${#LIMIT_TO[@]}" != "0" ]
        then
            if ! in_array "$EXTRA_PLUGIN_NAME.jar" "${LIMIT_TO[@]}"
            then
                continue
            fi
        fi

        add_plugin_link "$EXTRA_PLUGIN_NAME"
    done
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
    if [ -d "$PLUGIN_DIR_ABS" -a ! -h "$PLUGIN_DIR_ABS" ]
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
        echo -n "${ARTIFACT_GROUP_STATUS_COUNT[$ARTIFACT_GROUP]}"
        if [ "$ARTIFACT_GROUP" = "total" ]
        then
            case "${ARTIFACT_GROUP_STATUS[$ARTIFACT_GROUP]}" in
                "failed" )
                    echo -n ",$FIRST_FAILED_ARTIFACT"
                    ;;
                "broken" )
                    echo -n ",$FIRST_BROKEN_ARTIFACT"
                    ;;
            esac
        fi
        echo
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

cat_manual_index_header

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
    if [ -d "$PLUGIN_DIR_ABS" -a ! -h "$PLUGIN_DIR_ABS" ]
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
RELEASE_WAR_INSERT_BEFORE=
for RELEASE_WAR_INSERT_BEFORE_CANDIDATE in \
    "gerrit.war" \
    "withdocs.war" \

do
    if [ -z "$RELEASE_WAR_INSERT_BEFORE" ]
    then
        if grep -q 'Artifact: '"$RELEASE_WAR_INSERT_BEFORE_CANDIDATE" "$TARGET_HTML_FILE_ABS"
        then
            RELEASE_WAR_INSERT_BEFORE="$RELEASE_WAR_INSERT_BEFORE_CANDIDATE"
        fi
    fi
done
run_buck_build "gerrit, release.war" "//:release" "release.war" "war" "$RELEASE_WAR_INSERT_BEFORE"


# Building extra plugins
for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
do
    EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"

    # We skip early, to not remove the plugin link again, if only a
    # single plugin is getting built.
    if [ "${#LIMIT_TO[@]}" != "0" ]
    then
        if ! in_array "$EXTRA_PLUGIN_NAME.jar" "${LIMIT_TO[@]}"
        then
            continue
        fi
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
    local ARTIFACT_TO_LINK="$3"

    local STATUS_TEXT=
    set_STATUS_TEXT "uncounted"

    local ATTRIBUTE=
    if [ "$COUNT" -gt 0 ]
    then
        ATTRIBUTE=" class=\"$STATUS\""
    fi

    local LINK_START=
    local LINK_END=
    if [ -n "$ARTIFACT_TO_LINK" ]
    then
        LINK_START="<a href=\"#$ARTIFACT_TO_LINK\">"
        LINK_END="</a>"
    fi
    CONTENT="$CONTENT\n  <tr><td$ATTRIBUTE>$LINK_START${STATUS_TEXT^}$LINK_END</td><td$ATTRIBUTE>$LINK_START$COUNT$LINK_END</td></tr>"
}

cat_artifacts_summary_target_html() {
    local CONTENT="<table>\n  <tr><th>Artifacts</th><th>Count</th></tr>"

    cat_artifacts_summary_row_target_html "failed" "$ARTIFACTS_FAILED" "$FIRST_FAILED_ARTIFACT"
    cat_artifacts_summary_row_target_html "broken" "$ARTIFACTS_BROKEN" "$FIRST_BROKEN_ARTIFACT"
    cat_artifacts_summary_row_target_html "ok" "$ARTIFACTS_OK"
    cat_artifacts_summary_row_target_html "total" "$ARTIFACTS_TOTAL"

    CONTENT="$CONTENT\n</table>"

    CONTENT="${CONTENT//&/\\&}"

    sed -i \
        -e '/<!-- ARTIFACTS_TABLE -->/s@<!-- ARTIFACTS_TABLE -->@'"$CONTENT"'@g' \
        "$TARGET_HTML_FILE_ABS"
}

cat_artifacts_summary_target_html

cat_manual_index_footer

generate_overall_docs

rm -rf "$JAVADOC_CLASSPATH_DIR_ABS"

if [ "$PRINT_VERSIONS" = "yes" ]
then
    FORMATTED_ARGUMENTS=( \
        "${DEFAULT_ARGUMENTS[@]}" \
        "${SCRIPT_ARGUMENTS[@]}" \
        )
    if [ "${#FORMATTED_ARGUMENTS[@]}" = 0 ]
    then
        FORMATTED_ARGUMENTS="<em>&lt;none&gt;</em>"
    fi
    cat_target_html <<EOF

<h2>Build environment</h2>

<table>
  <tr><th>Hostname</th><td>$(hostname --fqdn)</td></tr>
  <tr><th>Ant</th><td>$(ant -version | head -n 1 | sed -e 's/^.*version \(.*\) compiled.*/\1/')</td></tr>
  <tr><th>Buck</th><td>$(buck --version 2>/dev/null | head -n 1 | sed -e 's/^.* version //')</td></tr>
  <tr><th>Build parameters</th><td>${FORMATTED_ARGUMENTS[@]}</td></tr>
  <tr><th>Build script </th><td>${REPO_DESCRIPTIONS["gerrit-builder"]}</td></tr>
EOF
if [ "$CODE_COVERAGE" = "yes" -a "$USE_JACOCO_TOOLBOX" = "yes" ]
then
    echo_target_html "<tr><th>JaCoCo Toolbox</th><td>$(run_jacoco_toolbox version | head -n 2 | sed -e 's/JaCoCo Toolbox //' -e 's/^.*using \(commit [0-9a-fA-F]*\) .*/ \(\1\)/' | tr -d '\n')</td></tr>"
fi
    cat_target_html <<EOF
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

"$SCRIPT_DIR_ABS/write_overview_index_htmls.sh"

kill_old_daemons

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

info "Final status: $STATUS"

finalize "$EXIT_CODE"
