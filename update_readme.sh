#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

print_help() {
    cat <<EOF
$0 ARGUMENTS TARGET_DIR

ARGUMENTS:
  --help             - prints this page

TARGET_DIR is the path to the directory that holds the build.
EOF
}

parse_arguments() {
    while [ $# -gt 0 ]
    do
        local ARGUMENT="$1"
        shift
        case "$ARGUMENT" in
            "--help" | "-h" | "-?" )
                print_help
                exit 0
                ;;
            * )
                [ $# -eq 0 ] || error "Unknown argument '$ARGUMENT'"
                TARGET_DIR_ABS="$ARGUMENT"
                ;;
        esac
    done
}

TARGET_DIR_ABS=
parse_arguments "$@"

if [ -z "$TARGET_DIR_ABS" ]
then
    error "No TARGET_DIR given"
fi


TARGET_DIR_ABS="${TARGET_DIR_ABS%/README.txt}"
if [ "${TARGET_DIR_ABS:0:1}" != "/" ]
then
    # TARGET_DIR_ABS is not yet absolute
    if [ -e "$ORIG_DIR_ABS/$TARGET_DIR_ABS" ]
    then
        TARGET_DIR_ABS="$ORIG_DIR_ABS/$TARGET_DIR_ABS"
    elif [ -e "$BUILDS_DIR_ABS/$TARGET_DIR_ABS" ]
    then
        TARGET_DIR_ABS="$BUILDS_DIR_ABS/$TARGET_DIR_ABS"
    elif [ -e "$PWD/$TARGET_DIR_ABS" ]
    then
        TARGET_DIR_ABS="$PWD/$TARGET_DIR_ABS"
    else
        error "Could not find existing target directory '$TARGET_DIR'. \
Please provide the absolute path to it"
    fi
fi

if [ ! -e "$TARGET_DIR_ABS" ]
then
    error "Target $TARGET_DIR_ABS does not exist"
fi

if [ ! -d "$TARGET_DIR_ABS" ]
then
    error "Target $TARGET_DIR_ABS is not a directory"
fi

if [ -z "$EDITOR" ]
then
    error "The EDITOR environment variable is not set. Please set it to your editor of choice"
fi

README_FILE_ABS="$TARGET_DIR_ABS/README.txt"
INDEX_FILE_ABS="$TARGET_DIR_ABS/$INDEX_FILE_RELC"
TMP_FILE_ABS="$(mktemp --tmpdir "gerrit-builder-update-readme.XXXXXX.txt")"

if [ -e "$README_FILE_ABS" ]
then
    cp "$README_FILE_ABS" "$TMP_FILE_ABS"
else
    touch "$TMP_FILE_ABS"
fi

cat >>"$TMP_FILE_ABS" <<EOF

# Please use the first line as subject.
# Keep the second line empty.
# Use the remaining line to provide more details.
# Keep lines at <80 characters.
# Lines starting in # will be removed.
# Trailing empty lines will be removed.
EOF

if ! "$EDITOR" "$TMP_FILE_ABS"
then
    error "Editor signalled error."
fi

# Dropping lines starting in #
sed -e '/^#/d' -i "$TMP_FILE_ABS"
# Dropping trailing empty lines
sed -e :a -e '/^\n*$/{$d;N;ba' -e '}' -i "$TMP_FILE_ABS"

if [ -s "$TMP_FILE_ABS" ]
then
    mv "$TMP_FILE_ABS" "$README_FILE_ABS"
else
    rm -f "$README_FILE_ABS"
fi

INDEX_CUT_LINE="$(grep --line-number '<!-- README.txt -->' "$INDEX_FILE_ABS" \
  | head -n 1  | cut -f 1 -d ':' || true)"
if [ -z "$INDEX_CUT_LINE" ]
then
    INDEX_CUT_LINE="$(grep --line-number '^<h2[^>]*>Artifacts</h2>' "$INDEX_FILE_ABS" \
      | head -n 1 | cut -f 1 -d ':' || true)"
    if [ -z "$INDEX_CUT_LINE" ]
    then
        error "Could not find place to cut/insert README.txt"
    else
        # Cutting at <h2...>Artifacts</h2>
        INDEX_CUT_LINE=$((INDEX_CUT_LINE-1))
    fi
else
    # Cutting at <!-- README.txt -->
    :
fi

# Render part before the README.txt
head -n "$INDEX_CUT_LINE" "$INDEX_FILE_ABS" \
    | sed -e '/<!-- README.txt start -->/,/<!-- README.txt end -->/d' \
    >"$TMP_FILE_ABS"

# Render README.txt
if [ -s "$README_FILE_ABS" ]
then
    echo "<!-- README.txt start -->" >>"$TMP_FILE_ABS"
    echo "<h2 id=\"note\">Note</h2>" >>"$TMP_FILE_ABS"
    echo "<pre class=\"note-box\">" >>"$TMP_FILE_ABS"
    cat "$README_FILE_ABS" >>"$TMP_FILE_ABS"
    echo "</pre>" >>"$TMP_FILE_ABS"
    echo "<!-- README.txt end -->" >>"$TMP_FILE_ABS"
fi

# Render part after the README.txt
tail -n +"$((INDEX_CUT_LINE+1))" "$INDEX_FILE_ABS" \
    | sed -e '/<!-- README.txt start -->/,/<!-- README.txt end -->/d' \
    >>"$TMP_FILE_ABS"

mv "$TMP_FILE_ABS" "$INDEX_FILE_ABS"

compute_checksums

finalize 0