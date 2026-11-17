import sys, subprocess, bisect, collections
elf, prof = sys.argv[1], sys.argv[2]
syms = []
for l in subprocess.run(['llvm-nm', '-n', elf], capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16) & ~1, p[2]))
syms.sort(); addrs = [a for a, _ in syms]
tot = collections.Counter(); T = 0
for l in open(prof):
    a, c = l.split(); a = int(a, 16); c = int(c); T += c
    i = bisect.bisect_right(addrs, a) - 1
    tot[syms[i][1] if i >= 0 else '?'] += c
for n, c in tot.most_common(int(sys.argv[3]) if len(sys.argv) > 3 else 25):
    print('%6.2f%%  %s' % (100.0 * c / T, n))
