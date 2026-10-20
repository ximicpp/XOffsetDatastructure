"""Linear output accounting and exact-byte deduplication of verified fresh outputs.

Only the coordinator calls this, after the child's group has terminated. No native
result is trusted by this module: verified=True is permitted only after the normal
full semantic verifier succeeded. Failed/partial outputs are counted and preserved.
Queue files belong to a separately sealed supervisor and are excluded from this
inventory; metadata_reserve_bytes reserves their bounded growth and final metadata.
Unique inode bytes/allocated blocks are not RSS or total filesystem/VHD allocation.
"""
from pathlib import Path
import hashlib, math, os, shutil, stat

VERSION='sc-linear-output-ledger-v1'
SLOTS=('.stdout','.stderr','.receipt.json','.wire','.canonical','.queries','.native.json')
BODIES=('.wire','.canonical','.queries')
KEYS=('maximum_wire_bytes','maximum_primary_wire_bytes','maximum_logical_retained_bytes',
      'maximum_allocated_output_bytes','metadata_reserve_bytes','disk_margin_bytes')

class ResourceError(ValueError): pass
def need(c,m):
    if not c: raise ResourceError(m)
def positive(v): return type(v) is int and v>0
def state(p):
    s=p.lstat();need(stat.S_ISREG(s.st_mode),'nonregular output: '+str(p))
    return {'dev':s.st_dev,'ino':s.st_ino,'bytes':s.st_size,'mtime_ns':s.st_mtime_ns,
            'mode':stat.S_IMODE(s.st_mode),'blocks_bytes':s.st_blocks*512,'links':s.st_nlink}
def signature(s):return tuple(s[k] for k in ('dev','ino','bytes','mtime_ns','mode'))
def identity_from_fd(fd):
    s=os.fstat(fd);need(stat.S_ISREG(s.st_mode),'nonregular descriptor')
    return (s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns)
def remove_owned_link(path, expected):
    """Rollback only a link this transaction created, never a replaced path."""
    try:s=path.lstat()
    except FileNotFoundError:return
    need(stat.S_ISREG(s.st_mode) and (s.st_dev,s.st_ino)==expected,
         'transaction link identity changed: '+str(path))
    path.unlink()

class Ledger:
    def __init__(self,out,policy,backing_path,free_probe=None):
        self.out=Path(out).absolute()
        need(self.out.is_dir() and self.out.resolve()==self.out,'output root is not a real absolute directory')
        need(set(policy)==set(KEYS) and all(positive(policy[k]) for k in KEYS),'invalid resource policy')
        self.policy=dict(policy);self.backing=Path(backing_path)
        need(self.backing.is_absolute() and self.backing.is_dir(),'backing volume probe missing')
        self.free_probe=free_probe or (lambda p: shutil.disk_usage(p).free)
        self.files={};self.inodes={};self.logical=0;self.unique_bytes=0;self.allocated=0
        self.generated_wire=0;self.primary_wire=0;self.pending=None
        self.completed_ids=set();self.proofs=[];self.blobs={};self.free_min={}
        self.pool=self.out/'verified-body-pool'
        need(not self.pool.exists(),'body pool must be new')
        self.pool.mkdir()
        for p in self._inventory():self._track(p)
        self._check_caps()

    def _path(self,p):
        p=Path(p).absolute()
        need(p!=self.out and p.is_relative_to(self.out),'path outside owned output')
        need(p.resolve(strict=False)==p,'output path has symlink components')
        need(p.relative_to(self.out).parts[0]!='queue','supervisor queue is separately sealed')
        return p
    def _inventory(self):
        found=[]
        for base,dirs,names in os.walk(self.out,followlinks=False):
            base=Path(base)
            if base==self.out and 'queue' in dirs:
                need(not (base/'queue').is_symlink(),'supervisor queue directory symlink')
                dirs.remove('queue')
            for n in dirs:need(not (base/n).is_symlink(),'output directory symlink')
            for n in names:
                p=self._path(base/n);state(p);found.append(p)
        return found
    def _track(self,p):
        p=self._path(p);self._store(str(p.relative_to(self.out)),state(p))
    def _store(self,name,s):
        old=self.files.get(name)
        if old is not None:
            self.logical-=old['bytes'];key=(old['dev'],old['ino']);entry=self.inodes[key]
            entry['refs']-=1
            if not entry['refs']:
                self.unique_bytes-=entry['bytes'];self.allocated-=entry['blocks']
                del self.inodes[key]
        self.files[name]=s;self.logical+=s['bytes'];key=(s['dev'],s['ino'])
        if key in self.inodes:
            entry=self.inodes[key]
            need(entry['bytes']==s['bytes'] and entry['mtime_ns']==s['mtime_ns'],'linked content identity mismatch')
            entry['refs']+=1
            # Block reservations can settle asynchronously; keep the conservative
            # maximum while running, then census the actual value at reconciliation.
            if s['blocks_bytes']>entry['blocks']:
                self.allocated+=s['blocks_bytes']-entry['blocks'];entry['blocks']=s['blocks_bytes']
        else:
            self.inodes[key]={'refs':1,'bytes':s['bytes'],'mtime_ns':s['mtime_ns'],'blocks':s['blocks_bytes']}
            self.unique_bytes+=s['bytes'];self.allocated+=s['blocks_bytes']
    def _totals(self):
        return {'logical_retained_bytes':self.logical,'unique_inode_bytes':self.unique_bytes,
            'allocated_output_bytes':self.allocated,'members':len(self.files),'unique_inodes':len(self.inodes)}
    def _check_caps(self):
        t=self._totals();p=self.policy
        need(self.generated_wire<=p['maximum_wire_bytes'],'logical generated wire cap exceeded')
        need(self.primary_wire<=p['maximum_primary_wire_bytes'],'logical primary wire cap exceeded')
        need(t['logical_retained_bytes']+p['metadata_reserve_bytes']<=p['maximum_logical_retained_bytes'],
             'logical retained cap exceeded')
        need(t['allocated_output_bytes']+p['metadata_reserve_bytes']<=p['maximum_allocated_output_bytes'],
             'allocated output cap exceeded')
        return t
    def before_attempt(self,job,prefix,file_limit):
        need(self.pending is None,'previous attempt was not recorded')
        need(type(job) is dict and type(job.get('id')) is str and job['id'] and
             job.get('execution_kind') in ('primary','conditioning'),'invalid job domain')
        need(job['id'] not in self.completed_ids,'job already attempted')
        prefix=self._path(prefix)
        need(prefix.parent==self.out/'commands' and prefix.name==job['id'],'job prefix differs')
        need(positive(file_limit),'invalid child file guard')
        paths=[prefix.with_suffix(s) for s in SLOTS]
        need(all(not p.exists() and not p.is_symlink() for p in paths),'child output already exists')
        t=self._check_caps();p=self.policy
        need(self.generated_wire+file_limit<=p['maximum_wire_bytes'],'wire admission refused')
        if job['execution_kind']=='primary':
            need(self.primary_wire+file_limit<=p['maximum_primary_wire_bytes'],'primary wire admission refused')
        # Seven RLIMIT_FSIZE slots and at most three new pool directory entries.
        logical_peak=10*file_limit+p['metadata_reserve_bytes']
        physical_peak=7*(file_limit+4096)+p['metadata_reserve_bytes']
        need(t['logical_retained_bytes']+logical_peak<=p['maximum_logical_retained_bytes'],
             'logical retained admission refused')
        need(t['allocated_output_bytes']+physical_peak<=p['maximum_allocated_output_bytes'],
             'allocated output admission refused')
        for label,path in [('guest',self.out),('backing',self.backing)]:
            free=self.free_probe(path)
            need(type(free) is int and free>=0,'invalid free-space probe')
            self.free_min[label]=min(self.free_min.get(label,free),free)
            need(free>=physical_peak+p['disk_margin_bytes'],label+' free-space admission refused')
        self.pending=(dict(job),prefix,file_limit)
        return {'generated_wire_bytes':self.generated_wire,'primary_wire_bytes':self.primary_wire,**t}

    def _dedup(self,path):
        path=self._path(path);before=state(path)
        need(before['links']==1,'native output was not a fresh single-link file')
        need(before['mode']&0o222,'native output was unexpectedly read-only before verification')
        fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW)
        try:
            opened=identity_from_fd(fd)
            need(opened==tuple(before[k] for k in ('dev','ino','bytes','mtime_ns')),'native output changed before read')
            digest=hashlib.sha256()
            while True:
                chunk=os.read(fd,1024*1024)
                if not chunk:break
                digest.update(chunk)
            need(identity_from_fd(fd)==opened,'native output changed while hashing')
            key=(before['bytes'],digest.hexdigest());name=str(key[0])+'-'+key[1]
            anchor=self.pool/name;fresh=key not in self.blobs
            if fresh:
                need(not anchor.exists() and not anchor.is_symlink(),'unregistered pool member')
                need(signature(state(path))==signature(before),'native path changed before linking')
                linked=False
                try:
                    os.link(path,anchor,follow_symlinks=False);linked=True
                    other=os.open(anchor,os.O_RDONLY|os.O_NOFOLLOW)
                    try:need(identity_from_fd(fd)==identity_from_fd(other),'new anchor identity differs')
                    finally:os.close(other)
                    anchor.chmod(0o444)
                    self.blobs[key]=state(anchor)
                except BaseException:
                    if linked:
                        remove_owned_link(anchor,(before['dev'],before['ino']))
                        # chmod can have succeeded before a later I/O error. Keep
                        # all original bytes and account the actual native mode.
                        self._track(path)
                    raise
            else:
                want=self.blobs[key];now=state(anchor)
                need(signature(now)==signature(want) and not now['mode']&0o222,'pool anchor changed')
                other=os.open(anchor,os.O_RDONLY|os.O_NOFOLLOW)
                try:
                    need(identity_from_fd(other)==tuple(want[k] for k in ('dev','ino','bytes','mtime_ns')),
                         'anchor descriptor changed')
                    os.lseek(fd,0,os.SEEK_SET)
                    while True:
                        a=os.read(fd,1024*1024);b=os.read(other,1024*1024)
                        need(a==b,'same-hash pool content is not byte identical')
                        if not a:break
                    need(identity_from_fd(fd)==opened and identity_from_fd(other)==tuple(want[k] for k in
                         ('dev','ino','bytes','mtime_ns')),'body changed during exact comparison')
                finally:os.close(other)
                temporary=path.with_name(path.name+'.verified-link')
                need(not temporary.exists() and not temporary.is_symlink(),'temporary link already exists')
                linked=False
                try:
                    os.link(anchor,temporary,follow_symlinks=False);linked=True
                    need(signature(state(temporary))==signature(want),'temporary link differs')
                    need(signature(state(path))==signature(before),'native path changed before atomic replacement')
                    os.replace(temporary,path)
                finally:
                    if linked:remove_owned_link(temporary,(want['dev'],want['ino']))
            self._track(anchor);self._track(path)
            proof={'path':str(path.relative_to(self.out)),'original_native_file':before,
                'body_identity':{'bytes':key[0],'sha256':key[1]},'anchor':str(anchor.relative_to(self.out)),
                'first_verified_occurrence':fresh,'full_semantic_validation_preceded_pooling':True,
                'exact_byte_comparison_to_existing_anchor':not fresh,'bytes_preserved':True}
            self.proofs.append(proof)
        finally:os.close(fd)

    def record_outputs(self,job,prefix,verified=False):
        need(type(verified) is bool,'verified must be a strict bool')
        need(self.pending is not None,'no admitted attempt')
        expected,p,limit=self.pending
        need(job==expected and self._path(prefix)==p,'attempt identity differs')
        present=[];oversized=[]
        for suffix in SLOTS:
            path=p.with_suffix(suffix)
            if path.exists() or path.is_symlink():
                self._track(path);present.append(suffix)
                if state(path)['bytes']>limit:oversized.append(suffix)
        wire=p.with_suffix('.wire')
        if '.wire' in present:
            size=state(wire)['bytes'];self.generated_wire+=size
            if job['execution_kind']=='primary':self.primary_wire+=size
        self.completed_ids.add(job['id']);self.pending=None
        need(not oversized,'native slots exceeded file guard: '+repr(oversized))
        if verified:
            need(tuple(present)==SLOTS,'verified result lacks required slots')
            for suffix in BODIES:self._dedup(p.with_suffix(suffix))
        return self._check_caps()

    def snapshot(self):
        return {'version':VERSION,'recorded_attempts':len(self.completed_ids),
            'generated_wire_bytes':self.generated_wire,'generated_primary_wire_bytes':self.primary_wire,
            **self._totals(),'deduplicated_body_slots':len(self.proofs),'body_anchors':len(self.blobs),
            'minimum_observed_free_bytes':dict(self.free_min),'policy':dict(self.policy),
            'queue_and_final_metadata_covered_by_reserve':True,
            'unique_inode_and_allocated_blocks_are_not_total_filesystem_or_VHD_usage':True}
    def reconcile(self,allowed_extra=()):
        need(self.pending is None,'unrecorded active attempt')
        extras={str(self._path(self.out/p).relative_to(self.out)) for p in allowed_extra}
        actual={str(p.relative_to(self.out)):state(p) for p in self._inventory()}
        need(set(actual)==set(self.files)|extras,'unknown or missing retained output member')
        for name,old in self.files.items():
            need(signature(actual[name])==signature(old),'retained output changed: '+name)
        self.files={};self.inodes={};self.logical=0;self.unique_bytes=0;self.allocated=0
        for name,s in actual.items():self._store(name,s)
        self._check_caps()
        return {'status':'passed',**self.snapshot(),'body_proofs':list(self.proofs)}
