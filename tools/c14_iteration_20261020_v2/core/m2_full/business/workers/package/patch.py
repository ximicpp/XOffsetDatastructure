"""Exact packaging-only tuple/list upgrade; no file mutation or import effects."""
import ast
import hashlib

OLD_HEADERS=('xoffsetdatastructure.hpp','xoffset_verified_paths.hpp',
 'xoffset_validation_scratch.hpp','validation_scratch.hpp',
 'xoffset_snapshot_index.hpp','xoffset_snapshot_index_budget.hpp')
NEW_HEADERS=('xoffset_core_lifetime.hpp','receive_raw_fwd.hpp','receive_raw.hpp',
 'receive_future.hpp','xoffset_verified_receive.hpp')
BASE={
 'tools/validate_vm_package.py':'5a55df123093f1b6ddb28fc49d969dfe921f421e21630797ec8865bdb9753b8a',
 'tools/prepare_alpha_release.py':'475ae349bcdfadf937af56d33ca48e0d456682ce0f024c6c4e296750a66728fd',
 'cmake/XOffsetFourPathHeaders.cmake':'3ed8475dc1f0dbfa33a3c66f4bfefb5ca3dd4721953c941826059f42980a5e6c'}

def headers(values):
    values=tuple(values)
    if (len(values)!=11 or len(set(values))!=11 or
        set(values)!=set(OLD_HEADERS+NEW_HEADERS)):
        raise ValueError('exact eleven final own-header identities required')
    return values

def tuple_assignment(text,values):
    tree=ast.parse(text)
    nodes=[n for n in tree.body if isinstance(n,ast.Assign) and
           any(isinstance(t,ast.Name) and t.id=='FULL_OWN_HEADERS' for t in n.targets)]
    if len(nodes)!=1 or ast.literal_eval(nodes[0].value)!=OLD_HEADERS:
        raise ValueError('original exact FULL_OWN_HEADERS literal differs')
    lines=text.splitlines(keepends=True);node=nodes[0]
    if node.col_offset!=0 or node.lineno!=node.end_lineno:
        raise ValueError('unexpected header tuple source shape')
    lines[node.lineno-1]='FULL_OWN_HEADERS = '+repr(values)+'\n'
    return ''.join(lines)

def apply_files(files,own_headers):
    values=headers(own_headers)
    if set(files)!=set(BASE):raise ValueError('exact three packaging inputs required')
    for name,text in files.items():
        generated='set(XOFFSET_FULL_OWN_HEADERS\n'+''.join('    '+n+'\n' for n in values)+')\n'
        if (type(text) is not str or
            (hashlib.sha256(text.encode()).hexdigest()!=BASE[name] and
             not (name.endswith('.cmake') and text==generated))):
            raise ValueError('original complete packaging file differs: '+name)
    out={name:tuple_assignment(files[name],values) for name in BASE if name.endswith('.py')}
    out['cmake/XOffsetFourPathHeaders.cmake']=generated
    return out
