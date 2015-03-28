#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

BASE_URL='http://builds.quelltextlich.at/'

print_help() {
    cat <<EOF
$0 ARGUMENTS

Writes out index html files for folders

ARGUMENTS:
  --base-url BASE_URL
             -- The base url to fetch structure information from.
                E.g.: http://build.quelltextlich.at/gerrit
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
        "--base-url" )
            [ $# -ge 1 ] || error "$ARGUMENT requires 1 more argument"
            BASE_URL="$1"
            shift || true
            ;;
        * )
            error "Unknown argument '$ARGUMENT'"
            ;;
    esac
done

cat_url_file_entries() {
    local URL="$1"
    curl --silent --show-error "$URL" | grep '^<tr><td' | cut -f 4 -d '"'
}

write_base_index_html() {
    local DIR_URL="$1"
    local SHORT_TITLE="$2"
    local TITLE="$3"
    if [ -z "$TITLE" ]
    then
        TITLE="$SHORT_TITLE"
    fi

    if [ "${DIR_URL: -1}" != "/" ]
    then
        DIR_URL="$DIR_URL/"
    fi

    local FILE_RELS_PAD="$DIR_URL"
    if [ ! -z "$FILE_RELS_PAD" ]
    then
        FILE_RELS_PAD="${FILE_RELS_PAD:0: -1}"
    fi
    FILE_RELS_PAD="${FILE_RELS_PAD////_}"
    local FILE_RELS="index$FILE_RELS_PAD.html"

    section "Writing index file for '$FILE_RELS'"

    local SKIP_PARENT_LINK=no
    if [ "$FILE_RELS" = "index.html" ]
    then
        SKIP_PARENT_LINK=yes
    fi

    set_target_html_file_abs "$FILE_RELS"

    cat_html_header_target_html \
        "$SHORT_TITLE" \
        "$SHORT_TITLE" \
        "" \
        "$TITLE"

    cat_target_html <<EOF
<table>
  <tr>
    <th>Entry</th>
    <th>Description</th>
  </tr>
EOF

    cat_url_file_entries "$BASE_URL$DIR_URL" | while read LINE
    do
        SKIP=no
        case "$LINE" in
            "../" )
                if [ -z "$FILE_RELS_PAD" ]
                then
                    SKIP=yes
                else
                    DESCRIPTION="Parent directory"
                fi
                ;;
            "favicon.ico" )
                SKIP=yes
                ;;
            "gerrit/" )
                DESCRIPTION="Builds of Gerrit &amp; plugins"
                ;;
            "images/" )
                SKIP=yes
                ;;
            "nightly/" )
                DESCRIPTION="Nightly builds of Gerrit &amp; plugins"
                ;;
            "LICENSE-Apache-2.0" )
                DESCRIPTION="Default license for artifacts"
                ;;
            "README.txt" )
                DESCRIPTION="More information about the builds"
                ;;
            "master/" | "stable-"*"/")
                DESCRIPTION="Nightly builds of Gerrit &amp; plugins for the ${LINE:0: -1} branch"
                ;;
            * )
                DESCRIPTION="$LINE"
                ;;
        esac
        if [ "$SKIP" = "no" ]
        then
            HREF="$LINE"
            if [ "${HREF: -1}" = "/" ]
            then
                HREF="${HREF}index.html"
            fi
            cat_target_html <<EOF
  <tr>
    <td><a href="$HREF">$LINE</a></td>
    <td>$DESCRIPTION</td>
  </tr>
EOF
        fi
    done

    echo_target_html "</table>"

    cat_html_footer_target_html

}

write_base_index_html "" "Automated builds"
write_base_index_html "/gerrit" "Gerrit builds" "Automated Gerrit builds"
write_base_index_html "/gerrit/nightly" "Nightly Gerrit builds"


finalize
