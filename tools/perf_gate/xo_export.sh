#!/bin/bash
# Export one XOffsetDatastructure commit with its pinned dependencies into a fresh
# directory, from local mirrors (macOS bash 3.2 and Linux). Replaces per-batch
# `git clone` + `git submodule update --init --recursive`, which filled the Mac's system disk
# and often timed out on the recursive Boost fetch.
#
#   xo_export.sh COMMIT DEST
#
# DEST gets: the repository tree at COMMIT (git archive), external/typelayout at the
# commit's gitlink, and external/boost with only what the build reads: LICENSE_1_0.txt and
# libs/<lib>/include of the libraries cmake/XOffsetDependencyLock.cmake pins. CMake checks
# those bytes against the lock, so no .git metadata is needed.
#
# Mirrors live under XO_GIT (default /Volumes/XOffsetCS/git) and are created on first use:
#   XOffsetDatastructure.git, TypeLayout.git   bare mirrors, fetched when a commit is missing
#   boost-export/<boost gitlink>/              the Boost subset, built once per gitlink
set -euo pipefail
[ $# -eq 2 ] || { sed -n '2,17p' "$0"; exit 2; }
COMMIT=$1; DEST=$2
XO_GIT=${XO_GIT:-/Volumes/XOffsetCS/git}
BOOST_LIBS="container core config throw_exception static_assert container_hash type_traits winapi intrusive predef"
mkdir -p "$XO_GIT"
[ ! -e "$DEST" ] || { echo "$DEST exists" >&2; exit 1; }

# mirror NAME URL SHA: make sure the bare mirror NAME has commit SHA.
mirror() {
    local m=$XO_GIT/$1
    [ -d "$m" ] || git clone -q --mirror "$2" "$m"
    if ! git -C "$m" cat-file -e "$3^{commit}" 2>/dev/null; then
        git -C "$m" fetch -q origin || true
        git -C "$m" cat-file -e "$3^{commit}" 2>/dev/null || git -C "$m" fetch -q origin "$3"
    fi
    git -C "$m" rev-parse "$3^{commit}"
}

full=$(mirror XOffsetDatastructure.git https://github.com/ximicpp/XOffsetDatastructure.git "$COMMIT")
X=$XO_GIT/XOffsetDatastructure.git
gitlink() { git -C "$X" ls-tree "$full" "$1" | awk '$2 == "commit" {print $3}'; }
tl=$(gitlink external/typelayout); bl=$(gitlink external/boost)
[ -n "$tl" ] && [ -n "$bl" ] || { echo "$full: missing external/typelayout or external/boost gitlink" >&2; exit 1; }

mkdir -p "$DEST"
git -C "$X" archive "$full" | tar -x -C "$DEST"
mirror TypeLayout.git https://github.com/ximicpp/TypeLayout.git "$tl" > /dev/null
mkdir -p "$DEST/external/typelayout"
git -C "$XO_GIT/TypeLayout.git" archive "$tl" | tar -x -C "$DEST/external/typelayout"

B=$XO_GIT/boost-export/$bl
if [ ! -e "$B/.complete" ]; then
    tmp=$XO_GIT/boost-export/.tmp-$bl-$$
    rm -rf "$tmp" "$B"; mkdir -p "$tmp/super" "$B/libs"
    git -C "$tmp/super" init -q
    git -C "$tmp/super" fetch -q --depth 1 https://github.com/boostorg/boost.git "$bl"
    git -C "$tmp/super" show FETCH_HEAD:LICENSE_1_0.txt > "$B/LICENSE_1_0.txt"
    for lib in $BOOST_LIBS; do
        ls=$(git -C "$tmp/super" ls-tree FETCH_HEAD "libs/$lib" | awk '$2 == "commit" {print $3}')
        [ -n "$ls" ] || { echo "boost $bl has no libs/$lib" >&2; exit 1; }
        mkdir -p "$tmp/$lib" "$B/libs/$lib"
        git -C "$tmp/$lib" init -q
        git -C "$tmp/$lib" fetch -q --depth 1 "https://github.com/boostorg/$lib.git" "$ls"
        git -C "$tmp/$lib" archive FETCH_HEAD include | tar -x -C "$B/libs/$lib"
        echo "$lib $ls" >> "$B/REVISIONS"
    done
    rm -rf "$tmp"
    touch "$B/.complete"
fi
mkdir -p "$DEST/external/boost"
cp -R "$B/LICENSE_1_0.txt" "$B/libs" "$DEST/external/boost/"
echo "exported $full (tree $(git -C "$X" rev-parse "$full^{tree}")) typelayout $tl boost $bl -> $DEST"
