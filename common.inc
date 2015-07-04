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
JACOCO_TOOLBOX_DIR_ABS="$SCRIPT_DIR_ABS/jacoco-toolbox"
declare -A JACOCO_TOOLBOX_EXEC_FILES_ABS
declare -A JACOCO_TOOLBOX_CLASS_FILES_ABS
declare -A JACOCO_TOOLBOX_SOURCE_FILES_ABS

EXTRA_PLUGINS_DIR_ABS="$SCRIPT_DIR_ABS/plugins"

BUILDS_DIR_ABS="$SCRIPT_DIR_ABS/builds"

TEST_SITE_DIR_ABS="$SCRIPT_DIR_ABS/test-site"
TEST_SITE_HOST="127.0.0.1"
TEST_SITE_HTTP_PORT="8089"
TEST_SITE_SSH_PORT="29419"
TEST_SITE_URL="http://$TEST_SITE_HOST:$TEST_SITE_HTTP_PORT"
TEST_SITE_USER_CREDENTIALS_DIR_ABS="$TEST_SITE_DIR_ABS/user-credentials"
TEST_SITE_GERRIT_RUN_FILE_ABS="$TEST_SITE_DIR_ABS/logs/gerrit.run"

USER_CREDENTIALS_DIR_ABS="$SCRIPT_DIR_ABS/user-credentials"

ANT_HOME="$ANT_DIR_ABS"
JAVA_HOME="$JAVA_DIR_ABS"

INDEX_FILE_RELC="index.html"
DOCS_DIR_RELT="docs"
MANUAL_DIR_RELD="manual"
MANUAL_DIR_RELT="$DOCS_DIR_RELT/$MANUAL_DIR_RELD"
declare -A DOC_MANUAL_FILE_RELT=()
declare -A REUSE_MANUAL_FROM=()
JAVADOC_DIR_RELD="javadoc"
JAVADOC_DIR_RELT="$DOCS_DIR_RELT/$JAVADOC_DIR_RELD"
JAVADOC_CLASSPATH_DIR_RELJ="classpath"
declare -A REUSE_JAVADOC_FROM=()
COVERAGE_DIR_RELD="coverage"
COVERAGE_DIR_RELT="$DOCS_DIR_RELT/$COVERAGE_DIR_RELD"
declare -A REUSE_UNIT_TESTS_FROM=()
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

DEFAULT_EXTERNAL_PLUGINS=()
add_to_plugins() {
    local ADDITION="$1"
    DEFAULT_EXTERNAL_PLUGINS=( "${DEFAULT_EXTERNAL_PLUGINS[@]}" "$ADDITION" )
}
add_to_plugins "admin-console"
add_to_plugins "avatars/external"
add_to_plugins "branch-network"
add_to_plugins "codenvy"
add_to_plugins "delete-project"
add_to_plugins "events-log"
add_to_plugins "force-draft"
add_to_plugins "importer"
add_to_plugins "its-base"
add_to_plugins "its-bugzilla"
add_to_plugins "its-jira"
add_to_plugins "its-phabricator"
add_to_plugins "its-rtc"
add_to_plugins "its-storyboard"
add_to_plugins "motd"
add_to_plugins "quota"
add_to_plugins "rabbitmq"
add_to_plugins "ref-protection"
add_to_plugins "reviewers"
add_to_plugins "reviewers-by-blame"
add_to_plugins "server-config"
add_to_plugins "serviceuser"
add_to_plugins "uploadvalidator"
add_to_plugins "websession-flatfile"
add_to_plugins "wip"
add_to_plugins "x-docs"

IGNORED_PLUGINS=()

ARTIFACTS_TOTAL=0
ARTIFACTS_OK=0
ARTIFACTS_BROKEN=0
ARTIFACTS_FAILED=0

REPO_NAMES=()
LAST_REPO_NAME=
declare -A REPO_DESCRIPTIONS=()
declare -A REPO_ARTIFACTS=()

declare -A TARGET_TEST_LABELS=()

declare -A ARTIFACT_STATUS=()
declare -A ARTIFACT_GROUP_STATUS=()
declare -A ARTIFACT_GROUP_STATUS_COUNT=()
declare -A ARTIFACT_GROUP_TOTAL_COUNT=()

ARTIFACTS_WAR=( \
    "gerrit.war" \
    "withdocs.war" \
    "release.war" \
)
ARTIFACTS_API=( \
    "extension-api.jar" \
    "extension-api-src.jar" \
    "extension-api-javadoc.jar" \
    "plugin-api.jar" \
    "plugin-api-src.jar" \
    "plugin-api-javadoc.jar" \
    "gwtui-api.jar" \
    "gwtui-api-src.jar" \
    "gwtui-api-javadoc.jar" \
    "api.zip" \
)
#---------------------------------------------------------------------
JAVA_VERSION="$(java -version 2>&1 | head -n 1 | cut -f 2 -d .)"
if [ "$JAVA_VERSION" != "7" \
    -a "$JAVA_VERSION" != "8" \
    -a "$JAVA_VERSION" != "9" ]
then
    error "You're Java did not identify as either Java 7, 8, 9"
fi

#---------------------------------------------------------------------
mkdir -p "$BUILDS_DIR_ABS"

#---------------------------------------------------------------------
post_parameter_parsing_setup() {
    DOC_MANUAL_DIR_ABS="$TARGET_DIR_ABS/$MANUAL_DIR_RELT"
    MANUAL_INDEX_FILE_ABS="$DOC_MANUAL_DIR_ABS/$INDEX_FILE_RELC"
    DOC_JAVADOC_DIR_ABS="$TARGET_DIR_ABS/$JAVADOC_DIR_RELT"
    JAVADOC_CLASSPATH_DIR_ABS="$DOC_JAVADOC_DIR_ABS/$JAVADOC_CLASSPATH_DIR_RELJ"
    DOC_COVERAGE_DIR_ABS="$TARGET_DIR_ABS/$COVERAGE_DIR_RELT"
}

# Calling once already upon sourcing, put scripts are encouraged to
# call it again once they are finished with parameter parsing
post_parameter_parsing_setup

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
is_ignored_plugin () {
    local PLUGIN_NAME="$1"
    if in_array "$PLUGIN_NAME" "${IGNORED_PLUGINS[@]}"
    then
        return 0
    fi
    return 1
}

#---------------------------------------------------------------------
is_war_artifact () {
    local ARTIFACT_BASENAME="$1"
    if in_array "$ARTIFACT_BASENAME" "${ARTIFACTS_WAR[@]}"
    then
        return 0
    fi
    return 1
}

#---------------------------------------------------------------------
is_api_artifact () {
    local ARTIFACT_BASENAME="$1"
    if in_array "$ARTIFACT_BASENAME" "${ARTIFACTS_API[@]}"
    then
        return 0
    fi
    return 1
}

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

    if [ "$(basename "$CURRENT_REPO_NAME")" = "gerrit-builder" ]
    then
       CURRENT_REPO_NAME="gerrit-builder"
    elif [ "${CURRENT_REPO_NAME:0:8}" = "plugins/" ]
    then
        local REPO_DIR_RELS
        plugin_name_to_REPO_DIR_RELS "$CURRENT_REPO_NAME"
        CURRENT_REPO_NAME="$REPO_DIR_RELS"
    fi

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
run_jacoco_toolbox() {
    local JACOCO_TOOLBOX_JAR_FILE_ABS="$(find "$JACOCO_TOOLBOX_DIR_ABS/toolbox/target" -maxdepth 1 -name "jacoco-toolbox-*.jar" | head -n 1)"
    if [ -e "$JACOCO_TOOLBOX_JAR_FILE_ABS" ]
    then
        java -jar "$JACOCO_TOOLBOX_JAR_FILE_ABS" \
            "$@" \
            --verbose
    else
        error "Could not find a JaCoCo Toolbox jar. Please make sure that '$JACOCO_TOOLBOX_JAR_FILE_ABS' exists, or use the parameter '--no-jacoco-toolbox' to turn off using the JaCoCo Toolbox."
    fi
}

#---------------------------------------------------------------------
run_jacoco_toolbox_report_no_versioning() {
    local ARTIFACT_BASENAME="$1"
    local ARTIFACT_GROUP="$2"
    echo "Generating coverage report using JaCoCo toolbox"

    local REPORT_TITLE="Code coverage analysis for "
    case "$ARTIFACT_GROUP" in
        "war" )
            REPORT_TITLE="$REPORT_TITLE Gerrit core"
            ;;
        "bundled" | "separate" )
            REPORT_TITLE="$REPORT_TITLE '$(plugin_jar_to_name "$ARTIFACT_BASENAME")' plugin"
            ;;
        "overall" )
            REPORT_TITLE="Code coverage analysis across all artifacts"
            ;;
        * )
            REPORT_TITLE="$REPORT_TITLE $ARTIFACT_BASENAME"
            ;;
    esac
    run_jacoco_toolbox \
        report-html \
        --input "${JACOCO_TOOLBOX_EXEC_FILES_ABS["$ARTIFACT_BASENAME"]}" \
        --analyze-for "${JACOCO_TOOLBOX_CLASS_FILES_ABS["$ARTIFACT_BASENAME"]}" \
        --source "${JACOCO_TOOLBOX_SOURCE_FILES_ABS["$ARTIFACT_BASENAME"]}" \
        --title "$REPORT_TITLE" \
        --output "$TARGET_DIR_ABS/$COVERAGE_DIR_RELT/$ARTIFACT_BASENAME"
}

#---------------------------------------------------------------------
run_jacoco_toolbox_report() {
    local ARTIFACT_BASENAME="$1"
    local ARTIFACT_GROUP="$2"

    local TARGET_FILE_ABS="$TARGET_DIR_ABS/$ARTIFACT_BASENAME"
    local ASPECT="coverage"

    if ! run_jacoco_toolbox_report_no_versioning "$ARTIFACT_BASENAME" "$ARTIFACT_GROUP"
    then
        if grep --quiet "version" "$TARGET_FILE_ABS.$ASPECT.stderr.txt"
        then
            info "Generating report using JaCoCo Toolbox failed."
            info "JaCoCo version did not match. Trying to up-/downgrade..."
            local EXEC_VERSION=$(run_jacoco_toolbox \
                identify \
                --input "${JACOCO_TOOLBOX_EXEC_FILES_ABS["$ARTIFACT_BASENAME"]}")
            EXEC_VERSION=${EXEC_VERSION##*,}
            pushd "$JACOCO_TOOLBOX_DIR_ABS" >/dev/null
            run_git clean -f
            run_git checkout "format-$EXEC_VERSION"
            mvn clean package
            popd >/dev/null

            if ! run_jacoco_toolbox_report_no_versioning "$ARTIFACT_BASENAME" "$ARTIFACT_GROUP"
            then
                error "Generating report using JaCoCo Toolbox failed"
            fi
        else
            error "Generating report using JaCoCo Toolbox failed"
        fi
    fi
}

#---------------------------------------------------------------------
run_jacoco_toolbox_report_logged() {
    local ARTIFACT_BASENAME="$1"
    local TARGET_FILE_ABS="$TARGET_DIR_ABS/$ARTIFACT_BASENAME"

    run_jacoco_toolbox_report "$@" 2> >(tee "$TARGET_FILE_ABS.coverage.stderr.txt" >&2) | tee "$TARGET_FILE_ABS.coverage.stdout.txt"
}
#---------------------------------------------------------------------
run_buck_logged() {
    local ASPECT="$1"
    # no shift ^, as the buck command (build/test) is getting used as
    # ASPECT.

    local RETURN_CODE=0

    local LOG_FILE_ABS="$GERRIT_DIR_ABS/buck-out/log/buck-0.log"
    local TRACE_FILE_ABS="$GERRIT_DIR_ABS/buck-out/log/traces/build.trace"

    rm -f "$LOG_FILE_ABS"
    rm -f "$TRACE_FILE_ABS"

    if ! run_buck "$@" 2> >(tee "$TARGET_FILE_ABS.$ASPECT.stderr.txt" >&2) | tee "$TARGET_FILE_ABS.$ASPECT.stdout.txt"
    then
        RETURN_CODE=1
    fi

    if [ -e "$LOG_FILE_ABS" ]
    then
        cp "$LOG_FILE_ABS" "$TARGET_FILE_ABS.$ASPECT.buck_log.txt"
    fi
    if [ -e "$TRACE_FILE_ABS" ]
    then
        cp "$TRACE_FILE_ABS" "$TARGET_FILE_ABS.$ASPECT.buck_trace.txt"
    fi
    return "$RETURN_CODE"
}

#---------------------------------------------------------------------
status_to_number () {
    local STATUS="$1"
    local VALUE=1000
    case "$STATUS" in
        "" )
            VALUE=0
            ;;
        "ok" )
            VALUE=10
            ;;
        "broken" )
            VALUE=20
            ;;
        "failed" )
            VALUE=30
            ;;
    esac
    echo "$VALUE"
}

#---------------------------------------------------------------------
update_artifact_group_statistics () {
    local STATUS="$1"
    local ARTIFACT_GROUP="$2"

    local GROUP_STATUS="${ARTIFACT_GROUP_STATUS["$ARTIFACT_GROUP"]}"

    if [ "$(status_to_number "$STATUS")" -gt "$(status_to_number "$GROUP_STATUS")" ]
    then
        ARTIFACT_GROUP_STATUS["$ARTIFACT_GROUP"]="$STATUS"
        ARTIFACT_GROUP_STATUS_COUNT["$ARTIFACT_GROUP"]=0
        GROUP_STATUS="$STATUS"
    fi

    if [ "$STATUS" = "$GROUP_STATUS" ]
    then
        ARTIFACT_GROUP_STATUS_COUNT["$ARTIFACT_GROUP"]=$((ARTIFACT_GROUP_STATUS_COUNT["$ARTIFACT_GROUP"]+1))
    fi

    ARTIFACT_GROUP_TOTAL_COUNT["$ARTIFACT_GROUP"]=$((ARTIFACT_GROUP_TOTAL_COUNT["$ARTIFACT_GROUP"]+1))

    if [ "$ARTIFACT_GROUP" != "total" ]
    then
        update_artifact_group_statistics "$STATUS" "total"
    fi
}

#---------------------------------------------------------------------
plugin_jar_to_name() {
    local PLUGIN_JAR_FILE_ABS="$1"
    local PLUGIN_JAR_BASENAME="$(basename "$PLUGIN_JAR_FILE_ABS")"
    local PLUGIN_NAME=
    case "$PLUGIN_JAR_BASENAME" in
        "cookbook-plugin.jar" )
            PLUGIN_NAME="cookbook"
            ;;
        "delete-project.jar" )
            PLUGIN_NAME="deleteproject"
            ;;
        * )
            PLUGIN_NAME="${PLUGIN_JAR_BASENAME:0:-4}"
            ;;
    esac
    echo "$PLUGIN_NAME"
}



#---------------------------------------------------------------------
dump_manual() {
    local ARTIFACT_BASENAME="$1"
    local DESCRIPTION="$2"
    shift 2
    local URLS_RELS=( "$@" )

    pushd "$DOC_MANUAL_DIR_ABS" >/dev/null

    info "Dumping manual at ${URL_RELS[@]}"

    local URLS=()
    for URL_RELS in "${URLS_RELS[@]}"
    do
        URLS+=( "http://$TEST_SITE_HOST:$TEST_SITE_HTTP_PORT/$URL_RELS" )
    done

    if ! wget \
        --no-host-directories \
        --recursive \
        --level=inf \
        --page-requisites \
        --no-parent \
        "${URLS[@]}"
    then
        info "Dumping manual recursively at $URL_RELS failed"
    fi

    # Finally, replace the hostname with 'gerrit.example.org', so documentation
    # is more generic, and less tuned to the build host.
    local HOSTNAME="$(hostname --fqdn)"
    local HOSTNAME_ESCAPED="${HOSTNAME//./\.}"
    for URL_RELS in "${URLS_RELS[@]}"
    do
        if [ -e "$URL_RELS" ]
        then
            find "$(dirname "$URL_RELS")" -type f \
                | xargs sed -i -e "s/${HOSTNAME_ESCAPED}/gerrit.example.org/g"
        fi
    done

    echo_manual_index "<li><a href=\"${URLS_RELS[0]}\">$DESCRIPTION</a></li>"

    DOC_MANUAL_FILE_RELT["$ARTIFACT_BASENAME"]="$MANUAL_DIR_RELT/${URLS_RELS[0]}"

    popd >/dev/null
}



#---------------------------------------------------------------------
run_system_test() {
    local ARTIFACT_BASENAME="$1"
    local ARTIFACT_GROUP="$2"
    local TEST_SITE_STARTED="no"

    local TARGET_FILE_ABS="$TARGET_DIR_ABS/$ARTIFACT_BASENAME"

    local GERRIT_WAR_FILE_ABS=""
    local PLUGIN_JAR_FILE_ABS=""
    case "${ARTIFACT_GROUP}" in
        "war")
            GERRIT_WAR_FILE_ABS="$TARGET_FILE_ABS"
            PLUGIN_JAR_FILE_ABS=""
            ;;
        "bundled" | \
            "separate" )
            GERRIT_WAR_FILE_ABS="$SYSTEM_TESTING_WAR_FILE_ABS"
            PLUGIN_JAR_FILE_ABS="$TARGET_FILE_ABS"
            ;;
    esac

    local RETURN_CODE=0

    if [ -e "$GERRIT_WAR_FILE_ABS" ]
    then
        local RUN_ID="$(date +%s).$$"

        "$SCRIPT_DIR_ABS"/stop_test_site.sh
        rm -rf "$TEST_SITE_DIR_ABS"
        if [ -e "$TEST_SITE_DIR_ABS" ]
        then
            info "Despite removing $TEST_SITE_DIR_ABS, the directory still exists"
            RETURN_CODE=2
        else
            info "Trying to start test-site (this may take a minute)"
            # Not doing tee redirection on stdout and stderr, as the
            # boot_fresh_test_site.sh is spawning a background
            # process, which got the tees hung and blocking here.
            if "$SCRIPT_DIR_ABS"/boot_fresh_test_site.sh "$GERRIT_WAR_FILE_ABS" "$RUN_ID" </dev/null 2>"$TARGET_FILE_ABS.system.stderr.txt" >"$TARGET_FILE_ABS.system.stdout.txt"
            then
                TEST_SITE_STARTED="yes"

                if [ -n "$PLUGIN_JAR_FILE_ABS" ]
                then
                    if [ -e "$PLUGIN_JAR_FILE_ABS" ]
                    then
                        local PLUGIN_NAME="$(plugin_jar_to_name "$PLUGIN_JAR_FILE_ABS")"
                        if "$SCRIPT_DIR_ABS"/ssh_test-site.sh admin gerrit plugin install --name "${PLUGIN_NAME}.jar" - < "$PLUGIN_JAR_FILE_ABS" 2>>"$TARGET_FILE_ABS.system.stderr.txt" >>"$TARGET_FILE_ABS.system.stdout.txt"
                        then
                            if [ "$("$SCRIPT_DIR_ABS"/ssh_test-site.sh admin gerrit plugin ls | cut -f 1 -d ' ' | tail -n 1)" = "$PLUGIN_NAME" ]
                            then
                                info "Plugin loaded and listed as '$PLUGIN_NAME'"
                                info "Plugin loaded and listed as '$PLUGIN_NAME'" 2>>"$TARGET_FILE_ABS.system.stdout.txt"
                            else
                                info "Plugin loaded, but was not listed as '$PLUGIN_NAME'"
                                info "Plugin loaded, but was not listed as '$PLUGIN_NAME'" 2>>"$TARGET_FILE_ABS.system.stdout.txt"
                                RETURN_CODE=1
                            fi

                            if [ "$GENERATE_MANUAL" = "yes" ]
                            then
                                local MANUAL_CHAPTER_LABEL="Plugin: $PLUGIN_NAME"
                                case "$ARTIFACT_GROUP" in
                                    "bundled" )
                                        MANUAL_CHAPTER_LABEL="Bundled plugin: $PLUGIN_NAME"
                                        ;;
                                    "separate" )
                                        MANUAL_CHAPTER_LABEL="Separate plugin: $PLUGIN_NAME"
                                        ;;
                                esac
                                dump_manual \
                                    "$ARTIFACT_BASENAME" \
                                    "$MANUAL_CHAPTER_LABEL" \
                                    "plugins/$PLUGIN_NAME/Documentation/index.html" \
                                    2> >(tee "$TARGET_FILE_ABS.manual.stderr.txt" >&2) \
                                    > >(tee "$TARGET_FILE_ABS.manual.stdout.txt")
                            fi
                        else
                            info "Failed to install plugin"
                            info "Failed to install plugin" 2>>"$TARGET_FILE_ABS.system.stdout.txt"
                            RETURN_CODE=1
                        fi
                    else
                        info "Plugin $PLUGIN_JAR_FILE_ABS does not exist"
                        info "Plugin $PLUGIN_JAR_FILE_ABS does not exist" 2>>"$TARGET_FILE_ABS.system.stdout.txt"
                        RETURN_CODE=1
                    fi
                elif is_war_artifact "$ARTIFACT_BASENAME"
                then
                    if [ "$GENERATE_MANUAL" = "yes" \
                        -a "$ARTIFACT_BASENAME" != "gerrit.war" \
                        -a -z "${REUSE_MANUAL_FROM[$ARTIFACT_BASENAME]}" ]
                    then
                        dump_manual \
                            "$ARTIFACT_BASENAME" \
                            "Gerrit core" \
                            "Documentation/index.html" \
                            "Documentation/images/link.png" \
                            2> >(tee "$TARGET_FILE_ABS.manual.stderr.txt" >&2) \
                            > >(tee "$TARGET_FILE_ABS.manual.stdout.txt")

                        local WAR_ARTIFACT=
                        for WAR_ARTIFACT in "${ARTIFACTS_WAR[@]}"
                        do
                            if [ -z "${REUSE_MANUAL_FROM[$WAR_ARTIFACT]}" ]
                            then
                                REUSE_MANUAL_FROM["$WAR_ARTIFACT"]="$ARTIFACT_BASENAME"
                            fi
                        done
                    fi
                fi
            else
                info "Gerrit did not come up"
                info "Gerrit did not come up" 2>>"$TARGET_FILE_ABS.system.stdout.txt"
                echo "===stdout==="
                cat "$TARGET_FILE_ABS.system.stdout.txt"
                echo "===stderr==="
                cat "$TARGET_FILE_ABS.system.stderr.txt" >&2
                echo "============"
                RETURN_CODE=1
            fi
        fi
    elif [ -n "$GERRIT_WAR_FILE_ABS" ]
    then
        # $GERRIT_WAR_FILE_ABS has a value, but the file does not exist
        if [ ! -e "$GERRIT_WAR_FILE_ABS" ]
        then
            error "The WAR for system testing ($GERRIT_WAR_FILE_ABS) does \
not exist. (You can specify a WAR file using the --system-testing-war \
argument)"
        fi
    fi

    if [ "$RETURN_CODE" != "2" ]
    then
        for LOG_FILE_RELL in \
            error_log \
            gc_log \
            httpd_log \
            replication_log \
            sshd_log \

        do
            if [ -e "$TEST_SITE_DIR_ABS/logs/$LOG_FILE_RELL" ]
            then
                cp "$TEST_SITE_DIR_ABS/logs/$LOG_FILE_RELL" "$TARGET_FILE_ABS.system.$LOG_FILE_RELL.txt"
            fi
        done
    fi

    if [ "$TEST_SITE_STARTED" = "yes" -a "$STOP_TEST_SITE" = "yes" ]
    then
        "$SCRIPT_DIR_ABS"/stop_test_site.sh
        rm -rf "$TEST_SITE_DIR_ABS"
    fi

    return "$RETURN_CODE"
}



#---------------------------------------------------------------------
generate_javadoc() {
    ARTIFACT_BASENAME="$1"
    REPO_DIR_RELG="$2"
    WITH_PLUGINS="$3"

    local SRC_DIR_RELG_CANDIDATES=( \
        "$REPO_DIR_RELG"/src \
        "$REPO_DIR_RELG"/*/src \
        )
    if [ "$WITH_PLUGINS" = "yes" ]
    then
        SRC_DIR_RELG_CANDIDATES+=( \
            "$REPO_DIR_RELG"/plugins/*/src \
            "$REPO_DIR_RELG"/plugins/*/*/src \
            )
    fi

    local SRC_DIRS_RELG=()
    local SRC_DIR_RELG_CANDIDATE
    for SRC_DIR_RELG_CANDIDATE in "${SRC_DIR_RELG_CANDIDATES[@]}"
    do
        if [ -d "$SRC_DIR_RELG_CANDIDATE" ]
        then
            SRC_DIRS_RELG+=( "$SRC_DIR_RELG_CANDIDATE" )
        fi
    done

    if [ "${#SRC_DIRS_RELG[@]}" = "0" ]
    then
        # No source directories. We won't get meaningful javadoc. So return
        return
    fi

    local JAVA_SOURCE_DIRS_RELG=$(
        find \
            "${SRC_DIRS_RELG[@]}" \
            -type d -name 'java' \
            ! -regex '.*/test/java' \
            2>/dev/null \
            | sort -u \
            | tr '\n' ':'
    )

    local PACKAGES=$(
        find \
            "${SRC_DIRS_RELG[@]}" \
            -type f -name '*.java' \
            ! -regex '.*/test/java/.*' \
            -execdir grep --no-filename '^package \([a-z][^ ;]*\) *;' {} + \
            2>/dev/null \
            | sed -e 's/^package \([^ ;]*\) *;.*/\1/' \
            | sort -u \
            | tr '\n' ':'
    )

    local TARGET_FILE_ABS="$TARGET_DIR_ABS/$ARTIFACT_BASENAME"

    # Building shared classpath with classes from current artifact and (if
    # needed) gerrit.war. This is needed to have javadoc find all class files,
    # the current artifact sources depend on, which is required to render all
    # implemented interfaces and so on.
    mkdir -p "$JAVADOC_CLASSPATH_DIR_ABS"
    pushd "$JAVADOC_CLASSPATH_DIR_ABS" >/dev/null
    if [ -e "$TARGET_DIR_ABS/gerrit.war" -a ! -d "com/google/gerrit/server" ]
    then
        info "Seeding shared classpath for javadoc (this may take a minute)"
        jar xf "$TARGET_DIR_ABS/gerrit.war"
        find WEB-INF -name '*.jar' -exec jar xf {} \;
    fi
    if [ -e "$TARGET_FILE_ABS" -a "${TARGET_FILE_ABS: -4}" != ".war" ]
    then
        jar xf "$TARGET_FILE_ABS"
    fi
    popd >/dev/null

    local TITLE="$(basename "$ARTIFACT_BASENAME") Documentation"
    if [ "$TITLE" = "overall Documentation" ]
    then
        TITLE="Gerrit Documentation"
    fi

    javadoc \
        -protected \
        -encoding UTF-8 \
        -charset UTF-8 \
        -notimestamp \
        -windowtitle "$TITLE" \
        -sourcepath "$JAVA_SOURCE_DIRS_RELG" \
        -link "http://docs.oracle.com/javase/${JAVA_VERSION}/docs/api/" \
        -subpackages "$PACKAGES" \
        -classpath "$JAVADOC_CLASSPATH_DIR_ABS" \
        -d "$DOC_JAVADOC_DIR_ABS/$ARTIFACT_BASENAME" \
        2> >(tee "$TARGET_FILE_ABS.javadoc.stderr.txt" >&2) \
        | tee "$TARGET_FILE_ABS.javadoc.stdout.txt" \
        || true
}



#---------------------------------------------------------------------
run_unit_test() {
    local ARTIFACT_BASENAME="$1"
    local ARTIFACT_GROUP="$2"
    local BUCK_TARGET="$3"

    local TARGET_FILE_ABS="$TARGET_DIR_ABS/$ARTIFACT_BASENAME"

    local BUCK_TEST_EXTRA_OPTS=()
    local RUN_UNIT_TESTS=no
    if [ -n "${TARGET_TEST_LABELS["$BUCK_TARGET"]}" ]
    then
        BUCK_TEST_EXTRA_OPTS+=( \
            "--include" "${TARGET_TEST_LABELS["$BUCK_TARGET"]}" \
            )
        RUN_UNIT_TESTS=yes
    elif is_war_artifact "$ARTIFACT_BASENAME"
    then
        local TARGET_TEST_LABEL=
        for TARGET_TEST_LABEL in "${TARGET_TEST_LABELS[@]}"
        do
            if [ -n "$TARGET_TEST_LABEL" ]
            then
                BUCK_TEST_EXTRA_OPTS+=( \
                    "--exclude" "$TARGET_TEST_LABEL" \
                    )
            fi
        done
        RUN_UNIT_TESTS=yes

        local WAR_ARTIFACT=
        for WAR_ARTIFACT in "${ARTIFACTS_WAR[@]}"
        do
            if [ -z "${REUSE_UNIT_TESTS_FROM[$WAR_ARTIFACT]}" ]
            then
                REUSE_UNIT_TESTS_FROM["$WAR_ARTIFACT"]="$ARTIFACT_BASENAME"
            fi
        done
    fi

    if [ "$RUN_UNIT_TESTS" = "yes" ]
    then
        local CODE_COVERAGE_DIR_ABS="$GERRIT_DIR_ABS/buck-out/gen/jacoco"
        local CODE_COVERAGE_EXEC_FILE_ABS="$CODE_COVERAGE_DIR_ABS/jacoco.exec"
        local CODE_COVERAGE_HTML_DIR_ABS="$CODE_COVERAGE_DIR_ABS/code-coverage"
        rm -rf "$CODE_COVERAGE_DIR_ABS"
        if [ "$CODE_COVERAGE" = "yes" ]
        then
            BUCK_TEST_EXTRA_OPTS+=( \
                "--code-coverage" \
                "--code-coverage-format" "HTML" \
                )
        fi

        if [ "${#IGNORED_UNIT_TESTS[@]}" -gt 0 ]
        then
            local IGNORED_UNIT_TEST
            for IGNORED_UNIT_TEST in "${IGNORED_UNIT_TESTS[@]}"
            do
                BUCK_TEST_EXTRA_OPTS+=( "--filter" "!$IGNORED_UNIT_TEST" )
            done
        fi

        if ! run_buck_logged test \
            --no-results-cache \
            "--exclude" "broken" "flaky" \
            "${BUCK_TEST_EXTRA_OPTS[@]}"
        then
            STATUS="broken"
        fi

        if [ -e "$CODE_COVERAGE_EXEC_FILE_ABS" ]
        then
            cp "$CODE_COVERAGE_EXEC_FILE_ABS" "$TARGET_FILE_ABS.coverage.jacoco.exec"
            JACOCO_TOOLBOX_EXEC_FILES_ABS["$ARTIFACT_BASENAME"]="$TARGET_FILE_ABS.coverage.jacoco.exec"
            if [ "$USE_JACOCO_TOOLBOX" = "yes" ]
            then
                rm -rf "$CODE_COVERAGE_HTML_DIR_ABS"
                local BUCK_REPORT_GENERATOR_INVOCATION="$(grep "buck/build/report-generator.jar" "$TARGET_FILE_ABS.test.buck_log.txt" | head -n 1 || true)"
                if [ -n "$BUCK_REPORT_GENERATOR_INVOCATION" ]
                then
                    while IFS='=' read KEY VALUE
                    do
                        case "$KEY" in
                            "classes.dir" )
                                JACOCO_TOOLBOX_CLASS_FILES_ABS["$ARTIFACT_BASENAME"]="$VALUE"
                                ;;
                            "src.dir" )
                                JACOCO_TOOLBOX_SOURCE_FILES_ABS["$ARTIFACT_BASENAME"]="$VALUE"
                                ;;
                        esac
                    done < <( sed -e 's/ -\(D\|jar \)/\n/g' <<<"$BUCK_REPORT_GENERATOR_INVOCATION" )
                fi
                if [ -n "${JACOCO_TOOLBOX_CLASS_FILES_ABS["$ARTIFACT_BASENAME"]}" ]
                then
                    run_jacoco_toolbox_report_logged "$ARTIFACT_BASENAME" "$ARTIFACT_GROUP"
                fi
            fi
        fi
        if [ -e "$CODE_COVERAGE_HTML_DIR_ABS" ]
        then
            cp -a "$CODE_COVERAGE_HTML_DIR_ABS" "$TARGET_DIR_ABS/$COVERAGE_DIR_RELT/$ARTIFACT_BASENAME"
        fi
    fi
}



#---------------------------------------------------------------------
run_buck_build() {
    local DESCRIPTION="$1"
    local BUCK_TARGET="$2"
    local BUCK_GENERATED_FILE_RELBG="$3"
    local ARTIFACT_GROUP="$4"
    local INSERT_BEFORE="$5"

    if [ -z "$ARTIFACT_GROUP" ]
    then
        ARTIFACT_GROUP="info"
    fi

    local STATUS="failed"
    local ARTIFACT_BASENAME="$(basename "$BUCK_GENERATED_FILE_RELBG")"

    if [ "${#LIMIT_TO[@]}" != "0" ]
    then
        if ! in_array "$ARTIFACT_BASENAME" "${LIMIT_TO[@]}"
        then
            return
        fi
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

    if [ "$STATUS" = "ok" -a "$TEST_UNIT" = "yes" ]
    then
        if [ -z "${REUSE_UNIT_TESTS_FROM[$ARTIFACT_BASENAME]}" ]
        then
            # No unit test re-using for this artifact, so we run the tests
            run_unit_test "$ARTIFACT_BASENAME" "$ARTIFACT_GROUP" "$BUCK_TARGET"
        else
            # This artifact reuses another artifact's tests. If this other
            # artifact did not complete with status 'ok', we need to mark
            # the current artifact's unit tests as broken. Otherwise, broken
            # tests/builds would not propagate.
            local UNIT_TEST_ARTIFACT_BASENAME="${REUSE_UNIT_TESTS_FROM[$ARTIFACT_BASENAME]}"
            if [ "${ARTIFACT_STATUS[$UNIT_TEST_ARTIFACT_BASENAME]}" != "ok" ]
            then
                STATUS="broken"
            fi
        fi
    fi

    if [ "$STATUS" = "ok" -a "$TEST_SYSTEM" = "yes" ]
    then
        if ! run_system_test "$ARTIFACT_BASENAME" "$ARTIFACT_GROUP"
        then
            STATUS="broken"
        fi
    fi

    if [ "$STATUS" = "ok" -a "$GENERATE_JAVADOC" = "yes" ]
    then
        # The directory of the buck generated file matches the source directory
        # relative to the gerrit directory. Hence, we can reuse it below.
        ARTIFACT_SOURCE_DIR_RELG="$(dirname "$BUCK_GENERATED_FILE_RELBG")"
        if [ -z "${REUSE_JAVADOC_FROM[$ARTIFACT_BASENAME]}" ]
        then
            if ! is_api_artifact "$ARTIFACT_BASENAME"
            then
                generate_javadoc \
                    "$ARTIFACT_BASENAME" \
                    "$ARTIFACT_SOURCE_DIR_RELG"

                if is_war_artifact "$ARTIFACT_BASENAME"
                then
                    local WAR_ARTIFACT=
                    for WAR_ARTIFACT in "${ARTIFACTS_WAR[@]}"
                    do
                        if [ -z "${REUSE_JAVADOC_FROM[$WAR_ARTIFACT]}" ]
                        then
                            REUSE_JAVADOC_FROM[$WAR_ARTIFACT]="$ARTIFACT_BASENAME"
                        fi
                    done
                fi
            fi
        fi
    fi

    echo_file_target_html \
        "$STATUS" \
        "$ARTIFACT_BASENAME" \
        "$BUCK_TARGET" \
        "$INSERT_BEFORE" \
        "$ARTIFACT_GROUP"
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

    local COUNT_ARGUMENT="$3"

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

    if [ -n "$COUNT_ARGUMENT" ]
    then
        COUNT="$COUNT_ARGUMENT"
    fi

    if [ "$ASPECT" = "counted" -a -n "$COUNT" ]
    then
        STATUS_TEXT="$COUNT&#160;$STATUS_TEXT"
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

    pushd "$TARGET_DIR_ABS" >/dev/null
    for LOG_FILE_RELT in "$ARTIFACT_FILE_RELT.$ASPECT."*".txt"
    do
        if [ -e "$LOG_FILE_RELT" ]
        then
            local LOG=$(sed -e 's/^.*\.\([^.]*\)\.[^.]*$/\1/' <<<"$LOG_FILE_RELT")
            echo -n "$CONNECTOR<a href=\"$LOG_FILE_RELT\">$LOG</a>"
            CONNECTOR=", "
        fi
    done
    popd >/dev/null

    if [ -z "$CONNECTOR" ]
    then
        echo -n "---"
    fi
}

#---------------------------------------------------------------------
cat_file_header_target_html() {
    cat_target_html <<EOF
  <tr>
    <th>Status</th>
    <th>Artifact</th>
    <th>Size</th>
    <th>Build logs</th>
    <th>Unit test logs</th>
    <th colspan="4">Unit test coverage</th>
    <th>System test logs</th>
    <th>Repository</th>
    <th>Description</th>
    <th>Commit</th>
    <th colspan="3">Changes</th>
    <th colspan="2">Documentation</th>
  </tr>
EOF
}

echo_artifact_group_name() {
    local NAME=""
    case "$ARTIFACT_GROUP" in
        "war" )
            NAME="WAR"
            ;;
        "api" )
            NAME="API"
            ;;
        "bundled" )
            NAME="Bundled plugins"
            ;;
        "separate" )
            NAME="Separate plugins"
            ;;
        "info" )
            NAME="Build information"
            ;;
        * )
            NAME="$ARTIFACT_GROUP"
            ;;
    esac
    echo "$NAME"
}

#---------------------------------------------------------------------
echo_file_size() {
    local FILE_ABS="$1"
    FORMATTED_SIZE="---"

    if [ -e "$FILE_ABS" ]
    then
        local SIZE="$(stat -c%s "$FILE_ABS")"
        if [ -n "$SIZE" ]
        then
            if [ "$SIZE" = 0 ]
            then
                FORMATTED_SIZE="0.0"
            elif [ "$SIZE" -lt 50000 ]
            then
                FORMATTED_SIZE="&lt;0.1"
            else
                FORMATTED_SIZE="$(( (SIZE + 50000) / 1000 / 100))"
                FORMATTED_SIZE="${FORMATTED_SIZE:0:-1}.${FORMATTED_SIZE: -1:1}"
                if [ "${FORMATTED_SIZE:0:1}" = "." ]
                then
                    FORMATTED_SIZE="0$FORMATTED_SIZE"
                fi
            fi
            FORMATTED_SIZE="$FORMATTED_SIZE&#160;MB"
        fi
    fi
    echo "$FORMATTED_SIZE"
}

#---------------------------------------------------------------------
echo_extracted_coverage() {
    INDEX_HTML_FILE_ABS="$1"
    ROW_NUMBER="$2"

    sed -e 's@</\?tr\(>\| \)@\n@g' <"$INDEX_HTML_FILE_ABS" \
        | grep 'Total' \
        | sed -e 's@</\?td\(>\| \)@\n@g' \
        | cut -f 2 -d '>' \
        | grep % \
        | head -n "$ROW_NUMBER" \
        | tail -n 1
}

#---------------------------------------------------------------------
echo_file_target_html() {
    local STATUS="$1"
    local ARTIFACT_FILE_RELT="$2"
    local BUCK_TARGET="$3"
    local INSERT_BEFORE="$4"
    local ARTIFACT_GROUP="$5"

    if [ -z "$ARTIFACT_GROUP" ]
    then
        ARTIFACT_GROUP="info"
    fi

    if [ "$MAIN_ARTIFACT_TABLE_LAST_GROUP" != "$ARTIFACT_GROUP" -a -z "$INSERT_BEFORE" ]
    then
        if [ ! -z "$MAIN_ARTIFACT_TABLE_LAST_GROUP" ]
        then
            echo_target_html "</table>"
        fi

        local HEADER="$(echo_artifact_group_name "$ARTIFACT_GROUP")"
cat_target_html <<EOF
<h3 id="group-$ARTIFACT_GROUP">$HEADER</h3>
<table>
EOF
        cat_file_header_target_html
    fi
    MAIN_ARTIFACT_TABLE_LAST_GROUP="$ARTIFACT_GROUP"

    local SIZE_CELL="---"
    local ARTIFACT_CELL=
    if [ -e "$TARGET_DIR_ABS/$ARTIFACT_FILE_RELT" ]
    then
        ARTIFACT_CELL="<a href=\"$ARTIFACT_FILE_RELT\">$ARTIFACT_FILE_RELT</a>"
        SIZE_CELL="$(echo_file_size "$TARGET_DIR_ABS/$ARTIFACT_FILE_RELT")"
    else
        ARTIFACT_CELL="$ARTIFACT_FILE_RELT"
    fi

    local BUILD_LOG_CELL="---"
    local UNIT_TEST_LOG_CELL="---"
    local SYSTEM_TEST_LOG_CELL="---"
    local REPO_CELL="---"
    local DESCRIPTION_CELL="---"
    local COMMIT_CELL="---"
    local DASHBOARD_CELL="---"
    local OPEN_CHANGES_CELL="---"
    local CLOSED_CHANGES_CELL="---"
    local MANUAL_CELL="---"
    local JAVADOC_CELL="---"
    local COVERAGE_INSTRUCTIONS_CELL="---"
    local COVERAGE_BRANCHES_CELL="---"
    local COVERAGE_EXEC_CELL="---"
    local COVERAGE_DOC_CELL="---"
    if [ -n "$BUCK_TARGET" ]
    then
        BUILD_LOG_CELL="$(echo_logged_build_cell "$ARTIFACT_FILE_RELT" "build")"

        local UNIT_TEST_ARTIFACT_FILE_RELT="$ARTIFACT_FILE_RELT"
        if [ -n "${REUSE_UNIT_TESTS_FROM[$ARTIFACT_FILE_RELT]}" ]
        then
            UNIT_TEST_ARTIFACT_FILE_RELT="${REUSE_UNIT_TESTS_FROM[$UNIT_TEST_ARTIFACT_FILE_RELT]}"
        fi
        UNIT_TEST_LOG_CELL="$(echo_logged_build_cell "$UNIT_TEST_ARTIFACT_FILE_RELT" "test")"
        if [ -e "$TARGET_DIR_ABS/$UNIT_TEST_ARTIFACT_FILE_RELT.coverage.jacoco.exec" ]
        then
            COVERAGE_EXEC_CELL="<a href=\"$UNIT_TEST_ARTIFACT_FILE_RELT.coverage.jacoco.exec\">jacoco.exec</a>"
        fi

        local COVERAGE_INDEX_FILE_ABS="$DOC_COVERAGE_DIR_ABS/$UNIT_TEST_ARTIFACT_FILE_RELT/index.html"
        if [ -e "$COVERAGE_INDEX_FILE_ABS" ]
        then
            COVERAGE_DOC_CELL="<a href=\"$COVERAGE_DIR_RELT/$UNIT_TEST_ARTIFACT_FILE_RELT/index.html\">html</a>"

            local COVERED_INSTRUCTIONS="$(echo_extracted_coverage "$COVERAGE_INDEX_FILE_ABS" 1 || echo "?%")"
            # Using ARTIFACT_... instead of UNIT_TEST_ARTIFACT_... as it relates
            # to the current artifact
            COVERAGE_INSTRUCTIONS_CELL="<span title=\"$COVERED_INSTRUCTIONS of $ARTIFACT_FILE_RELT instructions covered by unit tests\">$COVERED_INSTRUCTIONS</span>"

            local COVERED_BRANCHES="$(echo_extracted_coverage "$COVERAGE_INDEX_FILE_ABS" 2 || echo "?%")"
            # Using ARTIFACT_... instead of UNIT_TEST_ARTIFACT_... as it relates
            # to the current artifact
            COVERAGE_BRANCHES_CELL="<span title=\"$COVERED_BRANCHES of $ARTIFACT_FILE_RELT branches covered by unit tests\">$COVERED_BRANCHES</span>"
        fi

        SYSTEM_TEST_LOG_CELL="$(echo_logged_build_cell "$ARTIFACT_FILE_RELT" "system")"

        local REPO="$(cut -f 1 -d : <<<"$BUCK_TARGET")"
        if [ "${REPO:0:8}" != "plugins/" ]
        then
            REPO="gerrit"
            pushd "$GERRIT_DIR_ABS" >/dev/null
        else
            pushd "$GERRIT_DIR_ABS/$REPO" >/dev/null
        fi
        REPO_UPSTREAM_NAME=$(git config --get remote.origin.url | cut -d / -f 4-)
        if [ -z "$REPO_UPSTREAM_NAME" ]
        then
            REPO_UPSTREAM_NAME="$REPO"
        fi
        local COMMIT_HASH="$(run_git rev-parse HEAD)"
        popd  >/dev/null
        REPO_CELL="<a href=\"https://gerrit-review.googlesource.com/#/admin/projects/$REPO_UPSTREAM_NAME\">$REPO_UPSTREAM_NAME</a>"
        DESCRIPTION_CELL="<a href=\"https://gerrit.googlesource.com/$REPO_UPSTREAM_NAME/+/$COMMIT_HASH\">${REPO_DESCRIPTIONS["$REPO"]}</a>"
        COMMIT_CELL="<a href=\"https://gerrit.googlesource.com/$REPO_UPSTREAM_NAME/+/$COMMIT_HASH\"><code>${COMMIT_HASH:0:8}</code></a>"
        if [ -n "${REPO_ARTIFACTS["$REPO"]}" ]
        then
            REPO_ARTIFACTS["$REPO"]="${REPO_ARTIFACTS["$REPO"]},"
        fi
        REPO_ARTIFACTS["$REPO"]="${REPO_ARTIFACTS["$REPO"]}$ARTIFACT_FILE_RELT"
        DASHBOARD_CELL="<a href=\"https://gerrit-review.googlesource.com/#/projects/$REPO_UPSTREAM_NAME,dashboards/default\">dashboard</a>"
        local OPEN_CHANGES_COUNT="$( \
          curl -Ss "https://gerrit-review.googlesource.com/changes/?q=project:$REPO_UPSTREAM_NAME+branch:$BRANCH+status:open&n=11" \
          | grep '^    "id"' \
          | wc -l)"
        local OPEN_CHANGES_COUNT_VERBAL=""
        local OPEN_CHANGES_COUNT_VISUAL=""
        case "$OPEN_CHANGES_COUNT" in
            "0" )
                OPEN_CHANGES_COUNT_VERBAL="have been no open changes "
                ;;
            "1" )
                OPEN_CHANGES_COUNT_VERBAL="has been $OPEN_CHANGES_COUNT open change"
                OPEN_CHANGES_COUNT_VISUAL="$OPEN_CHANGES_COUNT"
                ;;
            "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" | "10" )
                OPEN_CHANGES_COUNT_VERBAL="have been $OPEN_CHANGES_COUNT open changes"
                OPEN_CHANGES_COUNT_VISUAL="$OPEN_CHANGES_COUNT"
                ;;
            "11" )
                OPEN_CHANGES_COUNT_VERBAL="have been more than 10 open changes"
                OPEN_CHANGES_COUNT_VISUAL=">10"
                ;;
        esac
        local OPEN_CHANGES_TITLE_ATTRIBUTE=
        if [ -n "$OPEN_CHANGES_COUNT_VERBAL" ]
        then
            OPEN_CHANGES_TITLE_ATTRIBUTE=" title=\"On $(timestamp) there $OPEN_CHANGES_COUNT_VERBAL waiting for review for $REPO_UPSTREAM_NAME on $BRANCH\""
        fi
        local OPEN_CHANGES_TEXT_ADDENDUM=
        if [ -n "$OPEN_CHANGES_COUNT_VISUAL" ]
        then
            OPEN_CHANGES_TEXT_ADDENDUM="<span class=\"elevated\">($OPEN_CHANGES_COUNT_VISUAL)</span>"
        fi
        OPEN_CHANGES_CELL="<a href=\"https://gerrit-review.googlesource.com/#/q/project:$REPO_UPSTREAM_NAME+branch:$BRANCH+status:open\"$OPEN_CHANGES_TITLE_ATTRIBUTE>open$OPEN_CHANGES_TEXT_ADDENDUM</a>"
        CLOSED_CHANGES_CELL="<a href=\"https://gerrit-review.googlesource.com/#/q/project:$REPO_UPSTREAM_NAME+branch:$BRANCH+status:closed\">closed</a>"

        local MANUAL_ARTIFACT_FILE_RELT="$ARTIFACT_FILE_RELT"
        if [ -n "${REUSE_MANUAL_FROM[$ARTIFACT_FILE_RELT]}" ]
        then
            MANUAL_ARTIFACT_FILE_RELT="${REUSE_MANUAL_FROM[$ARTIFACT_FILE_RELT]}"
        fi
        if [ -e "$TARGET_DIR_ABS/$MANUAL_ARTIFACT_FILE_RELT.manual.stderr.txt" ]
        then
            if [ -e "$TARGET_DIR_ABS/${DOC_MANUAL_FILE_RELT["$MANUAL_ARTIFACT_FILE_RELT"]}" ]
            then
                MANUAL_CELL="<a href=\"${DOC_MANUAL_FILE_RELT["$MANUAL_ARTIFACT_FILE_RELT"]}\">manual</a>"
            else
                MANUAL_CELL="manual"
            fi
            MANUAL_CELL+=" (<a href=\"$MANUAL_ARTIFACT_FILE_RELT.manual.stderr.txt\">stderr</a>"
            MANUAL_CELL+=", <a href=\"$MANUAL_ARTIFACT_FILE_RELT.manual.stdout.txt\">stdout</a>)"
        fi

        local JAVADOC_ARTIFACT_FILE_RELT="$ARTIFACT_FILE_RELT"
        if [ -n "${REUSE_JAVADOC_FROM[$ARTIFACT_FILE_RELT]}" ]
        then
            JAVADOC_ARTIFACT_FILE_RELT="${REUSE_JAVADOC_FROM[$ARTIFACT_FILE_RELT]}"
        fi
        if [ -e "$TARGET_DIR_ABS/$JAVADOC_ARTIFACT_FILE_RELT.javadoc.stderr.txt" ]
        then
            local JAVADOC_INDEX_FILE_RELT="$JAVADOC_DIR_RELT/$JAVADOC_ARTIFACT_FILE_RELT/index.html"
            if [ -e "$TARGET_DIR_ABS/$JAVADOC_INDEX_FILE_RELT" ]
            then
                JAVADOC_CELL="<a href=\"$JAVADOC_INDEX_FILE_RELT\">javadoc</a>"
            else
                JAVADOC_CELL="javadoc"
            fi
            JAVADOC_CELL+=" (<a href=\"$JAVADOC_ARTIFACT_FILE_RELT.javadoc.stderr.txt\">stderr</a>"
            JAVADOC_CELL+=", <a href=\"$JAVADOC_ARTIFACT_FILE_RELT.javadoc.stdout.txt\">stdout</a>)"
        fi
    fi

    local STATUS_TEXT=
    set_STATUS_TEXT "uncounted"

    local HTML="
  <!-- Artifact: $ARTIFACT_FILE_RELT -->
  <tr id=\"$ARTIFACT_FILE_RELT\" class=\"$STATUS\">
    <td><img src=\"$IMAGE_BASE_URL/$STATUS.png\" alt=\"Build $STATUS\" />&#160;$STATUS_TEXT</td>
    <th class=\"left th-semi-dark th-$STATUS\">$ARTIFACT_CELL</th>
    <td class=\"right\">$SIZE_CELL</td>
    <td>$BUILD_LOG_CELL</td>
    <td>$UNIT_TEST_LOG_CELL</td>
    <td>$COVERAGE_INSTRUCTIONS_CELL</td>
    <td>$COVERAGE_BRANCHES_CELL</td>
    <td>$COVERAGE_EXEC_CELL</td>
    <td>$COVERAGE_DOC_CELL</td>
    <td>$SYSTEM_TEST_LOG_CELL</td>
    <td>$REPO_CELL</td>
    <td>$DESCRIPTION_CELL</td>
    <td>$COMMIT_CELL</td>
    <td>$DASHBOARD_CELL</td>
    <td>$OPEN_CHANGES_CELL</td>
    <td>$CLOSED_CHANGES_CELL</td>
    <td>$MANUAL_CELL</td>
    <td>$JAVADOC_CELL</td>
  </tr>"

    if [ -z "$INSERT_BEFORE" ]
    then
        echo_target_html "$HTML"
    else
        HTML="${HTML//
/\\n}"
        sed -i -e '/Artifact: '"$INSERT_BEFORE"'/s@^@'"${HTML//&/\\&}"'\n@' "$TARGET_HTML_FILE_ABS"
    fi

    update_artifact_group_statistics "$STATUS" "$ARTIFACT_GROUP"

    ARTIFACTS_TOTAL=$((ARTIFACTS_TOTAL+1))
    if [ "$STATUS" = "ok" ]
    then
        ARTIFACTS_OK=$((ARTIFACTS_OK+1))
    elif [ "$STATUS" = "broken" ]
    then
        ARTIFACTS_BROKEN=$((ARTIFACTS_BROKEN+1))
        if [ -z "$FIRST_BROKEN_ARTIFACT" ]
        then
            FIRST_BROKEN_ARTIFACT="$ARTIFACT_FILE_RELT"
        fi
    else
        ARTIFACTS_FAILED=$((ARTIFACTS_FAILED+1))
        if [ -z "$FIRST_FAILED_ARTIFACT" ]
        then
            FIRST_FAILED_ARTIFACT="$ARTIFACT_FILE_RELT"
        fi
    fi
    ARTIFACT_STATUS["$ARTIFACT_FILE_RELT"]="$STATUS"
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
            LEGAL_URL="$(cat "$LEGAL_URL_RELS" | grep '^[^#]' | head -n 1)"
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
.elevated {
  font-size: 75%;
  vertical-align: super;
}
th, td {
  border: 1px solid black;
}
table {
  border-collapse: collapse;
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
.borderless {
  border: 0px solid black !important;
}
.tablerow table {
  display: inline-block;
  vertical-align: top;
  margin-right: 3em;
}
.tablerow h3 {
  display: none;
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

    echo_target_html -n "<p class=\"legal-footer\">Last update: $(timestamp)"
    if [ ! -z "$LEGAL_URL" ]
    then
        echo_target_html -n "<br/><a href=\"$LEGAL_URL\">Contact / Legal</a>"
    fi
    echo_target_html "</p>"
    cat_target_html <<EOF

</body>
</html>
EOF
}
#---------------------------------------------------------------------
echo_html_split_target_html() {
    local SPLIT="<p>- "
    if [ "$SKIP_PARENT_LINK" != "yes" ]
    then
        SPLIT="$SPLIT<a href=\"../$INDEX_FILE_RELC\">View parent directory</a> - "
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

#---------------------------------------------------------------------
set_config_value_file() {
    local FILE="$1"
    local LABEL="$2"
    local VALUE="$3"
    run_git config -f "$FILE" "$LABEL" "$VALUE"
}

#---------------------------------------------------------------------
set_config_value_file_stub() {
    local FILE_STUB="$1"
    local LABEL="$2"
    local VALUE="$3"
    set_config_value_file "$TEST_SITE_DIR_ABS"/etc/"$FILE_STUB".config "$LABEL" "$VALUE"
}

#---------------------------------------------------------------------
set_config_value() {
    local LABEL="$1"
    local VALUE="$2"
    set_config_value_file_stub "gerrit" "$LABEL" "$VALUE"
}

#---------------------------------------------------------------------
add_config_value_file() {
    local FILE="$1"
    local LABEL="$2"
    local VALUE="$3"
    run_git config -f "$FILE" --add "$LABEL" "$VALUE"
}

#---------------------------------------------------------------------
add_config_value_file_stub() {
    local FILE_STUB="$1"
    local LABEL="$2"
    local VALUE="$3"
    add_config_value_file "$TEST_SITE_DIR_ABS"/etc/"$FILE_STUB".config "$LABEL" "$VALUE"
}

#---------------------------------------------------------------------
add_config_value() {
    local LABEL="$1"
    local VALUE="$2"
    add_config_value_file_stub "gerrit" "$LABEL" "$VALUE"
}

#---------------------------------------------------------------------
set_config_value_secure() {
    local LABEL="$1"
    local VALUE="$2"
    set_config_value_file_stub "secure" "$LABEL" "$VALUE"
}

#---------------------------------------------------------------------
unset_config_value_file() {
    local FILE="$1"
    local LABEL="$2"
    run_git config --unset -f "$FILE" "$LABEL"
}

#---------------------------------------------------------------------
unset_config_value_file_stub() {
    local FILE_STUB="$1"
    local LABEL="$2"
    unset_config_value_file "$TEST_SITE_DIR_ABS"/etc/"$FILE_STUB".config "$LABEL"
}

#---------------------------------------------------------------------
unset_config_value() {
    local LABEL="$1"
    unset_config_value_file_stub "gerrit" "$LABEL"
}

#---------------------------------------------------------------------
remove_config_section_file() {
    local FILE="$1"
    local LABEL="$2"
    run_git config -f "$FILE" --remove-section "$LABEL"
}

#---------------------------------------------------------------------
remove_config_section_stub() {
    local FILE_STUB="$1"
    local LABEL="$2"
    set_config_value_file "$TEST_SITE_DIR_ABS"/etc/"$FILE_STUB".config "$LABEL.dummy" "dummy"
    remove_config_section_file "$TEST_SITE_DIR_ABS"/etc/"$FILE_STUB".config "$LABEL"
}

#---------------------------------------------------------------------
remove_config_section() {
    local LABEL="$1"
    remove_config_section_stub "gerrit" "$LABEL"
}

#---------------------------------------------------------------------
curl_as () {
    local USER_NAME="$1"
    shift

    local USERNAME="$(cat "$TEST_SITE_USER_CREDENTIALS_DIR_ABS/$USER_NAME/username")"
    local PASSWORD="$(cat "$TEST_SITE_USER_CREDENTIALS_DIR_ABS/$USER_NAME/password")"

    if [ -z "${TEST_SITE_SESSIONS[$USER_NAME]}" ]
    then
        TEST_SITE_SESSIONS[$USER_NAME]=$(curl --silent --show-error -D - 'http://127.0.0.1:8089/login/%23%2F?user_name='"$USER_NAME" | grep Set-Cookie | sed -e 's/^.*GerritAccount=\([^;]*\);.*/\1/')
    fi
    local SESSION="${TEST_SITE_SESSIONS[$USER_NAME]}"

    curl --silent --show-error --digest --user "$USERNAME:$PASSWORD" --cookie 'GerritAccount=$SESSION' "$@" >/dev/null
}

#---------------------------------------------------------------------
check_error_log_for () {
    local CHECK_RE="$1"
    local ITERATION_COUNT=0
    local MAX_ITERATION_COUNT=3
    local RETURN_CODE=1
    while [ $ITERATION_COUNT -lt $ITERATION_COUNT -a "$RETURN_CODE" = "1" ]
    do
        if [[ "$(tail -n 1 "$TEST_SITE_DIR_ABS/logs/error.log" | cut -f 5- -d ' ')" =~ "$CHECK_RE" ]]
        then
            RETURN_CODE=0
        else
            sleep 1s
        fi
        ITERATION_COUNT=$((ITERATION_COUNT+1))
    done
    if [ "$RETURN_CODE" = "1" ]
    then
        info "Could not find '$CHECK_RE' in test site's error log"
    fi
    return "$RETURN_CODE"
}

#---------------------------------------------------------------------
checked_kill_helper() {
    local PID="$1"
    shift

    if [ -e "/proc/$PID" ]
    then
        kill "$@" "$PID"
        COUNT=0
        while [ -e "/proc/$PID" -a "$COUNT" -lt 10 ]
        do
            sleep 1
            COUNT=$((COUNT+1))
        done
    fi
}

#---------------------------------------------------------------------
checked_kill() {
    local COMMAND="$1"
    local FILTER_RE="$2"

    if [ -z "$FILTER_RE" ]
    then
        FILTER_RE="."
    fi

    local PIDS=$( \
        ps -C "$COMMAND" -o pid,cmd \
        | grep -e "$FILTER_RE" \
        | grep -e '^[[:space:]]*\([0-9]\+\)[[:space:]].*' \
        | sed -e 's/^[[:space:]]*\([0-9]\+\)[[:space:]].*/\1/' \
    )

    if [ -n "$PIDS" ]
    then
        for PID in "$PIDS"
        do
            checked_kill_helper "$PID"
            checked_kill_helper "$PID"
            checked_kill_helper "$PID" "-KILL"

            if [ -e "/proc/$PID" ]
            then
                error "$COMMAND pid $PID still alive"
            fi
        done
    fi
}

plugin_name_to_REPO_DIR_RELS() {
    local PLUGIN_NAME="$1"
    if [ "${PLUGIN_NAME:0:8}" = "plugins/" ]
    then
        REPO_DIR_RELS="${PLUGIN_NAME:8}"
    else
        REPO_DIR_RELS="${PLUGIN_NAME}"
    fi

    REPO_DIR_RELS="${REPO_DIR_RELS////-}" # all slashes to dashes
    REPO_DIR_RELS="plugins/$REPO_DIR_RELS" # force slash in "plugins/"
}
