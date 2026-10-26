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
#   mac_queue.sh add NAME FILE [SHA256]
#                                   queue FILE as batch NAME; with SHA256 the file must match
#   mac_queue.sh status             queue, states, free disk, runner pid
#   mac_queue.sh stop               stop after the current batch (the batch is not interrupted)
#   mac_queue.sh cancel NAME        drop a batch that has not started
#   mac_queue.sh lock CMD [ARG...]  run CMD under the same lock (builds and tests outside the queue)
#
# Layout (XOQ, default ~/workspace/xo-queue, inside the folder the cloud threads can read):
#   queue/<NNN>-<NAME>/run.sh       the batch; runs with cwd = its directory and
#                                   XOQ_NAME, XOQ_DIR, XOQ_OUT (results to hand back) set
#   queue/<NNN>-<NAME>/state        queued | running <utc> | done <rc> <utc> | cancelled
#   queue/<NNN>-<NAME>/run.log      stdout and stderr of run.sh
#   queue/<NNN>-<NAME>/out/         RESULT.md, tar.gz and anything else run.sh copies there
#   runner.log, runner.pid, STOP
#
# A batch script is a complete, unattended version of its MAC_STEPS file: it stops itself
# (exit != 0) wherever the steps file says "stop and report", it waits for the quiet gate
# instead of timing out (mac_gate --quiet-timeout 43200), and it deletes its own build trees
# at the end. A failed batch does not stop the queue; the next batch is independent.
# The runner checks free space before each batch and waits while it is below XOQ_MIN_FREE_GB
# on XOQ_DISK (default /Volumes/XOffsetCS, 20 GB); it never deletes anything itself.
set -u
XOQ=${XOQ:-$HOME/workspace/xo-queue}
XOQ_DISK=${XOQ_DISK:-/Volumes/XOffsetCS}
XOQ_MIN_FREE_GB=${XOQ_MIN_FREE_GB:-20}
LOCK=$XOQ/.lock
Q=$XOQ/queue
mkdir -p "$Q"

utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }
say() { echo "$(utc) $*" >> "$XOQ/runner.log"; }
free_gb() { df -g "$1" 2>/dev/null | awk 'NR==2 {print $4}'; }
running_pid() {
    local p
    p=$(cat "$XOQ/runner.pid" 2>/dev/null) || return 1
    [ -n "$p" ] && kill -0 "$p" 2>/dev/null && echo "$p"
}
next_batch() {
    local d
    for d in $(ls -d "$Q"/*/ 2>/dev/null | sort); do
        d=${d%/}
        [ "$(head -n 1 "$d/state" 2>/dev/null)" = queued ] && { echo "$d"; return 0; }
    done
    return 1
}

loop() {
    echo $$ > "$XOQ/runner.pid"
    say "runner $$ started"
    while :; do
        if [ -e "$XOQ/STOP" ]; then rm -f "$XOQ/STOP"; say "runner $$ stopped"; rm -f "$XOQ/runner.pid"; exit 0; fi
        d=$(next_batch) || { sleep 60; continue; }
        f=$(free_gb "$XOQ_DISK")
        if [ -z "$f" ] || [ "$f" -lt "$XOQ_MIN_FREE_GB" ]; then
            say "waiting: ${f:-no} GB free on $XOQ_DISK (< $XOQ_MIN_FREE_GB) before $(basename "$d")"
            sleep 600; continue
        fi
        name=$(basename "$d")
        echo "running $(utc)" > "$d/state"
        say "start $name"
        mkdir -p "$d/out"
        # -i: no idle sleep while a batch runs; the display may still sleep (that is what we want).
        ( cd "$d" && XOQ_NAME=$name XOQ_DIR=$d XOQ_OUT=$d/out \
            lockf -k "$LOCK" caffeinate -i /bin/bash "$d/run.sh" ) > "$d/run.log" 2>&1 < /dev/null
        rc=$?
        echo "done $rc $(utc)" > "$d/state"
        say "end $name rc=$rc"
    done
}

cmd=${1:-status}; shift || true
case $cmd in
    start)
        if p=$(running_pid); then echo "runner already running (pid $p)"; exit 0; fi
        rm -f "$XOQ/STOP"
        nohup /bin/bash "$0" _loop > /dev/null 2>&1 < /dev/null &
        sleep 1; echo "runner started (pid $(cat "$XOQ/runner.pid" 2>/dev/null))" ;;
    _loop) loop ;;
    add)
        [ $# -ge 2 ] || { echo "usage: mac_queue.sh add NAME FILE [SHA256]" >&2; exit 2; }
        name=$1; file=$2; want=${3:-}
        case $name in *[!A-Za-z0-9._-]*|'') echo "NAME: letters, digits, . _ - only" >&2; exit 2 ;; esac
        [ -f "$file" ] || { echo "no such file: $file" >&2; exit 2; }
        got=$(shasum -a 256 "$file" | awk '{print $1}')
        if [ -n "$want" ] && [ "$got" != "$want" ]; then echo "sha256 mismatch: $got != $want" >&2; exit 1; fi
        last=$(ls "$Q" 2>/dev/null | sort | tail -n 1 | cut -c1-3)
        n=$(printf '%03d' $(( 10#${last:-0} + 1 )))
        d=$Q/$n-$name
        mkdir -p "$d" && cp "$file" "$d/run.sh" && echo "$got" > "$d/run.sh.sha256" && echo queued > "$d/state"
        echo "queued $n-$name (sha256 $got)"
        running_pid > /dev/null || echo "note: the runner is not running; mac_queue.sh start" ;;
    cancel)
        d=$(ls -d "$Q"/*-"$1" 2>/dev/null | tail -n 1)
        [ -n "$d" ] && [ "$(head -n 1 "$d/state")" = queued ] || { echo "no queued batch named $1" >&2; exit 1; }
        echo cancelled > "$d/state"; echo "cancelled $(basename "$d")" ;;
    stop) touch "$XOQ/STOP"; echo "runner stops after the current batch" ;;
    lock) exec lockf -k "$LOCK" "$@" ;;
    status)
        if p=$(running_pid); then echo "runner: pid $p"; else echo "runner: not running"; fi
        echo "free: $(free_gb "$XOQ_DISK") GB on $XOQ_DISK, $(free_gb "$HOME") GB on $HOME"
        for d in $(ls -d "$Q"/*/ 2>/dev/null | sort); do d=${d%/}; printf '%-40s %s\n' "$(basename "$d")" "$(head -n 1 "$d/state")"; done ;;
    *) sed -n '2,33p' "$0"; exit 2 ;;
esac
