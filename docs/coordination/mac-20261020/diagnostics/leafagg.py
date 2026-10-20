import re,sys,collections
def load(path,root='XOffsetAdapter::mutate'):
    lines=open(path).read().split('\n')
    s=[i for i,l in enumerate(lines) if 'Call graph:' in l][0]; e=[i for i,l in enumerate(lines) if 'Sort by top of stack' in l][0]
    nodes=[]
    for l in lines[s+1:e]:
        m=re.match(r'^([\s\+\!\:\|]*)(\d+)\s+(.*)$',l)
        if m: nodes.append((len(m.group(1)),int(m.group(2)),re.sub(r'\s*\(in [^)]*\).*','',m.group(3)).replace('XOffsetDatastructure::','')))
    total=0; self_=collections.Counter()
    for i,(d,c,n) in enumerate(nodes):
        if root in n:
            total+=c
            j=i+1; sub=[]
            while j<len(nodes) and nodes[j][0]>d: sub.append(nodes[j]); j+=1
            allnodes=[(d,c,n)]+sub
            for k,(dk,ck,nk) in enumerate(allnodes):
                child=sum(c2 for (d2,c2,n2) in allnodes[k+1:k+1+10**9] if d2>dk and all(allnodes[t][0]>dk for t in range(k+1,allnodes.index((d2,c2,n2))+1)) and d2==min([x[0] for x in allnodes[k+1:] if x[0]>dk] or [0]))
            # simpler: self = c - sum(direct children)
            stack=[]
            for k,(dk,ck,nk) in enumerate(allnodes):
                kids=0; m=k+1; dd=None
                while m<len(allnodes) and allnodes[m][0]>dk:
                    if dd is None: dd=allnodes[m][0]
                    if allnodes[m][0]==dd: kids+=allnodes[m][1]
                    m+=1
                self_[nk[:110]]+=ck-kids
    return total,self_
for p in sys.argv[1:]:
    t,s=load(p); print('==',p.split('/')[-1],'mutate samples',t)
    for n,c in s.most_common(14): print('%5d %5.1f%%  %s'%(c,100*c/max(t,1),n))
