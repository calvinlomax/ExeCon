#!/usr/bin/env python3
"""Unmet broad-runtime gate: ordinary CRT startup, exceptions, threads and floating point."""
from pathlib import Path
import subprocess
import sys
import tempfile
root=Path(__file__).resolve().parents[2]
build=Path(sys.argv[1] if len(sys.argv)>1 else 'build').resolve()
failed=0
for arch in ['x86_64','i686']:
    for expected in sorted((root/'tests/guests/runtime').glob('*.expected')):
        for mode in ['interp','jit'] if arch=='x86_64' else ['interp']:
            program=build/f'runtime-guests/{expected.stem}-{arch}.exe'
            if not program.exists():
                failed+=1;print(f'BUILD BLOCKED {arch}/{mode}/{expected.stem}: see runtime-guests/*.build-log');continue
            with tempfile.TemporaryDirectory(prefix='execon-runtime-') as td:
                (Path(td)/'fixture.txt').write_text('directory fixture\n')
                command=[str(build/'execon'),'--prefix',td,'--drive',f'T={td}','--env','EXECON_TEST=value']
                if mode=='interp':command+=['--interp-only']
                command += [str(program)] + (['sample'] if expected.stem=='argv_env' else [])
                try:result=subprocess.run(command,capture_output=True,timeout=15)
                except subprocess.TimeoutExpired:failed+=1;print(f'FAIL {arch}/{mode}/{expected.stem}: timeout');continue
                if result.returncode or result.stdout!=expected.read_bytes():
                    failed+=1;print(f'FAIL {arch}/{mode}/{expected.stem}: exit {result.returncode}: {result.stderr.decode(errors="replace").strip()[:180]}')
                else:print(f'PASS {arch}/{mode}/{expected.stem}')
print(f'{failed} broad-runtime checks failed; these are mandatory future compatibility gates.')
raise SystemExit(1 if failed else 0)
