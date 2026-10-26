#!/bin/bash
# Unattended batch queue for the Mac (macOS bash 3.2; no GNU tools needed).
#
# The Mac session pauses for hours at a time, and until now nothing started the next batch
# while it was away (10-24..10-26: about 8% of the time was first-time timing, 59% was
# session pauses). This runner keeps going without the session: it runs queued batch
# scripts one after another, each under one machine-wide lock, so a build or a CTest can
# never overlap a timed run (the cause of the 10-26 COMPOSED SLG rerun).
#
#   mac_queue.sh start              start the runner in the background (no-op if running)
#   mac_queue.sh restart            let the runner finish its current batch, then start this
#                                   version of the script in its place
#   mac_queue.sh add NAME FILE [SHA256] [--if COND]...
#                                   queue FILE as batch NAME; with SHA256 the file must match
#   mac_queue.sh add-batch MANIFEST queue every line of MANIFEST, or nothing if one line is bad
#   mac_queue.sh wait [SECONDS]     block until a batch changes state, at most SECONDS (2400)
#   mac_queue.sh status             queue, states, free disk, runner pid
#   mac_queue.sh stop               stop after the current batch (the batch is not interrupted)
#   mac_queue.sh cancel NAME        drop a batch that has not started
#   mac_queue.sh lock CMD [ARG...]  run CMD under the same lock (builds and tests outside the queue)
#
# --if COND runs the batch only when an earlier batch ended a given way, so both branches of
# a decision can be queued at once: NAME:ok (exit 0), NAME:fail (exit != 0) or NAME:<rc>.
# NAME is the latest batch of that name. Batches run in queue order, so the earlier batch has
# ended by then; when a condition does not hold the batch is marked skipped.
#
# MANIFEST lines are `NAME FILE SHA256 [--if COND]...`; blank lines and # comments are
# ignored. A FILE that does not exist as written is looked up by its base name next to
# MANIFEST, so a manifest that names /mnt/project-files/... paths works after its files are
# downloaded into one local folder. Paths must not contain spaces.
#
# wait is the session's heartbeat. The project pauses a session on this Mac after about 60
# minutes without activity in its thread (every pause from 10-20 to 10-26 came 60-64 minutes
# after the last message), and only a message from susu resumes it. A session that leaves
# `mac_queue.sh wait 2400` running in the background at the end of each turn is woken when a
# batch starts or ends, or after 40 minutes at the latest; a progress update then keeps it
# online, and results are handed back as soon as they exist.
#
# Layout (XOQ, default ~/workspace/xo-queue, inside the folder the cloud threads can read):
#   queue/<NNN>-<NAME>/run.sh       the batch; runs with cwd = its directory and
#                                   XOQ, XOQ_NAME, XOQ_DIR, XOQ_OUT (results to hand back) set
#   queue/<NNN>-<NAME>/needs        its --if conditions, one per line
#   queue/<NNN>-<NAME>/state        queued | running <utc> | done <rc> <utc> | skipped <why> |
#                                   cancelled
#   queue/<NNN>-<NAME>/run.log      stdout and stderr of run.sh
#   queue/<NNN>-<NAME>/out/         RESULT.md, tar.gz and anything else run.sh copies there
#   runner.log, runner.pid, runner.version, STOP, publish.log
#
# A batch script is a complete, unattended version of its MAC_STEPS file: it stops itself
# (exit != 0) wherever the steps file says "stop and report", it waits for the quiet gate
# instead of timing out (mac_gate --quiet-timeout 43200), and it deletes its own build trees
# at the end. A failed batch does not stop the queue; the next batch is independent.
# The runner checks free space before each batch and waits while it is below XOQ_MIN_FREE_GB
# on XOQ_DISK (default /Volumes/XOffsetCS, 20 GB); it never deletes anything itself.
#
# With XOQ_PUBLISH set to an executable when the runner starts, the runner calls
# `$XOQ_PUBLISH <batch dir>` when a batch starts, ends or is skipped, before the next batch
# begins, so results reach a place the cloud threads can read without the session
# (mac_publish_git.sh pushes them to a git branch). A failed publish is logged in
# publish.log and does not stop the queue.
set -u
XOQ=${XOQ:-$HOME/workspace/xo-queue}
XOQ_DISK=${XOQ_DISK:-/Volumes/XOffsetCS}
XOQ_MIN_FREE_GB=${XOQ_MIN_FREE_GB:-20}
XOQ_PUBLISH=${XOQ_PUBLISH:-}
VERSION=2
LOCK=$XOQ/.lock
Q=$XOQ/queue
mkdir -p "$Q"
export XOQ

utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }
say() { echo "$(utc) $*" >> "$XOQ/runner.log"; }
free_gb() { df -g "$1" 2>/dev/null | awk 'NR==2 {print $4}'; }
running_pid() {
    local p
    p=$(cat "$XOQ/runner.pid" 2>/dev/null) || return 1
    [ -n "$p" ] && kill -0 "$p" 2>/dev/null && echo "$p"
}
batches() { ls -d "$Q"/*/ 2>/dev/null | sort | sed 's#/$##'; }
next_batch() {
    local d
    for d in $(batches); do
        [ "$(head -n 1 "$d/state" 2>/dev/null)" = queued ] && { echo "$d"; return 0; }
    done
    return 1
}
valid_name() { case $1 in *[!A-Za-z0-9._-]*|'') return 1 ;; esac; return 0; }
valid_cond() {
    [ "${1%:*}" != "$1" ] && valid_name "${1%:*}" || return 1
    case ${1##*:} in ok|fail) return 0 ;; ''|*[!0-9]*) return 1 ;; esac
    return 0
}
# Prints why batch $1 must not run; prints nothing when every condition in its needs holds.
unmet() {
    local need name want dep state rc
    [ -f "$1/needs" ] || return 0
    while read -r need; do
        [ -n "$need" ] || continue
        name=${need%:*}; want=${need##*:}
        dep=$(ls -d "$Q"/*-"$name" 2>/dev/null | sort | tail -n 1)
        state=$(head -n 1 "$dep/state" 2>/dev/null)
        case $state in
            done\ *) rc=$(echo "$state" | awk '{print $2}') ;;
            *) echo "$need: $name is ${state:-missing}"; return 0 ;;
        esac
        case $want in
            ok) [ "$rc" = 0 ] || { echo "$need: $name exit $rc"; return 0; } ;;
            fail) [ "$rc" != 0 ] || { echo "$need: $name exit 0"; return 0; } ;;
            *) [ "$rc" = "$want" ] || { echo "$need: $name exit $rc"; return 0; } ;;
        esac
    done < "$1/needs"
}
publish() {
    [ -n "$XOQ_PUBLISH" ] || return 0
    { echo "== $(utc) $(basename "$1"): $(head -n 1 "$1/state")"; "$XOQ_PUBLISH" "$1"; } >> "$XOQ/publish.log" 2>&1 ||
        say "publish failed for $(basename "$1") (see publish.log)"
}
snapshot() {
    local d
    for d in $(batches); do echo "$(basename "$d") $(head -n 1 "$d/state" 2>/dev/null)"; done
}
# Checks NAME FILE SHA256 COND... and prints the file to queue (MANIFEST_DIR is the fallback).
check_one() {
    local name=$1 file=$2 want=$3 got c
    shift 3
    valid_name "$name" || { echo "$name: NAME takes letters, digits, . _ - only" >&2; return 2; }
    [ -f "$file" ] || [ -z "${MANIFEST_DIR:-}" ] || file=$MANIFEST_DIR/$(basename "$file")
    [ -f "$file" ] || { echo "$name: no such file: $file" >&2; return 2; }
    got=$(shasum -a 256 "$file" | awk '{print $1}')
    if [ -n "$want" ] && [ "$got" != "$want" ]; then echo "$name: sha256 mismatch: $got != $want" >&2; return 1; fi
    for c in "$@"; do
        valid_cond "$c" || { echo "$name: bad condition $c (NAME:ok, NAME:fail or NAME:<rc>)" >&2; return 2; }
    done
    echo "$file"
}
# A runner from before --if would run a conditional batch unconditionally.
check_runner() {
    [ -n "$1" ] || return 0
    running_pid > /dev/null || return 0
    [ "$(cat "$XOQ/runner.version" 2>/dev/null)" = "$VERSION" ] && return 0
    [ -e "$XOQ/STOP" ] && return 0
    echo "the running runner predates --if; run mac_queue.sh restart first" >&2
    return 2
}
enqueue() {
    local name=$1 file=$2 last n d got c
    shift 2
    got=$(shasum -a 256 "$file" | awk '{print $1}')
    last=$(ls "$Q" 2>/dev/null | sort | tail -n 1 | cut -c1-3)
    n=$(printf '%03d' $(( 10#${last:-0} + 1 )))
    d=$Q/$n-$name
    mkdir -p "$d" && cp "$file" "$d/run.sh" && echo "$got" > "$d/run.sh.sha256" || return 1
    : > "$d/needs"
    for c in "$@"; do echo "$c" >> "$d/needs"; done
    echo queued > "$d/state"
    echo "queued $n-$name (sha256 $got)${1:+ if $*}"
}
# Splits `[SHA256] [--if COND]...` into WANT and CONDS.
parse_tail() {
    WANT=; CONDS=
    while [ $# -gt 0 ]; do
        case $1 in
            --if) [ $# -ge 2 ] || { echo "--if needs NAME:ok, NAME:fail or NAME:<rc>" >&2; return 2; }
                  CONDS="$CONDS $2"; shift 2 ;;
            *) [ -z "$WANT" ] || { echo "unexpected argument: $1" >&2; return 2; }
               WANT=$1; shift ;;
        esac
    done
}

loop() {
    echo $$ > "$XOQ/runner.pid"
    echo "$VERSION" > "$XOQ/runner.version"
    say "runner $$ started (version $VERSION)${XOQ_PUBLISH:+, publishing with $XOQ_PUBLISH}"
    while :; do
        if [ -e "$XOQ/STOP" ]; then rm -f "$XOQ/STOP"; say "runner $$ stopped"; rm -f "$XOQ/runner.pid"; exit 0; fi
        d=$(next_batch) || { sleep 60; continue; }
        name=$(basename "$d")
        why=$(unmet "$d")
        if [ -n "$why" ]; then
            echo "skipped $why" > "$d/state"
            say "skip $name ($why)"
            publish "$d"; continue
        fi
        f=$(free_gb "$XOQ_DISK")
        if [ -z "$f" ] || [ "$f" -lt "$XOQ_MIN_FREE_GB" ]; then
            say "waiting: ${f:-no} GB free on $XOQ_DISK (< $XOQ_MIN_FREE_GB) before $name"
            sleep 600; continue
        fi
        echo "running $(utc)" > "$d/state"
        say "start $name"
        mkdir -p "$d/out"
        publish "$d"
        # -i: no idle sleep while a batch runs; the display may still sleep (that is what we want).
        ( cd "$d" && XOQ_NAME=$name XOQ_DIR=$d XOQ_OUT=$d/out \
            lockf -k "$LOCK" caffeinate -i /bin/bash "$d/run.sh" ) > "$d/run.log" 2>&1 < /dev/null
        rc=$?
        echo "done $rc $(utc)" > "$d/state"
        say "end $name rc=$rc"
        publish "$d"
    done
}

cmd=${1:-status}; shift || true
case $cmd in
    start)
        if p=$(running_pid); then echo "runner already running (pid $p)"; exit 0; fi
        if [ -n "$XOQ_PUBLISH" ] && [ ! -x "$XOQ_PUBLISH" ]; then echo "XOQ_PUBLISH is not executable: $XOQ_PUBLISH" >&2; exit 2; fi
        rm -f "$XOQ/STOP"
        nohup /bin/bash "$0" _loop > /dev/null 2>&1 < /dev/null &
        sleep 1; echo "runner started (pid $(cat "$XOQ/runner.pid" 2>/dev/null))${XOQ_PUBLISH:+, publishing with $XOQ_PUBLISH}" ;;
    restart)
        p=$(running_pid) || exec /bin/bash "$0" start
        if [ -n "$XOQ_PUBLISH" ] && [ ! -x "$XOQ_PUBLISH" ]; then echo "XOQ_PUBLISH is not executable: $XOQ_PUBLISH" >&2; exit 2; fi
        touch "$XOQ/STOP"
        nohup /bin/bash -c 'while kill -0 "$1" 2>/dev/null; do sleep 20; done; exec /bin/bash "$2" start' \
            _ "$p" "$0" > /dev/null 2>&1 < /dev/null &
        echo "runner $p stops after its current batch; this script starts in its place then" ;;
    _loop) loop ;;
    add)
        [ $# -ge 2 ] || { echo "usage: mac_queue.sh add NAME FILE [SHA256] [--if COND]..." >&2; exit 2; }
        name=$1; file=$2; shift 2
        parse_tail "$@" || exit 2
        # shellcheck disable=SC2086
        file=$(check_one "$name" "$file" "$WANT" $CONDS) || exit $?
        check_runner "$CONDS" || exit 2
        # shellcheck disable=SC2086
        enqueue "$name" "$file" $CONDS || exit 1
        running_pid > /dev/null || echo "note: the runner is not running; mac_queue.sh start" ;;
    add-batch)
        [ $# -eq 1 ] && [ -f "$1" ] || { echo "usage: mac_queue.sh add-batch MANIFEST" >&2; exit 2; }
        MANIFEST_DIR=$(cd "$(dirname "$1")" && pwd)
        # Check every line first, so a bad line queues nothing.
        checked=$(mktemp "${TMPDIR:-/tmp}/xoq.XXXXXX") || exit 1
        bad=0; lines=0; anycond=
        while read -r name file rest; do
            case $name in ''|\#*) continue ;; esac
            lines=$((lines + 1))
            # shellcheck disable=SC2086
            parse_tail $rest || { bad=1; continue; }
            [ -n "$WANT" ] || { echo "$name: every manifest line needs a SHA256" >&2; bad=1; continue; }
            # shellcheck disable=SC2086
            resolved=$(check_one "$name" "$file" "$WANT" $CONDS) || { bad=1; continue; }
            anycond="$anycond$CONDS"
            echo "$name $resolved $CONDS" >> "$checked"
        done < "$1"
        if [ "$bad" != 0 ] || [ "$lines" = 0 ]; then
            rm -f "$checked"; echo "nothing queued ($lines lines; see the errors above)" >&2; exit 1
        fi
        check_runner "$anycond" || { rm -f "$checked"; exit 2; }
        while read -r name file conds; do
            # shellcheck disable=SC2086
            enqueue "$name" "$file" $conds || { rm -f "$checked"; exit 1; }
        done < "$checked"
        rm -f "$checked"
        running_pid > /dev/null || echo "note: the runner is not running; mac_queue.sh start" ;;
    wait)
        secs=${1:-2400}
        case $secs in ''|*[!0-9]*) echo "usage: mac_queue.sh wait [SECONDS]" >&2; exit 2 ;; esac
        before=$(snapshot); t=0
        while [ "$t" -lt "$secs" ]; do
            sleep 20; t=$((t + 20))
            now=$(snapshot)
            if [ "$now" != "$before" ]; then
                echo "changed after ${t}s:"
                old=$(mktemp "${TMPDIR:-/tmp}/xoq.XXXXXX") && printf '%s\n' "$before" > "$old"
                printf '%s\n' "$now" | grep -Fxv -f "$old" | sed 's/^/  /'
                rm -f "$old"
                exec /bin/bash "$0" status
            fi
        done
        echo "heartbeat: no change in ${secs}s"
        exec /bin/bash "$0" status ;;
    cancel)
        d=$(ls -d "$Q"/*-"${1:-}" 2>/dev/null | sort | tail -n 1)
        [ -n "$d" ] && [ "$(head -n 1 "$d/state")" = queued ] || { echo "no queued batch named ${1:-}" >&2; exit 1; }
        echo cancelled > "$d/state"; echo "cancelled $(basename "$d")" ;;
    stop) touch "$XOQ/STOP"; echo "runner stops after the current batch" ;;
    lock) exec lockf -k "$LOCK" "$@" ;;
    status)
        if p=$(running_pid); then echo "runner: pid $p (version $(cat "$XOQ/runner.version" 2>/dev/null || echo 1))"
        else echo "runner: not running"; fi
        [ -e "$XOQ/STOP" ] && echo "runner: stopping after the current batch"
        echo "free: $(free_gb "$XOQ_DISK") GB on $XOQ_DISK, $(free_gb "$HOME") GB on $HOME"
        for d in $(batches); do
            printf '%-40s %s' "$(basename "$d")" "$(head -n 1 "$d/state")"
            [ -s "$d/needs" ] && printf '  (if %s)' "$(tr '\n' ' ' < "$d/needs" | sed 's/ $//')"
            echo
        done ;;
    *) sed -n '2,60p' "$0"; exit 2 ;;
esac
