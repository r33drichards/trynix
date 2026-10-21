#!/usr/bin/env python3
"""Read-only closure size gate. Never downloads or executes NAR/package contents.

Narinfo size metadata is a planning hint, not trusted execution authorization.
Any real loader must independently verify signatures, NAR hashes, and byte limits.
"""
import argparse
import concurrent.futures
import json
import re
import urllib.request

CACHE = 'https://cache.nixos.org'
ROOTS = {
    'modern': '6l9kbzxwq9ij2lcmwl2qb1jdpqisjr8l',  # rustc wrapper 1.99.0
    'historical': 'ja2nmxmwav162rfpa8yn7qh27kzfflk3',  # rustc 1.30.1
}

def fetch_info(digest):
    if not re.fullmatch('[0-9a-z]{32}', digest):
        raise ValueError('Invalid Nix digest')
    with urllib.request.urlopen(f'{CACHE}/{digest}.narinfo', timeout=30) as r:
        raw = r.read(65537)
    if len(raw) > 65536:
        raise ValueError('Oversized narinfo')
    fields = dict(line.split(': ', 1) for line in raw.decode().splitlines() if ': ' in line)
    return dict(path=fields['StorePath'], compressed=int(fields['FileSize']),
                unpacked=int(fields['NarSize']),
                references=[x[:32] for x in fields.get('References', '').split()])

def inventory(root, fetch=fetch_info):
    seen, pending = {}, {root}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        while pending:
            batch = sorted(pending)
            pending = set()
            for digest, info in zip(batch, pool.map(fetch, batch)):
                if info['compressed'] < 0 or info['unpacked'] < 0:
                    raise ValueError('Invalid size')
                seen[digest] = info
            for info in seen.values():
                pending.update(d for d in info['references'] if d not in seen)
            if len(seen) + len(pending) > 128:
                raise ValueError('Closure exceeds 128-path bound')
    return list(seen.values())

def summarize(infos, max_download=256*1024**2, max_unpacked=768*1024**2):
    compressed = sum(x['compressed'] for x in infos)
    unpacked = sum(x['unpacked'] for x in infos)
    return dict(paths=len(infos), compressedBytes=compressed, unpackedBytes=unpacked,
                maxDownloadBytes=max_download, maxUnpackedBytes=max_unpacked,
                withinBudget=compressed <= max_download and unpacked <= max_unpacked,
                largest=sorted(infos, key=lambda x:x['unpacked'], reverse=True)[:3],
                note='Inventory only. Does not prove RAM sufficiency, signatures, or execution compatibility.')

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('toolchain', choices=ROOTS)
    args=parser.parse_args()
    result=summarize(inventory(ROOTS[args.toolchain]))
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result['withinBudget'] else 2)
