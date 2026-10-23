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
for arch in ['x86_64','i686']:
    for interp in ([True] if a.interp_only else [True,False]):
        options=['--interp-only'] if interp else []
        for name in ['hello','console','startup','crt_subset']:
            r=subprocess.run([exe,*options,'--stats',a.build/f'guests/{name}-{arch}.exe'],capture_output=True)
            assert r.returncode==0,(name,r.stderr)
            assert r.stdout==(root/f'tests/guests/{name}.expected').read_bytes(),(name,r.stdout)
            if interp or arch=='i686': assert b'translated_blocks=0 ' in r.stderr
            else: assert b'translated_blocks=0 ' not in r.stderr
            count+=1
        payload=b'\x00\xff\xfeBinary\r\n\x1b[31mred\x1b[0m\n'
        r=subprocess.run([exe,*options,a.build/f'guests/echo-{arch}.exe'],input=payload,capture_output=True)
        assert r.returncode==0 and r.stdout==payload,(r.returncode,r.stdout,r.stderr)
        count+=1
        with tempfile.TemporaryDirectory(prefix='execon-test-') as td:
            status=Path(td)/'status.json'
            r=subprocess.run([exe,*options,'--status-file',status,a.build/f'guests/exit-{arch}.exe'],capture_output=True)
            assert r.returncode==42 and json.loads(status.read_text())=={'kind':'guest','code':42}
        count+=1
        r=subprocess.run([exe,*options,'--report-imports',a.build/f'guests/missing-{arch}.exe'],capture_output=True)
        assert r.returncode==126 and b'MISSING USER32.dll!MessageBoxW' in r.stderr and b'return-address=0x' in r.stderr,r.stderr
        count+=1
        r=subprocess.run([exe,*options,'--on-missing=return-error',a.build/f'guests/missing-{arch}.exe'],capture_output=True)
        assert r.returncode==0 and b'missing API USER32.dll!MessageBoxW' in r.stderr,r.stderr
        count+=1
        with tempfile.TemporaryDirectory(prefix='execon-files-') as td:
            r=subprocess.run([exe,*options,'--drive',f'T={td}',a.build/f'guests/files_heap-{arch}.exe'],capture_output=True)
            assert r.returncode==0 and r.stdout==b'ExeCon\n',(r.returncode,r.stderr)
            assert (Path(td)/'RoundTrip.bin').read_bytes()==b'ExeCon\n'
        r=subprocess.run([exe,*options,'--env','EXECON_TEST=from environment',a.build/f'guests/environment-{arch}.exe'],capture_output=True)
        assert r.returncode==0 and r.stdout==b'from environment',(r.returncode,r.stderr)
        count+=2
        for name, code in [('fault',0xc0000005),('illegal',0xc000001d)]:
            with tempfile.TemporaryDirectory(prefix='execon-fault-') as td:
                status=Path(td)/'status.json'
                r=subprocess.run([exe,*options,'--status-file',status,a.build/f'guests/{name}-{arch}.exe'],capture_output=True)
                assert r.returncode==127 and json.loads(status.read_text())=={'kind':'guest-fault','code':code},(r.returncode,r.stderr)
                assert b'guest PC 0x' in r.stderr,r.stderr
                if name=='fault': assert b'12345000' in r.stderr,r.stderr
            count+=1
r=subprocess.run([exe,'--inspect',a.build/'guests/hello-i686.exe'],capture_output=True)
assert r.returncode==0 and b'PE32 x86' in r.stdout
r=subprocess.run([exe,a.build/'guests/hello-i686.exe'],capture_output=True)
assert r.returncode==0 and r.stdout==b'Hello from ExeCon!\n'
count+=2
modes='interpreter' if a.interp_only else 'native/interpreter'
print(f'PASS {count} PE/CLI integration checks; PE32 stdcall and PE32+ {modes} execution verified')
