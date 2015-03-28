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
IGNORED_PLUGINS=()


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

mkdir -p "$OVERVIEW_DIR_ABS"

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
for BUILD_DIR_RELO in *
do
    if [ -d "$BUILD_DIR_RELO" ]
    then
        STATUS=$(cat "$BUILD_DIR_RELO/status.txt" || true)
        case "$STATUS" in
            "failed" )
                ARTIFACTS_FAILED=$(cat "$BUILD_DIR_RELO/failure_count.txt" || true)
                if [ -z "$ARTIFACTS_FAILED" ]
                then
                    ARTIFACTS_FAILED="?"
                fi
                STATUS_CELL_TEXT="$ARTIFACTS_FAILED $STATUS"
                ;;
            "broken" )
                ARTIFACTS_BROKEN=$(cat "$BUILD_DIR_RELO/broken_count.txt" || true)
                if [ -z "$ARTIFACTS_BROKEN" ]
                then
                    ARTIFACTS_BROKEN="?"
                fi
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

        API_VERSION=$(cat "$BUILD_DIR_RELO/api_version.txt" || true)
        if [ -z "$API_VERSION" ]
        then
            API_VERSION="---"
        fi

        DB_SCHEMA_VERSION=$(cat "$BUILD_DIR_RELO/db_schema_version.txt" || true)
        if [ -z "$DB_SCHEMA_VERSION" ]
        then
            DB_SCHEMA_VERSION="---"
        fi

        REPO_DESCRIPTION=$(cat "$BUILD_DIR_RELO/gerrit_description.txt" || true)
        if [ -z "$REPO_DESCRIPTION" ]
        then
            REPO_DESCRIPTION="---"
        fi

        cat_target_html <<EOF
  <tr>
    <td><a href="$BUILD_DIR_RELO/index.html">$BUILD_DIR_RELO</a></td>
    <td><img src="$IMAGE_BASE_URL/$STATUS.png" alt="Build $STATUS" />&#160;$STATUS_CELL_TEXT</td>
    <td>$REPO_DESCRIPTION</td>
    <td>$API_VERSION</td>
    <td>$DB_SCHEMA_VERSION</td>
  </tr>
EOF
    fi
done
popd >/dev/null

echo_target_html "</table>"

cat_html_footer_target_html

finalize
