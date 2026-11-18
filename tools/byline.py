import subprocess, collections, re, sys
rows=[l.split() for l in open(sys.argv[1])]
cyc={r[0]:int(r[1]) for r in rows}
T=sum(cyc.values())
addrs=list(cyc)
out=subprocess.run(['llvm-addr2line','-e','steelharbor.elf','-f']+['0x'+a for a in addrs],capture_output=True,text=True).stdout.split('\n')
src=open('../src/render.c').read().split('\n')
# map render.c line -> enclosing function by scanning
fn_of={}; cur='?'
for i,l in enumerate(src,1):
    m=re.match(r'^(static |)(inline |)(__attribute__\S* |)?[a-zA-Z_][\w ]*?\b(\w+)\(.*\)\s*$',l)
    if m and not l.startswith(' '): cur=m.group(4)
    fn_of[i]=cur
c=collections.Counter()
for i,a in enumerate(addrs):
    f=out[2*i]; loc=out[2*i+1]
    mm=re.search(r'render\.c:(\d+)',loc)
    if mm: c['render:'+fn_of.get(int(mm.group(1)),'?')]+=cyc[a]
    else: c[f+' '+loc.split('/')[-1].split(':')[0]]+=cyc[a]
for k,v in c.most_common(22): print('%5.2f%% %s'%(100*v/T,k))
