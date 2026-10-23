#!/usr/bin/env python3
"""Developer-only downloads. SHA-256 is checked BEFORE any executable is extracted/run."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import tarfile
import urllib.request
import zipfile

ROOT=Path(__file__).resolve().parents[1]

def verified(path, expected):
    with path.open('rb') as file:
        actual=hashlib.file_digest(file,'sha256').hexdigest()
    if actual!=expected:
        raise ValueError(f'SHA-256 mismatch for {path.name}: expected {expected}, got {actual}')

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--offline',action='store_true')
    options=parser.parse_args()
    manifest=json.loads((ROOT/'tests/acceptance/binaries.json').read_text())
    destination=ROOT/'third_party_bin'
    downloads=destination/'downloads'
    downloads.mkdir(parents=True,exist_ok=True)
    for spec in manifest.values():
        archive=downloads/spec['file']
        if not archive.exists():
            if options.offline: raise SystemExit(f'Missing cached download: {archive}')
            temporary=archive.with_suffix(archive.suffix+'.partial')
            try:
                with urllib.request.urlopen(spec['url'],timeout=60) as response, temporary.open('wb') as output:
                    total=0
                    while chunk:=response.read(1024*1024):
                        total+=len(chunk)
                        if total>64*1024*1024: raise ValueError('acceptance archive exceeds 64 MiB limit')
                        output.write(chunk)
                verified(temporary,spec['sha256'])
                temporary.replace(archive)
            finally:
                temporary.unlink(missing_ok=True)
        verified(archive,spec['sha256'])
        print(f'Verified {spec["file"]}')
    if platform.system()!='Darwin' or platform.machine()!='arm64':
        raise SystemExit('Extraction helper currently requires Darwin arm64; archives are verified and cached.')
    extractor=destination/'dev-tools/7zz'
    extractor.parent.mkdir(exist_ok=True)
    with tarfile.open(downloads/manifest['7zip-mac-dev-extractor']['file']) as archive:
        member=next(m for m in archive.getmembers() if m.name=='7zz')
        if not member.isfile() or member.size>32*1024*1024: raise ValueError('invalid dev extractor member')
        extractor.write_bytes(archive.extractfile(member).read())
    extractor.chmod(0o700)
    for target, spec, member in [('7za.exe','7zip-extra','x64/7za.exe'),
                                 ('7z.exe','7zip-installer','7z.exe'),
                                 ('7z.dll','7zip-installer','7z.dll')]:
        with (destination/target).open('wb') as output:
            subprocess.run([str(extractor),'e','-so',str(downloads/manifest[spec]['file']),member],stdout=output,check=True)
        if not (destination/target).stat().st_size: raise ValueError(f'Archive member absent: {member}')
    spec=manifest['curl']
    with zipfile.ZipFile(downloads/spec['file']) as archive:
        (destination/'curl.exe').write_bytes(archive.read(spec['member']))
    print('Developer-only binaries stored in ignored third_party_bin/. Never bundled with ExeCon.')
    print('7-Zip: GNU LGPL with unRAR restrictions and BSD portions; https://www.7-zip.org/license.txt')
    print('curl: curl license and bundled-library notices; https://curl.se/docs/copyright.html')
    print('Official source archives retain their license files. No Microsoft binaries are fetched.')

if __name__=='__main__': main()
