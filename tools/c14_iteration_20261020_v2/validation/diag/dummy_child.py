"""Tiny Python-only event producers. Never import or launch project binaries."""
import json
import os
from pathlib import Path
import signal
import sys
import time

case=sys.argv[1]
fd=int(os.environ['XOFFSET_DIAG_EVENT_FD'])
seq=0
def emit(label,payload=()):
    global seq
    raw=('XD1 '+str(seq)+' '+str(time.monotonic_ns())+' '+label+''.join(' '+str(v) for v in payload)+'\n').encode()
    seq+=1
    try:return os.write(fd,raw)
    except (BlockingIOError,InterruptedError):return 0

if case=='normal':
    labels=json.loads((Path(__file__).parent/'INTERFACE.json').read_bytes())['labels']
    for label in labels:emit(label,(1,2,3,4) if label.startswith('emit.pod') else ())
elif case=='no_event_hang':
    while True:time.sleep(1)
elif case=='event_hang':
    emit('main.begin');emit('t0');emit('t1');emit('t2')
    while True:time.sleep(1)
elif case=='partial_hang':
    emit('main.begin');os.write(fd,('XD1 1 '+str(time.monotonic_ns())+' input.read.').encode())
    while True:time.sleep(1)
elif case=='truncated':
    emit('main.begin');os.write(fd,b'XD1 1')
elif case in ('epipe','close_after_first'):
    if case=='close_after_first':emit('main.begin')
    until=time.monotonic()+.5
    while not (Path(os.environ['XOFFSET_DUMMY_CONTROL'])/'reader-closed.json').exists() and time.monotonic()<until:time.sleep(.001)
    try:emit('main.end')
    except BrokenPipeError:os.write(1,b'EPIPE-observed\n')
elif case=='nonzero':
    emit('main.begin');sys.exit(7)
elif case=='signal':
    emit('main.begin');os.kill(os.getpid(),signal.SIGTERM)
elif case=='large_logs':
    emit('main.begin')
    block=b'L'*65536
    for _ in range(3):os.write(1,block);os.write(2,block)
    emit('main.end')
elif case=='log_limit':
    emit('main.begin')
    for _ in range(64):os.write(1,b'L'*65536)
elif case=='sequence_gap':
    emit('main.begin');seq=2;emit('main.end')
elif case=='event65':
    for _ in range(65):emit('main.begin')
elif case=='descendant_fd':
    emit('main.begin')
    if os.fork()==0:
        while True:time.sleep(1)
    sys.exit(0)
elif case=='wakeups':
    emit('main.begin')
    for _ in range(50):time.sleep(.005);os.write(1,b'.')
    emit('main.end')
else:raise RuntimeError('unknown fixed dummy case')
