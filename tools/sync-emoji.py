#!/usr/bin/env python3
"""Copia a romfs/emoji los Twemoji que usan los fuentes (.cpp). Uso: sync-emoji.py <carpeta emoji-datasource-twitter/img/twitter/64> [extra.txt]"""
import sys, os, re, shutil, glob
src = sys.argv[1]; root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
dst = os.path.join(root, 'romfs', 'emoji'); os.makedirs(dst, exist_ok=True)
text = ''
for f in glob.glob(os.path.join(root, 'source', '**', '*.cpp'), recursive=True) + glob.glob(os.path.join(root, 'source', '**', '*.h'), recursive=True): text += open(f, encoding='utf-8').read()
for f in sys.argv[2:]: text += open(f, encoding='utf-8').read()
have = set(os.listdir(src)); miss = set(); n = 0
# secuencias: un emoji base + modificadores/ZWJ
pat = re.compile('[\U0001F000-\U0001FAFF←-⯿©®‼⁉™ℹ〰〽㊗㊙](?:️|‍[\U0001F000-\U0001FAFF←-⯿]️?|[\U0001F3FB-\U0001F3FF])*️?')
for m in set(pat.findall(text)):
    cps = [ord(c) for c in m]; zwj = 0x200d in cps
    base = '-'.join('%x' % c for c in cps if zwj or c != 0xfe0f)
    cand = [base, base + '-fe0f', '-'.join('%04x' % c for c in cps if zwj or c != 0xfe0f), '-'.join('%04x' % c for c in cps if zwj or c != 0xfe0f) + '-fe0f']
    hit = next((c for c in cand if c + '.png' in have), None)
    if not hit: miss.add(m); continue
    out = os.path.join(dst, base + '.png')
    if not os.path.exists(out): shutil.copy(os.path.join(src, hit + '.png'), out); n += 1
print('copiados', n, 'sin imagen:', ' '.join(sorted(miss)))
