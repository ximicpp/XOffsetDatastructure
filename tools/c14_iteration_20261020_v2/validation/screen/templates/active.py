def active(queue,kind,maximum):
    need(platform.system()=='Linux' and 'microsoft' in platform.release().lower(),'WSL2 only')
    queue=path(queue);q=C.read(queue/'request.json');actual=Path('/proc/self/cmdline').read_bytes().split(b'\0')[:-1]
    expected=['/usr/bin/python3','-B',str(HERE/'screen.py'),*sys.argv[1:]]
    need(q['command']==expected and actual==[s.encode() for s in expected],'exact actual direct worker command required')
    need(q['owner']=='W3' and q['kind']==kind and q['runner_sha256']==C.sha(RUNNER) and q['lock_dir']==str(LOCK) and q['out']==str(queue) and q['cwd']==str(ROOT)==str(Path.cwd()) and q['timeout_seconds']==maximum,'own original queue/cwd/cap required')
    with (LOCK/'queue.lock').open('r+') as stream:
        try:fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:pass
        else:fcntl.flock(stream,fcntl.LOCK_UN);raise C.ProtocolError('original global lease not held')
        stream.seek(0);record=C.parse(stream.read())
    need(record['status']=='running' and record['supervisor_pid']==os.getppid() and record['run_id']==q['run_id'] and record['owner']=='W3' and record['kind']==kind and record['out']==str(queue),'own active lease out/run/supervisor required')
    need((Path('/proc')/str(os.getppid())/'cmdline').read_bytes().split(b'\0')[:-1]==[b'/usr/bin/python3',str(RUNNER).encode(),b'_worker',str(queue).encode()],'actual original supervisor command required')
    return dict(request=q,request_identity=fid(queue/'request.json'),lease_record=record)
