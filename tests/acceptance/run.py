#!/usr/bin/env python3
"""Real acceptance gate; unsupported targets FAIL instead of being counted as passes."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import tempfile
import threading
import zipfile

ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--build',type=Path,default=ROOT/'build')
p.add_argument('--diagnose-only',action='store_true')
a=p.parse_args()
exe=(a.build/'execon').resolve()
reports=ROOT/'docs/acceptance'
reports.mkdir(exist_ok=True)
failed=[]

def execute(target,options,arguments):
    try:
        return subprocess.run([str(exe),*options,str(ROOT/'third_party_bin'/target),*arguments],
                              stdin=subprocess.DEVNULL,capture_output=True,timeout=15)
    except subprocess.TimeoutExpired as error:
        return subprocess.CompletedProcess([],124,error.stdout or b'',(error.stderr or b'')+b'\nAcceptance timeout\n')

for target in ['7za.exe','7z.exe','curl.exe']:
    if not (ROOT/'third_party_bin'/target).exists():
        raise SystemExit('Run scripts/fetch-test-binaries.sh first')
    report=execute(target,['--inspect','--report-imports'],[])
    (reports/(target+'.imports.txt')).write_bytes(report.stderr)
    if report.returncode: raise SystemExit(f'Inspection failed for {target}: {report.stderr.decode(errors="replace")}')
    if a.diagnose_only:
        for mode in ['interp','jit']:
            options=['--stats']+(['--interp-only'] if mode=='interp' else [])
            probe=execute(target,options,['--help'])
            (reports/f'{target}.{mode}.txt').write_bytes(f'exit={probe.returncode}\n'.encode()+probe.stdout+probe.stderr)
            print(f'{target} {mode}: exit {probe.returncode}; see docs/acceptance/')
        continue
    for mode in ['interp','jit']:
        options=['--stats']+(['--interp-only'] if mode=='interp' else [])
        if target.startswith('7z'):
            with tempfile.TemporaryDirectory(prefix='execon-accept-') as temp:
                work=Path(temp);payload=b'ExeCon archive fixture\x00\xff\n'*128
                (work/'fixture.bin').write_bytes(payload)
                with zipfile.ZipFile(work/'reference.zip','w') as reference: reference.writestr('fixture.bin',payload)
                guest=lambda name:'Z:'+str(work/name).replace('/','\\')
                commands=[['l',guest('reference.zip')],['a','-t7z',guest('created.7z'),guest('fixture.bin')],
                          ['x','-y','-o'+guest('extracted'),guest('created.7z')]]
                for command in commands:
                    result=execute(target,options,command)
                    (reports/f'{target}.{mode}.txt').write_bytes(f'exit={result.returncode}\n'.encode()+result.stdout+result.stderr)
                    if result.returncode:
                        failed.append(f'{target}/{mode}/{command[0]}: exit {result.returncode}');break
                else:
                    actual=(work/'extracted/fixture.bin').read_bytes()
                    assert hashlib.sha256(actual).digest()==hashlib.sha256(payload).digest()
                    assert (work/'created.7z').stat().st_size>0
                    print(f'PASS {target} {mode}: list/create/extract and SHA-256')
        else:
            payload=b'ExeCon HTTP acceptance\n'
            class Handler(BaseHTTPRequestHandler):
                def do_GET(self):
                    self.send_response(200);self.send_header('Content-Length',str(len(payload)));self.end_headers();self.wfile.write(payload)
                def log_message(self,*args): pass
            server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
            worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
            try:
                result=execute(target,options,['--noproxy','*','--silent','--show-error',f'http://127.0.0.1:{server.server_port}/fixture'])
                (reports/f'{target}.{mode}.txt').write_bytes(f'exit={result.returncode}\n'.encode()+result.stdout+result.stderr)
                if result.returncode or result.stdout!=payload: failed.append(f'{target}/{mode}/HTTP: exit {result.returncode}')
                else: print(f'PASS {target} {mode}: localhost HTTP')
            finally: server.shutdown();server.server_close();worker.join()
if a.diagnose_only:
    print('Diagnostic reports generated; this is not an acceptance pass.')
elif failed:
    print('\n'.join('FAIL '+message for message in failed))
    raise SystemExit(1)
