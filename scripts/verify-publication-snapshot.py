#!/usr/bin/env python3
"""Verify the published Linux source tree against the K3 source snapshot."""
import hashlib,json,pathlib,subprocess,sys,os
root=pathlib.Path(__file__).resolve().parents[1]
manifest=json.loads((root/'PUBLICATION_SNAPSHOT.json').read_text())
skip=set(manifest['digest_exclusions'])
raw=subprocess.check_output(['git','-C',str(root),'ls-tree','-rz','--full-tree','HEAD'])
paths=[]
for record in raw.split(b'\0'):
    if not record:continue
    header,path=record.split(b'\t',1)
    if header.split()[1]!=b'blob':raise SystemExit('Unexpected non-file Git entry')
    name=path.decode('utf-8')
    if name not in skip:paths.append((name,header.split()[0].decode()))
paths.sort()
regular=[name for name,mode in paths if mode!='120000']
objects=subprocess.check_output(['git','-C',str(root),'hash-object','--stdin-paths'],
    input=('\n'.join(regular)+'\n').encode(),stderr=subprocess.PIPE).decode().splitlines()
if len(objects)!=len(regular):raise SystemExit('Cannot hash working-tree files')
object_map=dict(zip(regular,objects))
h=hashlib.sha256()
for name,mode in paths:
    p=root/name
    if mode=='120000':
        if not p.is_symlink():raise SystemExit('Symlink not preserved: '+name+'; use a Linux clone')
        data=os.readlink(p).encode()
        oid=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    else:
        oid=object_map[name]
        if mode=='100755' and not os.access(p,os.X_OK):raise SystemExit('Executable permission missing: '+name)
    h.update(name.encode()+b'\0'+oid.encode()+b'\n')
if len(paths)!=manifest['verified_files'] or h.hexdigest()!=manifest['content_sha256']:
    raise SystemExit('Source differs from the K3 publication snapshot')
print('PASS:',len(paths),'source files match the published K3 baseline plus documented updates after Git text normalization; symlinks and executable permissions checked')
