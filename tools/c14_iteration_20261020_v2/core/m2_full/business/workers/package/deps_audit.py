"""Read-only Ninja dependency parsing; no compiler, subprocess or project import."""
from pathlib import Path
import argparse,hashlib,json,re

OPTIONAL_DEFAULT_HEADERS=('validation_scratch.hpp','xoffset_validation_scratch.hpp')
HEADER=re.compile(r'^(.+): #deps ([0-9]+), deps mtime [0-9]+ \((VALID|STALE)\)$')

def need(ok,message):
    if not ok:raise ValueError(message)

def sections(text):
    result={};current=None;expected=None
    def close():
        if current is not None:
            need(len(result[current])==expected,'Ninja dependency count differs: '+current)
            # Ninja's declared count describes raw entries, not distinct paths.
            # A repeated header remains a real entry; membership is set-based
            # only when the caller checks required includes.
    for line in text.splitlines():
        if not line.strip():continue
        match=HEADER.fullmatch(line)
        if match:
            close();current,count,state=match.groups();expected=int(count)
            need(state=='VALID' and current not in result,'stale/duplicate Ninja object: '+current)
            result[current]=[]
        else:
            need(current is not None and line[:1].isspace(),'unrecognized Ninja dependency line')
            name=line.strip();need(name.startswith('/'),'nonabsolute actual dependency')
            result[current].append(name)
    close();need(bool(result),'empty Ninja dependency inventory')
    return result

def audit_default(text,source,own,forbidden=()):
    own=tuple(own);need(len(own)==len(set(own))==11,'exact eleven own headers')
    need(set(OPTIONAL_DEFAULT_HEADERS).issubset(own),'optional default headers absent from declared eleven')
    objects=sections(text);lines={p for values in objects.values() for p in values}
    required=tuple(n for n in own if n not in OPTIONAL_DEFAULT_HEADERS)
    need(len(required)==9,'default full required header count')
    for name in required:need(str(Path(source)/name) in lines,'missing default full actual header '+name)
    for path in forbidden:need(str(path) not in text,'original source/prefix used: '+str(path))
    return {'required_default_headers':list(required),'required_default_count':9,
      'declared_own_headers':list(own),'observed_own_headers':[n for n in own if str(Path(source)/n) in lines],
      'not_observed_own_headers':[n for n in own if str(Path(source)/n) not in lines],
      'object_count':len(objects),'scope':'default full actual includes; separate first-include objects prove all eleven'}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--deps',type=Path,required=True)
    ap.add_argument('--source',type=Path,required=True);ap.add_argument('--own-headers-json',type=Path,required=True)
    ap.add_argument('--forbidden',action='append',default=[]);ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args();data=args.deps.read_bytes();record=json.loads(args.own_headers_json.read_text())
    own=record['own_headers'] if isinstance(record,dict) else record
    result=audit_default(data.decode(),args.source,own,args.forbidden)
    result.update(schema_version=1,status='passed',kind='archive-default-full-dependency-parser-check',
      deps={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()},
      native_executions=0,compiler_executions=0,source=str(args.source))
    with args.out.open('x') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(args.out,flush=True)

if __name__=='__main__':main()
