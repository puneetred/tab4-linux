#!/usr/bin/env python3
# Resolve apk dependency closure for wanted packages against installed set,
# download the .apk files straight from Alpine CDN on the Mac, emit a tarball.
import subprocess, tarfile, io, os, sys, urllib.request, re

BASE = 'http://dl-cdn.alpinelinux.org/alpine/v3.20'
WORK = '/Users/puneetjain/Documents/tab4'
WANTED = sys.argv[1:] or ['xset', 'xinput', 'evtest', 'xdotool', 'xev']

installed = set(l.strip() for l in open(f'{WORK}/dist/installed.txt') if l.strip())

def fetch(url):
    return urllib.request.urlopen(url, timeout=60).read()

pkgs = {}   # name -> (repo, version, [deps])
provides = {}  # soname -> pkgname
for repo in ['main', 'community']:
    raw = fetch(f'{BASE}/{repo}/armhf/APKINDEX.tar.gz')
    with tarfile.open(fileobj=io.BytesIO(raw), mode='r:gz') as tf:
        idx = tf.extractfile('APKINDEX').read().decode()
    for stanza in idx.split('\n\n'):
        name = ver = None
        deps, prov = [], []
        for line in stanza.split('\n'):
            if line.startswith('P:'): name = line[2:]
            elif line.startswith('V:'): ver = line[2:]
            elif line.startswith('D:'): deps = line[2:].split()
            elif line.startswith('p:'): prov = line[2:].split()
        if name:
            pkgs[name] = (repo, ver, deps)
            for p in prov:
                provides[re.split(r'[=<>~]', p)[0]] = name

def resolve(dep):
    d = re.split(r'[=<>~]', dep)[0]
    if d.startswith('so:'):
        return provides.get(d[3:])
    return d if d in pkgs else None

need = {}
queue = list(WANTED)
while queue:
    n = queue.pop()
    if n in installed or n in need or n not in pkgs:
        continue
    repo, ver, deps = pkgs[n]
    need[n] = (repo, ver)
    for d in deps:
        r = resolve(d)
        if r and r not in installed and r not in need:
            queue.append(r)

os.makedirs('/tmp/t4apks', exist_ok=True)
files = []
for n, (repo, ver) in sorted(need.items()):
    url = f'{BASE}/{repo}/armhf/{n}-{ver}.apk'
    dst = f'/tmp/t4apks/{n}-{ver}.apk'
    if not os.path.exists(dst):
        urllib.request.urlretrieve(url, dst)
    files.append(dst)
    print('got', n, ver)

with tarfile.open('/tmp/t4apks.tar', 'w') as tf:
    for f in files:
        tf.add(f, arcname=os.path.basename(f))
print('TAR-READY', len(files), 'packages', os.path.getsize('/tmp/t4apks.tar'), 'bytes')
