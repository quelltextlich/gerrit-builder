#!/bin/bash

#---------------------------------------------------------------------
source "$(dirname "$0")/common.inc"
#---------------------------------------------------------------------

DATE="$(date --utc +'%Y-%m-%d')"

FORCE=no
PULL=yes
CHECKOUT=yes
CLEAN=yes

print_help() {
    cat <<EOF
$0 ARGUMENTS

ARGUMENTS:
  --help             - prints this page
  --branch BRANCH    - Build branch BRANCH instead of the default, which is
                       inferred from the basename of the directory, with
                       "master" as fallback.
  --force            - Overwrite eventual existing artifacts target directory
  --no-checkout      - Don't 'git checkout' before building
  --no-clean         - Don't clean before building
  --no-pull          - Don't 'git pull' before building
  --no-repo-mangling - Neither 'git checkout' nor 'git pull' before building
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
        * )
            error "Unknown argument '$ARGUMENT'"
            ;;
    esac
done

TARGET_DIR_ABS="$ARTIFACTS_NIGHTLY_DIR_ABS/$BRANCH/$DATE"
FILES_DIR_RELT="files"
TARGET_FILE_DIR_ABS="$TARGET_DIR_ABS/$FILES_DIR_RELT"

if [ -e "$TARGET_DIR_ABS" -a "$FORCE" = yes ]
then
    rm -rf "$TARGET_DIR_ABS"
fi

if [ -e "$TARGET_DIR_ABS" ]
then
    error "'$TARGET_DIR_ABS' already exists"
fi

mkdir -p "$TARGET_FILE_DIR_ABS"

info "Date: $DATE"
info "Branch: $BRANCH"

HTML_SPLIT="<p>— <a href=\"..\">Go to parent directory</a> — <a href=\"$FILES_DIR_RELT\">View all files</a> —</p>"

cat_target_html <<EOF
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
      <html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
  <title>$DATE gerrit $BRANCH build</title>
  <meta http-equiv="Content-type" content="text/html;charset=UTF-8" />
  <meta name="description" content="Build of $BRANCH commitish of gerrit from $DATE" />
  <meta name="keywords" content="gerrit, jar, $BRANCH" />
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
  </style>
</head>
<body>

<h1>$DATE build of $BRANCH of gerrit</h1>

$HTML_SPLIT

<h2 id="summary">Build summary</h2>

<table>
<tr class="failed"><th class="th-failed">Build status</th><td><img src="$IMAGE_BASE_URL/failed.png" alt="Build failed" /> failed</td></tr>
<tr><th>Build date</th><td>$DATE</td></tr>
<tr><th>Commitish</th><td>$BRANCH</td></tr>
<tr><th>API version</th><td>---</td></tr>
</table>

<h2 id="artifacts">Artifacts</h2>

<table>
<tr>
<th>Status</th>
<th>Artifact</th>
<th>Size</th>
<th>Buck log</th>
<th>Repository</th>
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

describe_repo "withdocs.war"


BUCK_WANTED_VERSION="$(cat "$GERRIT_DIR_ABS/.buckversion")"

if [ ! -z "$(which buckd)" ]
then
    buckd --kill || true
fi

if [ "$BUCK_WANTED_VERSION" != "$(run_buck --version 2>/dev/null | cut -f 3 -d ' ')" ]
then
    section "Rebuilding buck"
    pushd "$BUCK_DIR_ABS" >/dev/null
    git checkout master
    git pull
    git checkout "$BUCK_WANTED_VERSION"
    ant
    popd >/dev/null
fi

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
    describe_repo "$EXTRA_PLUGIN_NAME.jar"
    popd >/dev/null

    if [ -h "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME" ]
    then
        rm "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME"
    fi

    if [ ! -e "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME" ]
    then
        ln -s "$EXTRA_PLUGIN_DIR_ABS" "$GERRIT_DIR_ABS/plugins/$EXTRA_PLUGIN_NAME"
    fi
done

cat >"$TARGET_FILE_DIR_ABS/build_description.json" <<EOF
{
$REPO_DESCRIPTIONS
}
EOF

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

run_buck_build "gerrit" "//:withdocs" "withdocs.war"

for EXTRA_PLUGIN_DIR_ABS in "$EXTRA_PLUGINS_DIR_ABS"/*
do
    EXTRA_PLUGIN_NAME="$(basename "$EXTRA_PLUGIN_DIR_ABS")"

    run_buck_build "$EXTRA_PLUGIN_NAME" "plugins/$EXTRA_PLUGIN_NAME:$EXTRA_PLUGIN_NAME" "plugins/$EXTRA_PLUGIN_NAME/$EXTRA_PLUGIN_NAME.jar"
done

#section "Building api"
#echo run_buck build api

pushd "$TARGET_FILE_DIR_ABS" >/dev/null
sha1sum * >sha1sums.txt
popd >/dev/null

echo_file_target_html "ok" "sha1sums.txt"
echo_file_target_html "ok" "build_description.json"

echo_target_html "</table>"

HTML_FAILED_MARKER_PRE=
HTML_FAILED_MARKER_POST=
STATUS=failed
if [ "$ARTIFACTS_FAILED" = "0" ]
then
    STATUS=ok
else
    if [ "$ARTIFACTS_OK" != "0" ]
    then
        STATUS="failed partially"
    fi
    HTML_FAILED_MARKER_PRE="<span class=\"failed\">"
    HTML_FAILED_MARKER_POST="</span>"
fi
cat_target_html <<EOF
<p>(Total artifacts: $ARTIFACTS_TOTAL; ok artifacts: $ARTIFACTS_OK, ${HTML_FAILED_MARKER_PRE}failed artifacts: $ARTIFACTS_FAILED${HTML_FAILED_MARKER_POST})</p>
EOF

cat_target_html <<EOF
$HTML_SPLIT

</body>
</html>
EOF

sed -i \
    -e '/Build.status/s/failed/'"$STATUS"'/g' \
    -e '/API version/s/---/'"$API_VERSION"'/g' \
    "$TARGET_DIR_ABS/index.html"

finalize
