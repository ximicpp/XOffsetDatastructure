#!/usr/bin/env bash
# Device mesh demo, cross-site run through a mailbox, cloud side
# (INTERFACES.md section 6). The cloud and the Mac have no live link, so the
# hub plans the cloud x Mac jobs with fixed device ids, the devices answer
# Job and Hop files (xo_peer --mailbox), and the two sites swap files once:
#
#   run_mailbox.sh plan [--out DIR] [COLUMN ...]
#       xo_hub --mailbox-plan for the cloud columns (ids 1.., two devices per
#       column as in run_mesh.sh) and tools/mac_devices.txt (ids 101..):
#       jobs [cloud, Mac, cloud], [Mac, cloud, cloud], one chain per type
#       [cloud, every Mac device, cloud], evil both ways. DIR/plan/box.
#   run_mailbox.sh pre [--out DIR]
#       the cloud devices answer every file they can (rounds until none is
#       left), then DIR/mac_inbox.tar.gz gets the files for the Mac devices
#       (inbox/, devices.txt, INBOX_INFO.txt) for run_mesh_mac.sh.
#   run_mailbox.sh post [--out DIR] MAC_OUT [MAC_OUT ...]
#       MAC_OUT: run_mesh_mac.sh's output directory, or a .tar.gz of one or
#       more of them; every mailbox/outbox.tar.gz in them (a later run with
#       XO_MAILBOX_PRIOR adds the files its first run lacked) goes into the
#       box (an existing file must be byte-identical), the cloud devices
#       answer the rest, then xo_hub --mailbox-score (DIR/score/hub) and
#       DIR/CROSS_RESULT.md (mesh_report.py over the cloud mesh
#       XMESH_CLOUD_RUN, the mailbox score and every Mac live mesh, a
#       mesh/ with hub/summary.txt, in the MAC_OUTs).
#   run_mailbox.sh fake-mac [--out DIR]
#       cloud dry run of the Mac side: the linux-x64-gcc programs play the
#       Mac devices on mac_inbox.tar.gz and write DIR/fake-mac/ as
#       run_mesh_mac.sh would (mailbox/outbox.tar.gz).
#
# Programs come from run_mesh.sh --build-only (XMESH_BUILD_ROOT). DIR
# defaults to <repo>/build/mesh-mailbox. Environment: XMESH_SEED (1),
# XMESH_LABEL, XMESH_CLOUD_RUN (cloud mesh output, default
# <repo>/build/mesh-run), XMESH_HUB_ARGS (extra --mailbox-plan arguments).
# Exit 0 when the step passed (post: every job), 1 otherwise, 2 on usage.
set -u

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
HERE=$REPO/demo/mesh/tools
BUILD_ROOT=${XMESH_BUILD_ROOT:-$REPO/build/mesh}
OUT=$REPO/build/mesh-mailbox
NDK=${ANDROID_NDK:-/opt/android-ndk-r29}
MSVC_ROOT=${XOFFSET_MSVC_ROOT:-/opt/msvc}
LABEL=${XMESH_LABEL:-cross-$(date -u +%Y%m%d-%H%M)}
STEP=${1:-}
[ $# -gt 0 ] && shift
MAC_OUT=""
COLUMNS=""
while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT=$2; shift 2 ;;
        -h|--help) sed -n '2,36p' "$0"; exit 0 ;;
        -*) echo "unknown option $1" >&2; exit 2 ;;
        *) if [ "$STEP" = post ]; then MAC_OUT="$MAC_OUT $1"; else COLUMNS="$COLUMNS $1"; fi; shift ;;
    esac
done
case "$STEP" in plan|pre|post|fake-mac) ;; *) sed -n '2,36p' "$0"; exit 2 ;; esac

# shellcheck source=mesh_columns.sh
. "$HERE/mesh_columns.sh"
export WINEDEBUG=-all
export XOFFSET_MSVC_ROOT=$MSVC_ROOT
HUB=$BUILD_ROOT/hub/bin/Release/xo_hub
[ -x "$HUB" ] || { echo "no hub at $HUB (run_mesh.sh --build-only first)" >&2; exit 1; }
PLAN=$OUT/plan
BOX=$PLAN/box
MAC_IDS=$(awk '!/^#/ && NF == 3 {print $1}' "$HERE/mac_devices.txt")

# rounds DEVICES_FILE BOX LOGDIR: every device ("ID NAME COLUMN PROGRAM") answers
# what it can, all at once, until a round writes no file.
rounds() {
    local devs=$1 box=$2 logs=$3 r=0 before after id name col prog bin
    mkdir -p "$logs"
    while :; do
        r=$((r + 1))
        before=$(ls "$box" | wc -l)
        while read -r id name col prog; do
            bin=$BUILD_ROOT/$col/bin/Release/$prog$(exe "$col")
            [ -f "$bin" ] || { echo "missing $bin" >> "$logs/$name.log"; continue; }
            # shellcheck disable=SC2046
            ( $(runner "$col") "$bin" --mailbox "$(path_for "$col" "$box")" "$(path_for "$col" "$box")" --id "$id" \
                  --name "$name" >> "$logs/$name.log" 2>&1 || echo "round $r: exit $?" >> "$logs/$name.log" ) &
        done < "$devs"
        wait
        after=$(ls "$box" | wc -l)
        echo "round $r: $((after - before)) new files"
        [ "$after" != "$before" ] || break
        [ $r -lt 80 ] || { echo "still writing after $r rounds" >&2; return 1; }
    done
    ! grep -l "^round [0-9]*: exit" "$logs"/*.log 2>/dev/null | sed 's/^/a device failed: /' | grep . >&2
}

case "$STEP" in
plan)
    if [ -z "$COLUMNS" ]; then
        for c in $ALL_COLUMNS; do
            if available "$c"; then COLUMNS="$COLUMNS $c"; else echo "skip $c: toolchain not on this host" >&2; fi
        done
    fi
    COLUMNS=$(echo $COLUMNS)
    [ -e "$PLAN" ] && { echo "$PLAN exists (an earlier plan: look before removing it)" >&2; exit 2; }
    mkdir -p "$OUT"
    : > "$OUT/cloud_devices.txt"
    : > "$OUT/devices.in"
    id=0
    for c in $COLUMNS; do
        for mode in exc noexc; do
            id=$((id + 1))
            prog=xo_peer; name=$c
            [ $mode = noexc ] && { prog=xo_peer_noexc; name=$c-noexc; }
            [ -f "$BUILD_ROOT/$c/bin/Release/$prog$(exe "$c")" ] || { echo "no $prog for $c (build it first)" >&2; exit 1; }
            echo "$id $name $c $prog" >> "$OUT/cloud_devices.txt"
            echo "$id $name cloud" >> "$OUT/devices.in"
        done
    done
    awk '!/^#/ && NF == 3' "$HERE/mac_devices.txt" >> "$OUT/devices.in"
    # shellcheck disable=SC2086
    "$HUB" --mailbox-plan "$PLAN" --devices "$OUT/devices.in" --run "$LABEL" --seed "${XMESH_SEED:-1}" \
        ${XMESH_HUB_ARGS:-} || exit 1
    echo "$LABEL" > "$OUT/label.txt"
    echo "plan: $(($(wc -l < "$PLAN/plan.csv") - 1)) jobs, $(ls "$BOX" | wc -l) Job files in $BOX"
    ;;
pre)
    [ -f "$PLAN/plan.csv" ] || { echo "no plan in $PLAN (run_mailbox.sh plan first)" >&2; exit 2; }
    echo "== cloud devices, before the Mac"
    rounds "$OUT/cloud_devices.txt" "$BOX" "$OUT/logs-pre" || exit 1
    rm -rf "$OUT/mac_inbox" "$OUT/mac_inbox.tar.gz"
    mkdir -p "$OUT/mac_inbox/inbox"
    for id in $MAC_IDS; do
        for f in "$BOX"/*_to"$id".xmjob "$BOX"/*_to"$id".xmhop; do [ -f "$f" ] && cp "$f" "$OUT/mac_inbox/inbox/"; done
    done
    cp "$PLAN/devices.txt" "$OUT/mac_inbox/devices.txt"
    {
        echo "label=$(cat "$OUT/label.txt")"
        echo "commit=$(git -C "$REPO" rev-parse HEAD)"
        echo "plan_sha256=$(sha256sum "$PLAN/plan.csv" | cut -c1-64)"
        echo "jobs=$(($(wc -l < "$PLAN/plan.csv") - 1))"
        echo "inbox_files=$(ls "$OUT/mac_inbox/inbox" | wc -l)"
        echo "inbox_bytes=$(du -sb "$OUT/mac_inbox/inbox" | cut -f1)"
        echo "written_utc=$(date -u +%FT%TZ)"
    } > "$OUT/mac_inbox/INBOX_INFO.txt"
    tar -czf "$OUT/mac_inbox.tar.gz" -C "$OUT" mac_inbox
    cat "$OUT/mac_inbox/INBOX_INFO.txt"
    echo "mac inbox: $OUT/mac_inbox.tar.gz $(sha256sum "$OUT/mac_inbox.tar.gz" | cut -c1-64)"
    ;;
fake-mac)
    [ -f "$OUT/mac_inbox.tar.gz" ] || { echo "no $OUT/mac_inbox.tar.gz (run_mailbox.sh pre first)" >&2; exit 2; }
    F=$OUT/fake-mac
    rm -rf "$F"
    mkdir -p "$F/work" "$F/out/mailbox"
    tar -xzf "$OUT/mac_inbox.tar.gz" -C "$F/work"
    cp -R "$F/work/mac_inbox/inbox" "$F/work/box"
    awk '!/^#/ && NF == 3 {p = ($2 ~ /-noexc$/) ? "xo_peer_noexc" : "xo_peer"; print $1, $2, "linux-x64-gcc", p}' \
        "$HERE/mac_devices.txt" > "$F/work/devices"
    rounds "$F/work/devices" "$F/work/box" "$F/out/mailbox/logs" || exit 1
    mkdir -p "$F/work/outbox"
    for f in "$F/work/box"/*; do
        [ -e "$F/work/mac_inbox/inbox/$(basename "$f")" ] || cp "$f" "$F/work/outbox/"
    done
    tar -czf "$F/out/mailbox/outbox.tar.gz" -C "$F/work" outbox
    echo "fake Mac outbox: $(ls "$F/work/outbox" | wc -l) files, $F/out/mailbox/outbox.tar.gz"
    ;;
post)
    [ -n "$MAC_OUT" ] || { echo "post needs MAC_OUT (run_mesh_mac.sh output dir or .tar.gz)" >&2; exit 2; }
    [ -f "$PLAN/plan.csv" ] || { echo "no plan in $PLAN" >&2; exit 2; }
    rm -rf "$OUT/mac-out" "$OUT/mac-outbox"
    ROOTS=""; k=0
    for a in $MAC_OUT; do
        if [ -f "$a" ]; then
            k=$((k + 1)); mkdir -p "$OUT/mac-out/$k"
            tar -xzf "$a" -C "$OUT/mac-out/$k" || exit 1
            ROOTS="$ROOTS $OUT/mac-out/$k"
        elif [ -d "$a" ]; then ROOTS="$ROOTS $(cd "$a" && pwd)"
        else echo "no $a" >&2; exit 2; fi
    done
    # shellcheck disable=SC2086
    OUTBOXES=$(find $ROOTS -path '*/mailbox/outbox.tar.gz' | sort)
    # shellcheck disable=SC2086
    MESHES=$(find $ROOTS -path '*/mesh/hub/summary.txt' | sort)
    [ -n "$OUTBOXES" ] || { echo "no mailbox/outbox.tar.gz in$MAC_OUT" >&2; exit 2; }
    n=0; same=0; k=0
    for o in $OUTBOXES; do
        k=$((k + 1)); mkdir -p "$OUT/mac-outbox/$k"
        tar -xzf "$o" -C "$OUT/mac-outbox/$k" || exit 1
        echo "Mac outbox $o: $(ls "$OUT/mac-outbox/$k/outbox" | wc -l) files"
        for f in "$OUT/mac-outbox/$k/outbox"/*; do
            [ -f "$f" ] || continue
            b=$(basename "$f")
            if [ -e "$BOX/$b" ]; then
                if cmp -s "$f" "$BOX/$b"; then same=$((same + 1))
                # a later run's build line (same device, rebuilt) does not replace the first one's
                elif case $b in build_*.txt) true ;; *) false ;; esac; then
                    echo "note: $b differs in $o (kept the first): $(head -1 "$f")"
                else echo "conflict: the Mac's $b ($o) differs from the box's" >&2; exit 1; fi
            else
                cp "$f" "$BOX/$b"; n=$((n + 1))
            fi
        done
    done
    echo "Mac outbox: $n new files ($same already here)"
    echo "== cloud devices, after the Mac"
    rounds "$OUT/cloud_devices.txt" "$BOX" "$OUT/logs-post" || exit 1
    rm -rf "$OUT/score"
    mkdir -p "$OUT/score"
    "$HUB" --mailbox-score "$PLAN" --out "$OUT/score/hub" --run "$(cat "$OUT/label.txt")" > "$OUT/score/score.log" 2>&1
    RC=$?
    tail -6 "$OUT/score/hub/summary.txt"
    MAIN=${XMESH_CLOUD_RUN:-$REPO/build/mesh-run}
    # devices are listed under the id of the first run that has them: cloud mesh, mailbox, Mac mesh
    ALSO=""
    if [ -f "$MAIN/hub/summary.txt" ]; then ALSO="--also $OUT/score"; else MAIN=$OUT/score; fi
    for m in $MESHES; do ALSO="$ALSO --also $(dirname "$(dirname "$m")")"; done
    # shellcheck disable=SC2086
    python3 "$HERE/mesh_report.py" "$MAIN" $ALSO > "$OUT/CROSS_RESULT.md" || RC=1
    head -3 "$OUT/CROSS_RESULT.md"
    echo "report: $OUT/CROSS_RESULT.md"
    exit $RC
    ;;
esac
