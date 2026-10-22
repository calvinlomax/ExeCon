#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
p=argparse.ArgumentParser()
p.add_argument('build',type=Path)
p.add_argument('--interp-only',action='store_true')
a=p.parse_args()
exe=(a.build/'execon').resolve()
root=Path(__file__).resolve().parents[2]
count=0
for interp in ([True] if a.interp_only else [True,False]):
    options=['--interp-only'] if interp else []
    for name in ['hello','console']:
        r=subprocess.run([exe,*options,'--stats',a.build/f'guests/{name}-x86_64.exe'],capture_output=True)
        assert r.returncode==0,(name,r.stderr)
        assert r.stdout==(root/f'tests/guests/{name}.expected').read_bytes(),(name,r.stdout)
        if interp: assert b'translated_blocks=0 ' in r.stderr
        else: assert b'translated_blocks=0 ' not in r.stderr
        count+=1
    payload=b'\x00\xff\xfeBinary\r\n\x1b[31mred\x1b[0m\n'
    r=subprocess.run([exe,*options,a.build/'guests/echo-x86_64.exe'],input=payload,capture_output=True)
    assert r.returncode==0 and r.stdout==payload,(r.returncode,r.stdout,r.stderr)
    count+=1
    with tempfile.TemporaryDirectory(prefix='execon-test-') as td:
        status=Path(td)/'status.json'
        r=subprocess.run([exe,*options,'--status-file',status,a.build/'guests/exit-x86_64.exe'],capture_output=True)
        assert r.returncode==42 and json.loads(status.read_text())=={'kind':'guest','code':42}
    count+=1
    r=subprocess.run([exe,*options,'--report-imports',a.build/'guests/missing-x86_64.exe'],capture_output=True)
    assert r.returncode==126 and b'MISSING USER32.dll!MessageBoxW' in r.stderr and b'return-address=0x' in r.stderr,r.stderr
    count+=1
    r=subprocess.run([exe,*options,'--on-missing=return-error',a.build/'guests/missing-x86_64.exe'],capture_output=True)
    assert r.returncode==0 and b'missing API USER32.dll!MessageBoxW' in r.stderr,r.stderr
    count+=1
r=subprocess.run([exe,'--inspect',a.build/'guests/hello-i686.exe'],capture_output=True)
assert r.returncode==0 and b'PE32 x86' in r.stdout
r=subprocess.run([exe,a.build/'guests/hello-i686.exe'],capture_output=True)
assert r.returncode==126 and b'PE32 execution is not implemented' in r.stderr
count+=2
print(f'PASS {count} PE/CLI integration checks; PE32 execution explicitly rejected')
