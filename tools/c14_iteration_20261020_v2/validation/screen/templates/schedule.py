def schedule():
    """Independent specified SHA stream; never a sample/native retry."""
    seed=DESIGN['randomization']['seed_utf8'];orders=['AB']*36+['BA']*36;counter=0
    for i in range(71,0,-1):
        m=i+1;limit=(2**256//m)*m
        while True:
            x=int.from_bytes(hashlib.sha256(seed.encode()+b'\0'+counter.to_bytes(8,'big')).digest(),'big');counter+=1
            if x<limit:break
        j=x%m;orders[i],orders[j]=orders[j],orders[i]
    pairs=[];children=[];jobs=[]
    for i,order in enumerate(orders):
        pairs.append(dict(pair_index=i,pair_id=f'p{i:03d}',order=order,child_ordinals=[2*i,2*i+1]))
        for j,symbol in enumerate(order):
            role='control' if symbol=='A' else 'candidate'
            children.append(dict(ordinal=2*i+j,pair_index=i,pair_id=f'p{i:03d}',within_pair=j,symbol=symbol,role=role))
            jobs.append(dict(id=f'p{i:03d}-{j+1}-{role}',pair=i,order=order,role=role,ordinal=2*i+j,symbol=symbol))
    raw=dict(pairs=pairs,children=children)
    canonical=__import__('json').dumps(raw,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()
    need(counter==DESIGN['randomization']['hash_draws'] and raw['pairs']==DESIGN['randomization']['pairs'] and raw['children']==DESIGN['randomization']['children'],'independent schedule reconstruction differs')
    need(hashlib.sha256(canonical).hexdigest()==DESIGN['randomization']['schedule_sha256'] and orders.count('AB')==orders.count('BA')==36,'balanced randomized schedule digest differs')
    return jobs
