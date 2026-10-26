#!/bin/bash
# Publishes one mac_queue.sh batch to a git branch the cloud threads can fetch, so results
# reach them while the Mac session is paused (macOS bash 3.2 and Linux).
#
#   XOQ_PUBLISH=<path>/mac_publish_git.sh mac_queue.sh start    the runner calls it per batch
#   mac_publish_git.sh BATCH_DIR                                 by hand, e.g. to backfill
#
# Settings (in the runner's environment):
#   XOQ_PUB_REMOTE  git URL to push to (required); the Mac's own git credentials are used
#   XOQ_PUB_BRANCH  branch, default mac-results; created as an orphan branch if missing
#   XOQ_PUB_MAX_MB  largest file copied, default 20; bigger ones are listed in
#                   NOT_PUBLISHED.txt with size and sha256 and stay on the Mac
#
# The branch holds one folder per batch, <NNN>-<NAME>/, with state, needs, run.sh.sha256,
# run.log (its last XOQ_PUB_MAX_MB if longer) and out/, plus QUEUE.md with the output of
# `mac_queue.sh status`. Each call is one commit; a rejected push starts over from the
# remote branch, up to three times. The working clone is $XOQ/publish (depth 1).
#
# Cloud side: git fetch --depth 1 origin mac-results && git show FETCH_HEAD:QUEUE.md
set -u
d=$(cd "${1:?usage: mac_publish_git.sh BATCH_DIR}" && pwd) || exit 2
XOQ=${XOQ:-$(dirname "$(dirname "$d")")}
REMOTE=${XOQ_PUB_REMOTE:?set XOQ_PUB_REMOTE to the git URL to push to}
BRANCH=${XOQ_PUB_BRANCH:-mac-results}
MAX=$(( ${XOQ_PUB_MAX_MB:-20} * 1024 * 1024 ))
QUEUE_SH=${XOQ_QUEUE_SH:-$(cd "$(dirname "$0")" && pwd)/mac_queue.sh}
W=$XOQ/publish
name=$(basename "$d")

g() { git -C "$W" "$@"; }
if [ ! -d "$W/.git" ]; then
    git init -q "$W" && g remote add origin "$REMOTE" || exit 1
fi
g remote set-url origin "$REMOTE"
[ -n "$(g config user.name)" ] || g config user.name "XOffset Mac queue"
[ -n "$(g config user.email)" ] || g config user.email "xo-mac-queue@localhost"

copy_batch() {
    local dst=$W/$name f size
    rm -rf "$dst"; mkdir -p "$dst"
    for f in state needs run.sh.sha256; do [ -f "$d/$f" ] && cp "$d/$f" "$dst/"; done
    if [ -f "$d/run.log" ]; then
        size=$(wc -c < "$d/run.log" | tr -d ' ')
        if [ "$size" -le "$MAX" ]; then cp "$d/run.log" "$dst/run.log"
        else tail -c "$MAX" "$d/run.log" > "$dst/run.log"; echo "run.log $size (last $MAX bytes kept)" >> "$dst/NOT_PUBLISHED.txt"; fi
    fi
    [ -d "$d/out" ] || return 0
    ( cd "$d/out" && find . -type f ) | sed 's#^\./##' | while read -r f; do
        size=$(wc -c < "$d/out/$f" | tr -d ' ')
        if [ "$size" -le "$MAX" ]; then
            mkdir -p "$dst/out/$(dirname "$f")" && cp "$d/out/$f" "$dst/out/$f"
        else
            echo "out/$f $size $(shasum -a 256 "$d/out/$f" | awk '{print $1}')" >> "$dst/NOT_PUBLISHED.txt"
        fi
    done
}

for attempt in 1 2 3; do
    if g fetch -q --depth 1 origin "$BRANCH" 2>/dev/null; then
        g checkout -q -B "$BRANCH" FETCH_HEAD && g reset -q --hard FETCH_HEAD || exit 1
    elif ! g rev-parse -q --verify HEAD > /dev/null; then
        g checkout -q --orphan "$BRANCH" || exit 1
    fi
    copy_batch || exit 1
    {
        echo "# Mac queue"
        echo
        echo "Updated $(date -u +%Y-%m-%dT%H:%M:%SZ) after $name: $(head -n 1 "$d/state" 2>/dev/null)"
        echo
        echo '```'
        [ -f "$QUEUE_SH" ] && XOQ=$XOQ /bin/bash "$QUEUE_SH" status
        echo '```'
    } > "$W/QUEUE.md"
    g add -A
    g commit -q -m "$name: $(head -n 1 "$d/state" 2>/dev/null)" || { echo "nothing new to publish for $name"; exit 0; }
    if g push -q origin "HEAD:refs/heads/$BRANCH"; then
        echo "published $name to $BRANCH ($(g rev-parse --short HEAD))"; exit 0
    fi
    echo "push of $name rejected (attempt $attempt); starting over from the remote branch"
    sleep $((attempt * 15))
done
exit 1
