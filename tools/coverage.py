# Coverage of the reconstruction: Ghidra function list (decomp-pop2img5) vs addresses cited in source/. usage: coverage.py [SEGMENT]
import re,glob,collections,sys,os
WS=os.environ.get('POP2_WORKSPACE',os.path.expanduser('~/pop2dec'))   # the analysis workspace (README)
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
fn=[]
for l in open(WS+'/decomp-pop2img5/functions.txt'):
    m=re.match(r'([0-9a-fA-F]{4}):([0-9a-fA-F]{4}) (\S+) size=(\d+)',l)
    if m: s,o=int(m.group(1),16),int(m.group(2),16); fn.append((s*16+o,int(m.group(4)),m.group(1).upper(),m.group(3))); continue
    m=re.match(r'(OVL\d\d_[0-9A-F]{4})::([0-9a-f]{6}) (\S+) size=(\d+)',l)
    if m: fn.append((int(m.group(2),16),int(m.group(4)),m.group(1),m.group(3)))
fn.sort()
cited=set()
for f in glob.glob(ROOT+'/source/*.c')+glob.glob(ROOT+'/source/*.h'):
    t=open(f).read()
    for m in re.finditer(r'\b([0-9A-Fa-f]{4}):([0-9A-Fa-f]{4})\b',t): cited.add(int(m.group(1),16)*16+int(m.group(2),16))
    for m in re.finditer(r'\b(0[23][0-9A-Fa-f]{4})\b',t): cited.add(int(m.group(1),16))
    for m in re.finditer(r'ovl_([0-9a-f]{5})\b',t): cited.add(int(m.group(1),16))
    for m in re.finditer(r'_([0-9a-f]{4})_([0-9a-f]{3,4})\b',t): cited.add(int(m.group(1),16)*16+int(m.group(2),16))
cov=collections.defaultdict(lambda:[0,0,0,0])
import bisect
hit=set()
for i,(a0,sz,seg,name) in enumerate(fn):
    for a in cited:
        if a0<=a<a0+max(sz,1): hit.add(i); break
for i,(a,sz,seg,name) in enumerate(fn):
    c=cov[seg]; c[0]+=1; c[1]+=sz
    if i in hit: c[2]+=1; c[3]+=sz
tot=[0,0,0,0]
for seg in sorted(cov):
    c=cov[seg]; tot=[x+y for x,y in zip(tot,c)]
    print('%s funcs %4d/%4d bytes %6d/%6d (%3d%%)'%(seg,c[2],c[0],c[3],c[1],100*c[3]//max(c[1],1)))
print('total funcs %d/%d bytes %d/%d (%d%%)'%(tot[2],tot[0],tot[3],tot[1],100*tot[3]//tot[1]))
if len(sys.argv)>1:
    for i,(a,sz,seg,name) in enumerate(fn):
        if seg==sys.argv[1] and i not in hit: print('  missing', '%s:%04X'%(seg,a-int(seg,16)*16), sz, name)
