import re, sys, subprocess
# Compare reflection vs descriptor call graphs: every function body in the .s
# is keyed by its demangled name with the descriptor-specific spellings mapped
# onto the reflection ones; call targets are canonicalized the same way.
src = open(sys.argv[1]).read().splitlines()
bodies, cur = {}, None
for line in src:
    m = re.match(r'^([A-Za-z_.$][\w.$]*):', line)
    if m and not m.group(1).startswith('.L'):
        cur = m.group(1); bodies[cur] = []; continue
    if cur is None: continue
    if re.match(r'^\s*\.(cfi_endproc|size)', line) or line.startswith('.Lfunc_end'):
        cur = None; continue
    s = line.split('#')[0].rstrip()
    t = s.strip()
    if not t or t.startswith('.cfi') or t.startswith('.p2align') or re.match(r'^\.L\w+:$', t): continue
    if t.startswith('.'): continue
    bodies[cur].append(t)
names = list(bodies)
dem = subprocess.run(['c++filt'], input='\n'.join(names), capture_output=True, text=True).stdout.splitlines()
def canon(d):
    d = d.replace('desc_', 'refl_')
    d = d.replace('xo_desc::static_for', 'XOffsetDatastructure::detail::static_for')
    d = d.replace('probe::', 'XOffsetDatastructure::detail::')
    return d
cmap = {n: canon(x) for n, x in zip(names, dem)}
def norm(body):
    out = []
    for t in body:
        t = re.sub(r'\.L\w+', '.L', t)
        def sym(m):
            s = m.group(0)
            return '<' + cmap[s] + '>' if s in cmap else s
        t = re.sub(r'[A-Za-z_$][\w.$]*', sym, t)
        out.append(t)
    return out
refl = {cmap[n]: norm(b) for n, b in bodies.items() if not (n.startswith('desc_') or 'xo_desc' in cmap[n] or 'probe::' in subprocess.run(['c++filt', n], capture_output=True, text=True).stdout)}
desc = {cmap[n]: norm(b) for n, b in bodies.items() if n.startswith('desc_') or 'xo_desc' in subprocess.run(['c++filt', n], capture_output=True, text=True).stdout or 'probe::' in subprocess.run(['c++filt', n], capture_output=True, text=True).stdout}
ok = True; total = 0
for k in sorted(desc):
    same = refl.get(k) == desc[k]
    ok &= same; total += len(desc[k])
    short = k if len(k) < 90 else k[:87] + '...'
    print(f"{'IDENTICAL' if same else 'DIFFERENT'} {len(desc[k]):4d}  {short}")
print(f"{'ALL IDENTICAL' if ok else 'SOME DIFFER'}: {len(desc)} functions, {total} instructions")
sys.exit(0 if ok else 1)
