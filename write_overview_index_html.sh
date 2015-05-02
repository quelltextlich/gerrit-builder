#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

print_help() {
    cat <<EOF
$0 ARGUMENTS

ARGUMENTS:
  --help             - prints this page
  --branch BRANCH    - Build branch BRANCH instead of the default, which is
                       inferred from the basename of the directory, with
                       "master" as fallback.
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
        "--branch" )
            [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
            BRANCH="$1"
            shift || true
            ;;
        * )
            error "Unknown argument '$ARGUMENT'"
            ;;
    esac
done

mkdir -p "$ARTIFACTS_DIR_ABS"

set_target_html_file_abs "$ARTIFACTS_DIR_ABS/index.html"

cat_html_header_target_html \
    "Gerrit $BRANCH builds" \
    "Gerrit $BRANCH builds" \
    "gerrit, jar, $BRANCH" \
    "Gerrit builds for $BRANCH"

cat_target_html <<EOF
<table>
  <tr>
    <td class="borderless" colspan="1"/>
    <th rowspan="2">Build</th>
    <td class="borderless" colspan="3"/>
    <th colspan="4">Status per artifact group</th>
  </tr>

  <tr>
    <th>Status</th>
    <th>Gerrit HEAD</th>
    <th>API version</th>
    <th>DB schema<br/>version</th>
    <th>WAR</th>
    <th>API</th>
    <th>Bundled<br/>plugins</th>
    <th>Separate<br/>plugins</th>
  </tr>
EOF

declare -A ARTIFACT_GROUP_CELLS=()
declare -A ARTIFACT_GROUP_CELL_EXTRAS=()

add_artifact_group_cell() {
    local ADDITION="$1"
    ARTIFACT_GROUP_CELLS["$ARTIFACT_GROUP"]="${ARTIFACT_GROUP_CELLS["$ARTIFACT_GROUP"]}$ADDITION"
}

read_artifact_group_statuses() {
    ARTIFACT_GROUP_CELLS=()
    ARTIFACT_GROUP_CELL_EXTRAS=()

    INPUT_FILE_RELO="$BUILD_DIR_RELO/artifacts_group_numbers.txt"
    while IFS="," read ARTIFACT_GROUP TOTAL_COUNT GROUP_STATUS GROUP_COUNT FIRST_ARTIFACT_W_STATUS
    do
        ARTIFACT_GROUP_CELLS["$ARTIFACT_GROUP"]=""
        if [ "$ARTIFACT_GROUP" = "total" -a -n "$FIRST_ARTIFACT_W_STATUS" ]
        then
            add_artifact_group_cell "<a href=\"$BUILD_DIR_RELO/index.html#$FIRST_ARTIFACT_W_STATUS\">"
        else
            add_artifact_group_cell "<a href=\"$BUILD_DIR_RELO/index.html#group-$ARTIFACT_GROUP\">"
        fi
        add_artifact_group_cell "<img src=\"$IMAGE_BASE_URL/$GROUP_STATUS.png\" alt=\"Build $GROUP_STATUS\" />"
        if [ "$ARTIFACT_GROUP" = "total" ]
        then
            local COUNT_ARGUMENT=""
            if [ "$GROUP_STATUS" != "ok" ]
            then
                COUNT_ARGUMENT="$GROUP_COUNT"
            fi
            local STATUS_TEXT=
            set_STATUS_TEXT "counted" "$GROUP_STATUS" "$COUNT_ARGUMENT"
            add_artifact_group_cell "&#160;$STATUS_TEXT"
        else
            add_artifact_group_cell " "
            if [ "$GROUP_STATUS" = "ok" ]
            then
                add_artifact_group_cell "$GROUP_STATUS"
            else
                add_artifact_group_cell "$GROUP_COUNT/$TOTAL_COUNT"
            fi
        fi
        add_artifact_group_cell "</a>"
        ARTIFACT_GROUP_CELL_EXTRAS["$ARTIFACT_GROUP"]=" class=\"$STATUS\""
    done < <( echo "total,,died," ; if [ -e "$INPUT_FILE_RELO" ] ; then cat "$INPUT_FILE_RELO" ; fi)
}

echo_group_status_cell_target_html() {
    local ARTIFACT_GROUP="$1"
    local GROUP_CELL="${ARTIFACT_GROUP_CELLS["$ARTIFACT_GROUP"]}"
    local GROUP_CELL_EXTRA="${ARTIFACT_GROUP_CELL_EXTRAS["$ARTIFACT_GROUP"]}"
    if [ -z "$GROUP_CELL" ]
    then
        GROUP_CELL="---"
        GROUP_CELL_EXTRA=""
    fi
    echo_target_html "<td>$GROUP_CELL</td>"
}

dump_first_line_if_exists() {
    local FILE_BASENAME="$1"
    local DEFAULT_LINE="$2"
    local FILE_RELO="$BUILD_DIR_RELO/$FILE_BASENAME"
    local LINE="$DEFAULT_LINE"
    if [ -f "$FILE_RELO" ]
    then
        LINE="$(head -n 1 <"$FILE_RELO")"
    fi
    echo "$LINE"
}

pushd "$ARTIFACTS_DIR_ABS" >/dev/null
for BUILD_DIR_RELO in *
do
    if [ -d "$BUILD_DIR_RELO" ]
    then
        read_artifact_group_statuses || true
        STATUS=$(dump_first_line_if_exists "status.txt")
        case "$STATUS" in
            "failed" )
                ARTIFACTS_FAILED=$(dump_first_line_if_exists "failure_count.txt" "?")
                STATUS_CELL_TEXT="$ARTIFACTS_FAILED $STATUS"
                ;;
            "broken" )
                ARTIFACTS_BROKEN=$(dump_first_line_if_exists "broken_count.txt" "?")
                STATUS_CELL_TEXT="$ARTIFACTS_BROKEN $STATUS"
                ;;
            "ok" | \
                "died" )
                STATUS_CELL_TEXT="$STATUS"
                ;;
            * )
                STATUS="died"
                STATUS_CELL_TEXT="$STATUS"
                ;;
        esac

        API_VERSION=$(dump_first_line_if_exists "api_version.txt" "---")
        DB_SCHEMA_VERSION=$(dump_first_line_if_exists "db_schema_version.txt" "---")
        REPO_DESCRIPTION=$(dump_first_line_if_exists "gerrit_description.txt" "---")

        README="$(dump_first_line_if_exists "README.txt")"

        cat_target_html <<EOF
  <tr>
EOF
        echo_group_status_cell_target_html "total"
        cat_target_html <<EOF
    <td class="th-semi-dark"><a href="$BUILD_DIR_RELO/index.html">$BUILD_DIR_RELO</a></td>
EOF


        cat_target_html <<EOF
    <td>$REPO_DESCRIPTION</td>
    <td>$API_VERSION</td>
    <td>$DB_SCHEMA_VERSION</td>
EOF
        for ARTIFACT_GROUP in war api bundled separate
        do
            echo_group_status_cell_target_html "$ARTIFACT_GROUP"
        done
        if [ -n "$README" ]
        then
            echo_target_html "    <td><a href=\"$BUILD_DIR_RELO/README.txt\">Note</a></td>"
        fi
        echo_target_html "  </tr>"
    fi
done
popd >/dev/null

echo_target_html "</table>"

cat_html_footer_target_html

finalize
