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

MANUAL_DIR_RELT="docs/manual"
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
add_to_plugins "avatars/external"
add_to_plugins "delete-project"
add_to_plugins "its-base"
add_to_plugins "its-bugzilla"
add_to_plugins "its-jira"
add_to_plugins "its-phabricator"
add_to_plugins "its-rtc"
add_to_plugins "its-storyboard"
add_to_plugins "quota"
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

declare -A ARTIFACT_GROUP_STATUS=()
declare -A ARTIFACT_GROUP_STATUS_COUNT=()
declare -A ARTIFACT_GROUP_TOTAL_COUNT=()
#---------------------------------------------------------------------
mkdir -p "$ARTIFACTS_DIR_ABS"

#---------------------------------------------------------------------
post_parameter_parsing_setup() {
    DOC_MANUAL_DIR_ABS="$TARGET_DIR_ABS/$MANUAL_DIR_RELT"
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

    popd >/dev/null
}



#---------------------------------------------------------------------
# sets $DOC_MANUAL_FILE_RELM if appropriate
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
                            else
                                info "Plugin loaded, but was not listed as '$PLUGIN_NAME'"
                                RETURN_CODE=1
                            fi

                            if [ "$GENERATE_MANUAL" = "yes" ]
                            then
                                DOC_MANUAL_FILE_RELM="plugins/$PLUGIN_NAME/Documentation/index.html"
                                dump_manual "$DOC_MANUAL_FILE_RELM" \
                                    2> >(tee "$TARGET_FILE_ABS.manual.stderr.txt" >&2) \
                                    | tee "$TARGET_FILE_ABS.manual.stdout.txt"
                            fi
                        else
                            info Failed to install plugin
                            RETURN_CODE=1
                        fi
                    else
                        info "Plugin $PLUGIN_JAR_FILE_ABS does not exist"
                        RETURN_CODE=1
                    fi
                elif [ "$ARTIFACT_BASENAME" = "withdocs.war" ]
                then
                    if [ "$GENERATE_MANUAL" = "yes" ]
                    then
                        DOC_MANUAL_FILE_RELM="Documentation/index.html"
                        dump_manual \
                            "$DOC_MANUAL_FILE_RELM" \
                            "Documentation/images/link.png" \
                            2> >(tee "$TARGET_FILE_ABS.manual.stderr.txt" >&2) \
                            | tee "$TARGET_FILE_ABS.manual.stdout.txt"
                    fi
                fi
            else
                info "Gerrit did not come up"
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
run_buck_build() {
    local DESCRIPTION="$1"
    local BUCK_TARGET="$2"
    local BUCK_GENERATED_FILE_RELBG="$3"
    local ARTIFACT_GROUP="$4"
    local INSERT_BEFORE="$5"

    local DOC_MANUAL_FILE_RELM=""

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

    if [ "$STATUS" = "ok" -a "$TEST_UNIT" = "yes" -a -n "${TARGET_TEST_LABELS["$BUCK_TARGET"]}" ]
    then
        if ! run_buck_logged test --include "${TARGET_TEST_LABELS["$BUCK_TARGET"]}" then
        then
            STATUS="broken"
        fi
    fi

    if [ "$STATUS" = "ok" -a "$TEST_SYSTEM" = "yes" ]
    then
        if ! run_system_test "$ARTIFACT_BASENAME" "$ARTIFACT_GROUP"
        then
            STATUS="broken"
        fi
    fi

    echo_file_target_html "$STATUS" "$ARTIFACT_BASENAME" "$BUCK_TARGET" "$INSERT_BEFORE" "$ARTIFACT_GROUP" "$DOC_MANUAL_FILE_RELM"
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
    <th>System test logs</th>
    <th>Repository</th>
    <th>Description</th>
    <th>Commit</th>
    <th colspan="3">Changes</th>
    <th>Documentation</th>
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
echo_file_target_html() {
    local STATUS="$1"
    local ARTIFACT_FILE_RELT="$2"
    local BUCK_TARGET="$3"
    local INSERT_BEFORE="$4"
    local ARTIFACT_GROUP="$5"
    local DOC_MANUAL_FILE_RELM="$6"

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
    local DOCUMENTATION_CELL="---"
    if [ -n "$BUCK_TARGET" ]
    then
        BUILD_LOG_CELL="$(echo_logged_build_cell "$ARTIFACT_FILE_RELT" "build")"
        UNIT_TEST_LOG_CELL="$(echo_logged_build_cell "$ARTIFACT_FILE_RELT" "test")"
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
        OPEN_CHANGES_CELL="<a href=\"https://gerrit-review.googlesource.com/#/q/project:$REPO_UPSTREAM_NAME+branch:$BRANCH+status:open\">open</a>"
        CLOSED_CHANGES_CELL="<a href=\"https://gerrit-review.googlesource.com/#/q/project:$REPO_UPSTREAM_NAME+branch:$BRANCH+status:closed\">closed</a>"
        if [ -n "$DOC_MANUAL_FILE_RELM" ]
        then
            DOCUMENTATION_CELL="<a href=\"$MANUAL_DIR_RELT/$DOC_MANUAL_FILE_RELM\">manual</a>"
            DOCUMENTATION_CELL+=" (<a href=\"$ARTIFACT_FILE_RELT.manual.stderr.txt\">stderr</a>"
            DOCUMENTATION_CELL+=", <a href=\"$ARTIFACT_FILE_RELT.manual.stdout.txt\">stdout</a>)"
        fi
    fi

    # Patching up WAR artifacts with documentation from withdocs.jar
    case "$ARTIFACT_FILE_RELT" in
        "release.war" )
            if [ "$DOCUMENTATION_CELL" = "---" \
                -a "$DOCUMENTATION_CELL_WITHDOCS_WAR" != "---" ]
            then
                DOCUMENTATION_CELL="$DOCUMENTATION_CELL_WITHDOCS_WAR"
            fi
            ;;
        "withdocs.war" )
            DOCUMENTATION_CELL_WITHDOCS_WAR="$DOCUMENTATION_CELL"
            ;;
    esac

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
    <td>$SYSTEM_TEST_LOG_CELL</td>
    <td>$REPO_CELL</td>
    <td>$DESCRIPTION_CELL</td>
    <td>$COMMIT_CELL</td>
    <td>$DASHBOARD_CELL</td>
    <td>$OPEN_CHANGES_CELL</td>
    <td>$CLOSED_CHANGES_CELL</td>
    <td>$DOCUMENTATION_CELL</td>
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

    echo_target_html -n "<p class="legal-footer">Last update: $(timestamp)"
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
