/* gbaprof: a small headless GBA (ARM7TDMI) interpreter with approximate cycle counting.
   Purpose: measure the frame rate of Steel Harbor ROMs (counts Mode 4 page flips per emulated
   second) and dump the displayed page. Not a general emulator: no BIOS (a tiny IRQ stub),
   no audio, no OBJ/BG rendering beyond Mode 4 page dumps, timers without cascade.

   usage: gbaprof rom.gba seconds "frames:keys,frames:keys,..." [dump_flip,dump_flip,...] [outprefix]
          keys = hex GBA key bits (1=A 2=B 4=SEL 8=START 10=R 20=L 40=U 80=D 100=R-shoulder 200=L-sh.)
   prints flips, fps, and per-second fps. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int32_t s32;
typedef uint64_t u64; typedef int64_t s64;

static u8 bios[0x4000], ewram[0x40000], iwram[0x8000], io[0x400], pal[0x400], vram[0x18000], oam[0x400];
static u8 *rom; static u32 romsize; static u8 sram[0x8000];

static u32 r[16];
static int fn, fz, fc, fv, thumb, irqdis, mode;   /* mode bits 0..4 */
static u32 bank_sp[3], bank_lr[3], spsr_irq, spsr_svc;   /* 0 usr/sys, 1 irq, 2 svc */
static u64 cycles;
static int seqfetch;            /* next code fetch sequential */
static int pf, pfacc;           /* ROM prefetch buffer model */
static u32 waitcnt; static int ew_fast;
static int romN16 = 5, romS16 = 3;

/* timers */
static u16 tm_reload[4], tm_ctl[4]; static u64 tm_start[4];
static u64 frame_start;         /* cycle when current video frame started */
static int vblank_done;
static long flips; static u64 flip_cyc[100000], wait_cyc[100000]; static u64 wait_acc; static u32 wait_lo, wait_hi;
static u32 min_sp = 0xFFFFFFFF, min_isp = 0xFFFFFFFF, min_sp_pc, min_sp_lr; static u8 min_stack[0x800];
static u32 callee_addr; static u32 call_hist[1000000]; static long ncall_hist;
static u32 keys_now;
static int dump_list[256], ndump; static const char *outpre = "gp";

#define FRAME_CYC 280896
#define LINE_CYC 1232

static void fatal(const char *m, u32 a) { fprintf(stderr, "gbaprof: %s %08x at pc=%08x thumb=%d cyc=%llu\n", m, a, r[15], thumb, (unsigned long long)cycles); exit(1); }

/* ---------------------------------------------------------------- timing */
static int wait_of(u32 a, int w32, int seq)
{
    switch (a >> 24) {
    case 0x02: return ew_fast ? (w32 ? 4 : 2) : (w32 ? 6 : 3);
    case 0x05: case 0x06: return w32 ? 2 : 1;
    case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
        if (w32) return (seq ? romS16 : romN16) + romS16;
        return seq ? romS16 : romN16;
    case 0x0E: return 5;
    default: return 1;
    }
}
static inline void busy(int k)       /* cycles where the prefetcher can work */
{
    if (!(waitcnt & 0x4000)) return;
    pfacc += k;
    while (pfacc >= romS16 && pf < 8) { pf++; pfacc -= romS16; }
    if (pf >= 8) pfacc = 0;
}
static inline void data_cyc(u32 a, int w32, int seq)
{
    int k = wait_of(a, w32, seq);
    cycles += k;
    if ((a >> 24) >= 8 && (a >> 24) <= 0xD) { pf = 0; pfacc = 0; } else busy(k);
}
static inline void icyc(int k) { cycles += k; busy(k); }

static void recompute_rom(void)
{
    static const int nt[4] = { 4, 3, 2, 8 };
    romN16 = 1 + nt[(waitcnt >> 2) & 3];
    romS16 = 1 + (((waitcnt >> 4) & 1) ? 1 : 2);
}

/* ---------------------------------------------------------------- IO */
/* FIXDT=n: timer 2 (the game clock) advances n ticks per read and the key script is indexed by
   game frame (one read of the game clock per main-loop pass) instead of VBlank, so every build plays
   exactly the same game however long its frames take: pure cost comparison. (Indexing by displayed
   frame was not enough: the VBlank key latch then saw the next frame's keys at speed-dependent times.) */
static int fixdt; static u32 fixdt_acc; static long game_frames;
static u32 keys_for_frame(long vf);
static u16 timer_val(int n)
{
    if (!(tm_ctl[n] & 0x80)) return tm_reload[n];
    static const int ps[4] = { 1, 64, 256, 1024 };
    u64 t = (cycles - tm_start[n]) / ps[tm_ctl[n] & 3];
    u32 period = 65536 - tm_reload[n];
    return (u16)(tm_reload[n] + t % period);
}
static u16 io_rd16(u32 a)
{
    a &= 0x3FE;
    u64 fc_ = cycles - frame_start;
    int line = (int)(fc_ / LINE_CYC);
    switch (a) {
    case 0x004: { u16 v = *(u16 *)&io[4] & 0xFFF8; if (line >= 160 && line < 227) v |= 1; if ((fc_ % LINE_CYC) >= 960) v |= 2; if (line == (*(u16 *)&io[4] >> 8)) v |= 4; return v; }
    case 0x006: return (u16)line;
    case 0x108: if (fixdt) { fixdt_acc += fixdt; game_frames++; return (u16)fixdt_acc; } return timer_val(2);
    case 0x100: case 0x104: case 0x10C: return timer_val((a - 0x100) >> 2);
    case 0x130: return (u16)(~(fixdt ? keys_for_frame(game_frames) : keys_now) & 0x3FF);
    }
    return *(u16 *)&io[a];
}
static void dump_page(long fl);
static void do_dma(int n)
{
    u32 base = 0xB0 + n * 12;
    u32 src = *(u32 *)&io[base], dst = *(u32 *)&io[base + 4];
    u32 cnt = *(u16 *)&io[base + 8]; u16 ctl = *(u16 *)&io[base + 10];
    if (((ctl >> 12) & 3) != 0) return;          /* only immediate DMA is emulated */
    if (cnt == 0) cnt = n == 3 ? 0x10000 : 0x4000;
    int w32 = (ctl >> 10) & 1;
    int dinc = (ctl >> 5) & 3, sinc = (ctl >> 7) & 3;
    extern u32 rd32n(u32), rd16n(u32); extern void wr32n(u32, u32), wr16n(u32, u16);
    for (u32 i = 0; i < cnt; i++) {
        if (w32) wr32n(dst, rd32n(src)); else wr16n(dst, (u16)rd16n(src));
        cycles += wait_of(src, w32, i > 0) + wait_of(dst, w32, i > 0);
        int sz = w32 ? 4 : 2;
        if (sinc == 0) src += sz; else if (sinc == 1) src -= sz;
        if (dinc == 0 || dinc == 3) dst += sz; else if (dinc == 1) dst -= sz;
    }
    cycles += 2;
    *(u16 *)&io[base + 10] &= 0x7FFF;
}
static void io_wr16(u32 a, u16 v)
{
    if ((a & 0xFFFC) == 0x800) { if (a & 2) ew_fast = ((v >> 8) & 0xF) == 0xE; return; }   /* memory control: EWRAM wait states */
    a &= 0x3FE;
    if (a == 0x202) { *(u16 *)&io[0x202] &= ~v; return; }
    if (a == 0x000) {
        u16 old = *(u16 *)&io[0];
        *(u16 *)&io[0] = v;
        if ((old ^ v) & 0x10) {
            if (flips < 100000) { flip_cyc[flips] = cycles; wait_cyc[flips] = wait_acc; }
            flips++;
            for (int i = 0; i < ndump; i++) if (dump_list[i] == flips) dump_page(flips);
        }
        return;
    }
    if (a >= 0x100 && a < 0x110) {
        int n = (a - 0x100) >> 2;
        if (a & 2) {
            if (!(tm_ctl[n] & 0x80) && (v & 0x80)) tm_start[n] = cycles;
            tm_ctl[n] = v;
        } else tm_reload[n] = v;
        *(u16 *)&io[a] = v;
        return;
    }
    if (a == 0x204) { waitcnt = v; recompute_rom(); }
    *(u16 *)&io[a] = v;
    if (a == 0xBA || a == 0xC6 || a == 0xD2 || a == 0xDE) if (v & 0x8000) do_dma((a - 0xBA) / 12);
}

/* ---------------------------------------------------------------- memory */
static inline u8 *vptr(u32 a) { a &= 0x1FFFF; if (a >= 0x18000) a -= 0x8000; return &vram[a]; }
static u32 rd8n(u32 a)
{
    switch (a >> 24) {
    case 0x00: return a < 0x4000 ? bios[a] : 0;
    case 0x02: return ewram[a & 0x3FFFF];
    case 0x03: return iwram[a & 0x7FFF];
    case 0x04: { u16 h = io_rd16(a & ~1); return (a & 1) ? h >> 8 : h & 0xFF; }
    case 0x05: return pal[a & 0x3FF];
    case 0x06: return *vptr(a);
    case 0x07: return oam[a & 0x3FF];
    case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
        a &= 0x1FFFFFF; return a < romsize ? rom[a] : ((a >> 1) >> ((a & 1) * 8)) & 0xFF;
    case 0x0E: return sram[a & 0x7FFF];
    }
    return 0;
}
u32 rd16n(u32 a)
{
    a &= ~1u;
    switch (a >> 24) {
    case 0x00: return a < 0x4000 ? *(u16 *)&bios[a] : 0;
    case 0x02: return *(u16 *)&ewram[a & 0x3FFFF];
    case 0x03: return *(u16 *)&iwram[a & 0x7FFF];
    case 0x04: return io_rd16(a);
    case 0x05: return *(u16 *)&pal[a & 0x3FF];
    case 0x06: return *(u16 *)vptr(a);
    case 0x07: return *(u16 *)&oam[a & 0x3FF];
    case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
        a &= 0x1FFFFFF; return a < romsize ? *(u16 *)&rom[a] : (a >> 1) & 0xFFFF;
    case 0x0E: return 0xFFFF;
    }
    return 0;
}
u32 rd32n(u32 a)
{
    a &= ~3u;
    switch (a >> 24) {
    case 0x02: return *(u32 *)&ewram[a & 0x3FFFF];
    case 0x03: return *(u32 *)&iwram[a & 0x7FFF];
    case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
        a &= 0x1FFFFFF; if (a + 3 < romsize) return *(u32 *)&rom[a];
    }
    return rd16n(a) | (rd16n(a + 2) << 16);
}
static void wr8n(u32 a, u8 v)
{
    switch (a >> 24) {
    case 0x02: ewram[a & 0x3FFFF] = v; return;
    case 0x03: iwram[a & 0x7FFF] = v; return;
    case 0x04: { u16 h = *(u16 *)&io[a & 0x3FE]; if (a & 1) h = (h & 0xFF) | (v << 8); else h = (h & 0xFF00) | v; io_wr16(a & ~1, h); return; }
    case 0x05: *(u16 *)&pal[a & 0x3FE] = v | (v << 8); return;
    case 0x06: *(u16 *)vptr(a & ~1) = v | (v << 8); return;
    case 0x0E: sram[a & 0x7FFF] = v; return;
    }
}
void wr16n(u32 a, u16 v)
{
    a &= ~1u;
    switch (a >> 24) {
    case 0x02: *(u16 *)&ewram[a & 0x3FFFF] = v; return;
    case 0x03: *(u16 *)&iwram[a & 0x7FFF] = v; return;
    case 0x04: io_wr16(a, v); return;
    case 0x05: *(u16 *)&pal[a & 0x3FF] = v; return;
    case 0x06: *(u16 *)vptr(a) = v; return;
    case 0x07: *(u16 *)&oam[a & 0x3FF] = v; return;
    }
}
void wr32n(u32 a, u32 v)
{
    a &= ~3u;
    switch (a >> 24) {
    case 0x02: *(u32 *)&ewram[a & 0x3FFFF] = v; return;
    case 0x03: *(u32 *)&iwram[a & 0x7FFF] = v; return;
    }
    wr16n(a, (u16)v); wr16n(a + 2, (u16)(v >> 16));
}
/* timed accessors */
static inline u32 rd8(u32 a, int s)  { data_cyc(a, 0, s); return rd8n(a); }
static inline u32 rd16(u32 a, int s) { data_cyc(a, 0, s); return rd16n(a); }
static inline u32 rd32(u32 a, int s) { data_cyc(a, 1, s); return rd32n(a); }
static inline void wr8(u32 a, u32 v, int s)  { data_cyc(a, 0, s); wr8n(a, (u8)v); }
static inline void wr16(u32 a, u32 v, int s) { data_cyc(a, 0, s); wr16n(a, (u16)v); }
static inline void wr32(u32 a, u32 v, int s) { data_cyc(a, 1, s); wr32n(a, v); }

/* ---------------------------------------------------------------- CPU helpers */
static u32 get_cpsr(void) { return (fn << 31) | (fz << 30) | (fc << 29) | (fv << 28) | (irqdis << 7) | (thumb << 5) | mode; }
static int bank_of(int m) { return m == 0x12 ? 1 : m == 0x13 ? 2 : 0; }
static void set_mode(int m)
{
    int ob = bank_of(mode), nb = bank_of(m);
    if (ob != nb) { bank_sp[ob] = r[13]; bank_lr[ob] = r[14]; r[13] = bank_sp[nb]; r[14] = bank_lr[nb]; }
    mode = m;
}
static void set_cpsr(u32 v, int ctl)
{
    fn = v >> 31; fz = (v >> 30) & 1; fc = (v >> 29) & 1; fv = (v >> 28) & 1;
    if (ctl) { irqdis = (v >> 7) & 1; thumb = (v >> 5) & 1; set_mode(v & 0x1F); }
}
static u32 *spsr_ptr(void) { return mode == 0x12 ? &spsr_irq : mode == 0x13 ? &spsr_svc : 0; }

static inline int cond_ok(u32 c)
{
    switch (c) {
    case 0: return fz; case 1: return !fz; case 2: return fc; case 3: return !fc;
    case 4: return fn; case 5: return !fn; case 6: return fv; case 7: return !fv;
    case 8: return fc && !fz; case 9: return !fc || fz; case 10: return fn == fv; case 11: return fn != fv;
    case 12: return !fz && fn == fv; case 13: return fz || fn != fv; case 14: return 1;
    }
    return 0;
}
static inline u32 ror32(u32 v, int s) { s &= 31; return s ? (v >> s) | (v << (32 - s)) : v; }
static inline void setnz(u32 v) { fn = v >> 31; fz = v == 0; }
static inline u32 add_f(u32 a, u32 b, u32 cin, int s)
{
    u64 t = (u64)a + b + cin; u32 res = (u32)t;
    if (s) { setnz(res); fc = (t >> 32) & 1; fv = ((~(a ^ b) & (a ^ res)) >> 31) & 1; }
    return res;
}
static inline u32 sub_f(u32 a, u32 b, u32 cin, int s)   /* a - b - !cin */
{
    return add_f(a, ~b, cin, s);
}
static void branch_to(u32 a)
{
    r[15] = a & (thumb ? ~1u : ~3u);
    seqfetch = 0; pf = 0; pfacc = 0;
    /* pipeline refill: one extra sequential fetch */
    cycles += wait_of(r[15], !thumb, 1);
}
static int mul_cyc(u32 m)
{
    if ((m & 0xFFFFFF00) == 0 || (m & 0xFFFFFF00) == 0xFFFFFF00) return 1;
    if ((m & 0xFFFF0000) == 0 || (m & 0xFFFF0000) == 0xFFFF0000) return 2;
    if ((m & 0xFF000000) == 0 || (m & 0xFF000000) == 0xFF000000) return 3;
    return 4;
}

static void enter_irq(u32 ret)
{
    u32 old = get_cpsr();
    set_mode(0x12);
    spsr_irq = old;
    r[14] = ret + 4;
    thumb = 0; irqdis = 1;
    branch_to(0x18);
}

/* ---------------------------------------------------------------- ARM */
static int branched;
static void wreg(int d, u32 v) { if (d == 15) { branch_to(v); branched = 1; } else r[d] = v; }
static u32 rreg(int n, u32 pc8) { return n == 15 ? pc8 : r[n]; }

static void arm_dp(u32 op, u32 pc)
{
    int opc = (op >> 21) & 15, s = (op >> 20) & 1, rn = (op >> 16) & 15, rd = (op >> 12) & 15;
    u32 b, sc = fc;
    u32 pc8 = pc + 8;
    if (op & (1 << 25)) {
        int rot = ((op >> 8) & 15) * 2; b = ror32(op & 0xFF, rot); if (rot) sc = b >> 31;
    } else {
        int rm = op & 15, typ = (op >> 5) & 3, amt; u32 v;
        if (op & 0x10) { pc8 += 4; amt = r[(op >> 8) & 15] & 0xFF; v = rreg(rm, pc8); icyc(1);
            if (amt == 0) b = v;
            else switch (typ) {
            case 0: if (amt < 32) { sc = (v >> (32 - amt)) & 1; b = v << amt; } else { sc = amt == 32 ? v & 1 : 0; b = 0; } break;
            case 1: if (amt < 32) { sc = (v >> (amt - 1)) & 1; b = v >> amt; } else { sc = amt == 32 ? v >> 31 : 0; b = 0; } break;
            case 2: if (amt < 32) { sc = (v >> (amt - 1)) & 1; b = (u32)((s32)v >> amt); } else { sc = v >> 31; b = sc ? 0xFFFFFFFF : 0; } break;
            default: amt &= 31; if (amt == 0) { sc = v >> 31; b = v; } else { sc = (v >> (amt - 1)) & 1; b = ror32(v, amt); } break;
            }
        } else { amt = (op >> 7) & 31; v = rreg(rm, pc8);
            switch (typ) {
            case 0: if (amt) { sc = (v >> (32 - amt)) & 1; b = v << amt; } else b = v; break;
            case 1: if (amt) { sc = (v >> (amt - 1)) & 1; b = v >> amt; } else { sc = v >> 31; b = 0; } break;
            case 2: if (amt) { sc = (v >> (amt - 1)) & 1; b = (u32)((s32)v >> amt); } else { sc = v >> 31; b = sc ? 0xFFFFFFFF : 0; } break;
            default: if (amt) { sc = (v >> (amt - 1)) & 1; b = ror32(v, amt); } else { b = (fc << 31) | (v >> 1); sc = v & 1; } break;
            }
        }
    }
    u32 a = rreg(rn, pc8), res = 0;
    int logical = 0, wr = 1;
    switch (opc) {
    case 0: res = a & b; logical = 1; break;
    case 1: res = a ^ b; logical = 1; break;
    case 2: res = sub_f(a, b, 1, s); break;
    case 3: res = sub_f(b, a, 1, s); break;
    case 4: res = add_f(a, b, 0, s); break;
    case 5: res = add_f(a, b, fc, s); break;
    case 6: res = sub_f(a, b, fc, s); break;
    case 7: res = sub_f(b, a, fc, s); break;
    case 8: res = a & b; logical = 1; wr = 0; break;
    case 9: res = a ^ b; logical = 1; wr = 0; break;
    case 10: sub_f(a, b, 1, 1); wr = 0; break;
    case 11: add_f(a, b, 0, 1); wr = 0; break;
    case 12: res = a | b; logical = 1; break;
    case 13: res = b; logical = 1; break;
    case 14: res = a & ~b; logical = 1; break;
    case 15: res = ~b; logical = 1; break;
    }
    if (logical && s) { setnz(res); fc = sc; }
    if (wr) {
        if (rd == 15 && s) { u32 *sp = spsr_ptr(); u32 v = sp ? *sp : get_cpsr(); set_cpsr(v, 1); branch_to(res); branched = 1; }
        else wreg(rd, res);
    }
}

static void do_msr(u32 op, u32 v)
{
    u32 mask = 0; if (op & (1 << 16)) mask |= 0xFF; if (op & (1 << 19)) mask |= 0xFF000000;
    if (op & (1 << 22)) { u32 *sp = spsr_ptr(); if (sp) *sp = (*sp & ~mask) | (v & mask); }
    else { u32 nv = (get_cpsr() & ~mask) | (v & mask); set_cpsr(nv, (mask & 0xFF) != 0); }
}
static void arm_step(u32 op, u32 pc)
{
    if (!cond_ok(op >> 28)) return;
    u32 pc8 = pc + 8;
    switch ((op >> 25) & 7) {
    case 0:
        if ((op & 0x0FFFFFF0) == 0x012FFF10) { u32 t = r[op & 15]; thumb = t & 1; branch_to(t); branched = 1; return; }
        if ((op & 0x0FC000F0) == 0x00000090) {      /* MUL/MLA */
            int rd = (op >> 16) & 15, rn = (op >> 12) & 15, rs = (op >> 8) & 15, rm = op & 15;
            u32 res = r[rm] * r[rs]; int k = mul_cyc(r[rs]);
            if (op & (1 << 21)) { res += r[rn]; k++; }
            icyc(k); r[rd] = res; if (op & (1 << 20)) setnz(res);
            return;
        }
        if ((op & 0x0F8000F0) == 0x00800090) {      /* long multiply */
            int hi = (op >> 16) & 15, lo = (op >> 12) & 15, rs = (op >> 8) & 15, rm = op & 15;
            u64 res; int sg = (op >> 22) & 1;
            if (sg) res = (u64)((s64)(s32)r[rm] * (s64)(s32)r[rs]); else res = (u64)r[rm] * r[rs];
            int k = mul_cyc(r[rs]) + 1;
            if (op & (1 << 21)) { res += ((u64)r[hi] << 32) | r[lo]; k++; }
            icyc(k); r[lo] = (u32)res; r[hi] = (u32)(res >> 32);
            if (op & (1 << 20)) { fn = (res >> 63) & 1; fz = res == 0; }
            return;
        }
        if ((op & 0x0FB00FF0) == 0x01000090) {      /* SWP */
            int rn = (op >> 16) & 15, rd = (op >> 12) & 15, rm = op & 15;
            u32 ad = r[rn];
            if (op & (1 << 22)) { u32 t = rd8(ad, 0); wr8(ad, r[rm], 0); r[rd] = t; }
            else { u32 t = ror32(rd32(ad, 0), (ad & 3) * 8); wr32(ad, r[rm], 0); r[rd] = t; }
            icyc(1); return;
        }
        if ((op & 0x90) == 0x90 && (op & 0x60)) {   /* halfword / signed transfers */
            int p = (op >> 24) & 1, u = (op >> 23) & 1, im = (op >> 22) & 1, w = (op >> 21) & 1, l = (op >> 20) & 1;
            int rn = (op >> 16) & 15, rd = (op >> 12) & 15, sh = (op >> 5) & 3;
            u32 off = im ? (((op >> 4) & 0xF0) | (op & 15)) : r[op & 15];
            u32 base = rreg(rn, pc8), ad = p ? (u ? base + off : base - off) : base;
            if (l) {
                u32 v;
                if (sh == 1) { v = rd16(ad, 0); if (ad & 1) v = ror32(v, 8); }
                else if (sh == 2) v = (u32)(s32)(int8_t)rd8(ad, 0);
                else { v = (ad & 1) ? (u32)(s32)(int8_t)rd8(ad, 0) : (u32)(s32)(int16_t)rd16(ad, 0); }
                icyc(1);
                if (!p) { u32 nb = u ? base + off : base - off; if (rn != rd) r[rn] = nb; }
                else if (w && rn != rd) r[rn] = ad;
                wreg(rd, v);
            } else {
                wr16(ad, rreg(rd, pc8 + 4), 0);
                if (!p) r[rn] = u ? base + off : base - off; else if (w) r[rn] = ad;
            }
            return;
        }
        if ((op & 0x0FBF0FFF) == 0x010F0000) {      /* MRS */
            int rd = (op >> 12) & 15;
            if (op & (1 << 22)) { u32 *sp = spsr_ptr(); r[rd] = sp ? *sp : get_cpsr(); } else r[rd] = get_cpsr();
            return;
        }
        if ((op & 0x0FB0FFF0) == 0x0120F000) { do_msr(op, r[op & 15]); return; }
        arm_dp(op, pc); return;
    case 1:
        if ((op & 0x0FB0F000) == 0x0320F000) { do_msr(op, ror32(op & 0xFF, ((op >> 8) & 15) * 2)); return; }
        arm_dp(op, pc); return;
    case 2: case 3: {                                /* LDR/STR */
        int p = (op >> 24) & 1, u = (op >> 23) & 1, bb = (op >> 22) & 1, w = (op >> 21) & 1, l = (op >> 20) & 1;
        int rn = (op >> 16) & 15, rd = (op >> 12) & 15;
        u32 off;
        if (op & (1 << 25)) {
            int amt = (op >> 7) & 31, typ = (op >> 5) & 3; u32 v = r[op & 15];
            switch (typ) {
            case 0: off = v << amt; break;
            case 1: off = amt ? v >> amt : 0; break;
            case 2: off = amt ? (u32)((s32)v >> amt) : ((s32)v < 0 ? 0xFFFFFFFF : 0); break;
            default: off = amt ? ror32(v, amt) : ((fc << 31) | (v >> 1)); break;
            }
        } else off = op & 0xFFF;
        u32 base = rreg(rn, pc8), ad = p ? (u ? base + off : base - off) : base;
        if (l) {
            u32 v = bb ? rd8(ad, 0) : ror32(rd32(ad, 0), (ad & 3) * 8);
            icyc(1);
            if (!p) { if (rn != rd) r[rn] = u ? base + off : base - off; }
            else if (w && rn != rd) r[rn] = ad;
            wreg(rd, v);
        } else {
            u32 v = rreg(rd, pc8 + 4);
            if (bb) wr8(ad, v, 0); else wr32(ad, v, 0);
            if (!p) r[rn] = u ? base + off : base - off; else if (w) r[rn] = ad;
        }
        return; }
    case 4: {                                        /* LDM/STM */
        int p = (op >> 24) & 1, u = (op >> 23) & 1, sbit = (op >> 22) & 1, w = (op >> 21) & 1, l = (op >> 20) & 1;
        int rn = (op >> 16) & 15; u32 list = op & 0xFFFF;
        int n = __builtin_popcount(list); if (!n) return;
        u32 base = r[rn], start;
        if (u) start = p ? base + 4 : base; else start = p ? base - 4 * n : base - 4 * n + 4;
        u32 nb = u ? base + 4 * n : base - 4 * n;
        u32 ad = start; int first = 1;
        if (l) {
            if (w) r[rn] = nb;
            u32 newpc = 0; int haspc = 0;
            for (int i = 0; i < 16; i++) if (list & (1 << i)) {
                u32 v = rd32(ad, !first); first = 0; ad += 4;
                if (i == 15) { newpc = v; haspc = 1; } else r[i] = v;
            }
            icyc(1);
            if (haspc) {
                if (sbit) { u32 *sp = spsr_ptr(); if (sp) set_cpsr(*sp, 1); }
                branch_to(newpc); branched = 1;
            }
        } else {
            for (int i = 0; i < 16; i++) if (list & (1 << i)) {
                u32 v = i == 15 ? pc + 12 : r[i];
                wr32(ad, v, !first); first = 0; ad += 4;
            }
            if (w) r[rn] = nb;
        }
        return; }
    case 5: {
        s32 off = (s32)(op << 8) >> 6;
        if (op & (1 << 24)) r[14] = pc + 4;
        branch_to(pc8 + off); branched = 1; return; }
    case 7:
        if (op & (1 << 24)) fatal("SWI", op);
        return;
    default: fatal("undefined ARM op", op);
    }
}

/* ---------------------------------------------------------------- Thumb */
static void thumb_step(u16 op, u32 pc)
{
    u32 pc4 = pc + 4;
    switch (op >> 11) {
    case 0: case 1: case 2: {   /* shifts imm */
        int typ = op >> 11, amt = (op >> 6) & 31, rs = (op >> 3) & 7, rd = op & 7; u32 v = r[rs], res;
        if (typ == 0) { if (amt) { fc = (v >> (32 - amt)) & 1; res = v << amt; } else res = v; }
        else if (typ == 1) { if (amt) { fc = (v >> (amt - 1)) & 1; res = v >> amt; } else { fc = v >> 31; res = 0; } }
        else { if (amt) { fc = (v >> (amt - 1)) & 1; res = (u32)((s32)v >> amt); } else { fc = v >> 31; res = fc ? 0xFFFFFFFF : 0; } }
        r[rd] = res; setnz(res); return; }
    case 3: {
        int rn = (op >> 6) & 7, rs = (op >> 3) & 7, rd = op & 7;
        u32 b = (op & 0x400) ? (u32)rn : r[rn];
        r[rd] = (op & 0x200) ? sub_f(r[rs], b, 1, 1) : add_f(r[rs], b, 0, 1); return; }
    case 4: { int rd = (op >> 8) & 7; r[rd] = op & 0xFF; setnz(r[rd]); return; }
    case 5: { int rd = (op >> 8) & 7; sub_f(r[rd], op & 0xFF, 1, 1); return; }
    case 6: { int rd = (op >> 8) & 7; r[rd] = add_f(r[rd], op & 0xFF, 0, 1); return; }
    case 7: { int rd = (op >> 8) & 7; r[rd] = sub_f(r[rd], op & 0xFF, 1, 1); return; }
    case 8:
        if (!(op & 0x400)) {    /* ALU */
            int o = (op >> 6) & 15, rs = (op >> 3) & 7, rd = op & 7; u32 a = r[rd], b = r[rs], res;
            switch (o) {
            case 0: res = a & b; setnz(res); r[rd] = res; break;
            case 1: res = a ^ b; setnz(res); r[rd] = res; break;
            case 2: { int s = b & 0xFF; icyc(1); if (s == 0) res = a; else if (s < 32) { fc = (a >> (32 - s)) & 1; res = a << s; } else { fc = s == 32 ? a & 1 : 0; res = 0; } setnz(res); r[rd] = res; break; }
            case 3: { int s = b & 0xFF; icyc(1); if (s == 0) res = a; else if (s < 32) { fc = (a >> (s - 1)) & 1; res = a >> s; } else { fc = s == 32 ? a >> 31 : 0; res = 0; } setnz(res); r[rd] = res; break; }
            case 4: { int s = b & 0xFF; icyc(1); if (s == 0) res = a; else if (s < 32) { fc = (a >> (s - 1)) & 1; res = (u32)((s32)a >> s); } else { fc = a >> 31; res = fc ? 0xFFFFFFFF : 0; } setnz(res); r[rd] = res; break; }
            case 5: r[rd] = add_f(a, b, fc, 1); break;
            case 6: r[rd] = sub_f(a, b, fc, 1); break;
            case 7: { int s = b & 0xFF; icyc(1); if (s) { s &= 31; if (s == 0) fc = a >> 31; else { fc = (a >> (s - 1)) & 1; } res = ror32(a, s); } else res = a; setnz(res); r[rd] = res; break; }
            case 8: setnz(a & b); break;
            case 9: r[rd] = sub_f(0, b, 1, 1); break;
            case 10: sub_f(a, b, 1, 1); break;
            case 11: add_f(a, b, 0, 1); break;
            case 12: res = a | b; setnz(res); r[rd] = res; break;
            case 13: res = a * b; icyc(mul_cyc(a)); setnz(res); r[rd] = res; break;
            case 14: res = a & ~b; setnz(res); r[rd] = res; break;
            default: res = ~b; setnz(res); r[rd] = res; break;
            }
            return;
        } else {                /* hi regs / BX */
            int o = (op >> 8) & 3, rs = ((op >> 3) & 15), rd = (op & 7) | ((op >> 4) & 8);
            u32 b = rs == 15 ? pc4 : r[rs];
            switch (o) {
            case 0: { u32 a = rd == 15 ? pc4 : r[rd]; u32 v = a + b; if (rd == 15) { branch_to(v); branched = 1; } else r[rd] = v; break; }
            case 1: { u32 a = rd == 15 ? pc4 : r[rd]; sub_f(a, b, 1, 1); break; }
            case 2: if (rd == 15) { branch_to(b); branched = 1; } else r[rd] = b; break;
            default: thumb = b & 1; branch_to(b); branched = 1; break;
            }
            return;
        }
    case 9: { int rd = (op >> 8) & 7; u32 ad = (pc4 & ~3u) + (op & 0xFF) * 4; r[rd] = rd32(ad, 0); icyc(1); return; }
    case 10: case 11: {
        int ro = (op >> 6) & 7, rb = (op >> 3) & 7, rd = op & 7; u32 ad = r[rb] + r[ro];
        switch ((op >> 9) & 7) {
        case 0: wr32(ad, r[rd], 0); break;
        case 1: wr16(ad, r[rd], 0); break;
        case 2: wr8(ad, r[rd], 0); break;
        case 3: r[rd] = (u32)(s32)(int8_t)rd8(ad, 0); icyc(1); break;
        case 4: r[rd] = ror32(rd32(ad, 0), (ad & 3) * 8); icyc(1); break;
        case 5: { u32 v = rd16(ad, 0); if (ad & 1) v = ror32(v, 8); r[rd] = v; icyc(1); break; }
        case 6: r[rd] = rd8(ad, 0); icyc(1); break;
        default: r[rd] = (ad & 1) ? (u32)(s32)(int8_t)rd8(ad, 0) : (u32)(s32)(int16_t)rd16(ad, 0); icyc(1); break;
        }
        return; }
    case 12: { int o = (op >> 6) & 31, rb = (op >> 3) & 7, rd = op & 7; wr32(r[rb] + o * 4, r[rd], 0); return; }
    case 13: { int o = (op >> 6) & 31, rb = (op >> 3) & 7, rd = op & 7; u32 ad = r[rb] + o * 4; r[rd] = ror32(rd32(ad, 0), (ad & 3) * 8); icyc(1); return; }
    case 14: { int o = (op >> 6) & 31, rb = (op >> 3) & 7, rd = op & 7; wr8(r[rb] + o, r[rd], 0); return; }
    case 15: { int o = (op >> 6) & 31, rb = (op >> 3) & 7, rd = op & 7; r[rd] = rd8(r[rb] + o, 0); icyc(1); return; }
    case 16: { int o = (op >> 6) & 31, rb = (op >> 3) & 7, rd = op & 7; wr16(r[rb] + o * 2, r[rd], 0); return; }
    case 17: { int o = (op >> 6) & 31, rb = (op >> 3) & 7, rd = op & 7; u32 ad = r[rb] + o * 2; u32 v = rd16(ad, 0); if (ad & 1) v = ror32(v, 8); r[rd] = v; icyc(1); return; }
    case 18: { int rd = (op >> 8) & 7; wr32(r[13] + (op & 0xFF) * 4, r[rd], 0); return; }
    case 19: { int rd = (op >> 8) & 7; u32 ad = r[13] + (op & 0xFF) * 4; r[rd] = ror32(rd32(ad, 0), (ad & 3) * 8); icyc(1); return; }
    case 20: { int rd = (op >> 8) & 7; r[rd] = (pc4 & ~3u) + (op & 0xFF) * 4; return; }
    case 21: { int rd = (op >> 8) & 7; r[rd] = r[13] + (op & 0xFF) * 4; return; }
    case 22: case 23:
        if ((op & 0xFF00) == 0xB000) { u32 o = (op & 0x7F) * 4; if (op & 0x80) r[13] -= o; else r[13] += o; return; }
        if ((op & 0xF600) == 0xB400) {
            int l = (op >> 11) & 1, rr = (op >> 8) & 1; u32 list = op & 0xFF;
            int n = __builtin_popcount(list) + rr;
            if (!l) {   /* push */
                u32 ad = r[13] - 4 * n; r[13] = ad; int first = 1;
                for (int i = 0; i < 8; i++) if (list & (1 << i)) { wr32(ad, r[i], !first); first = 0; ad += 4; }
                if (rr) wr32(ad, r[14], !first);
            } else {
                u32 ad = r[13]; int first = 1;
                for (int i = 0; i < 8; i++) if (list & (1 << i)) { r[i] = rd32(ad, !first); first = 0; ad += 4; }
                u32 npc = 0; if (rr) { npc = rd32(ad, !first); ad += 4; }
                r[13] = ad; icyc(1);
                if (rr) { branch_to(npc); branched = 1; }
            }
            return;
        }
        fatal("undefined thumb", op);
    case 24: case 25: {
        int l = (op >> 11) & 1, rb = (op >> 8) & 7; u32 list = op & 0xFF, ad = r[rb]; int first = 1;
        int inlist = (list >> rb) & 1;
        for (int i = 0; i < 8; i++) if (list & (1 << i)) {
            if (l) r[i] = rd32(ad, !first); else wr32(ad, r[i], !first);
            first = 0; ad += 4;
        }
        if (l) icyc(1);
        if (!(l && inlist)) r[rb] = ad;
        return; }
    case 26: case 27: {
        int c = (op >> 8) & 15;
        if (c == 15) fatal("SWI thumb", op);
        if (cond_ok(c)) { s32 off = (s32)(int8_t)(op & 0xFF) * 2; branch_to(pc4 + off); branched = 1; }
        return; }
    case 28: { s32 off = ((s32)(op << 21) >> 20); branch_to(pc4 + off); branched = 1; return; }
    case 30: { s32 off = ((s32)(op << 21) >> 9); r[14] = pc4 + off; return; }
    case 31: { u32 t = r[14] + ((op & 0x7FF) << 1); r[14] = (pc + 2) | 1; branch_to(t); branched = 1; return; }
    default: fatal("undefined thumb", op);
    }
}

/* ---------------------------------------------------------------- frames */
static void dump_page(long fl)
{
    char fnm[256]; snprintf(fnm, sizeof fnm, "%s%04ld.ppm", outpre, fl);
    FILE *f = fopen(fnm, "wb"); if (!f) return;
    fprintf(f, "P6 240 160 255\n");
    int page = (*(u16 *)&io[0] & 0x10) ? 0xA000 : 0;
    static u16 scr[160][240];
    for (int i = 0; i < 240 * 160; i++) scr[i / 240][i % 240] = *(u16 *)&pal[vram[page + i] * 2];
    /* windows 0/1 with brightness increase on BG2; window spans may come from an HBlank DMA 0 table
       (line 0 uses the registers, line y the table entry y-1) */
    {
        u16 dc = *(u16 *)&io[0], bc = *(u16 *)&io[0x50], d0 = *(u16 *)&io[0xBA];
        if (getenv("WDBG")) fprintf(stderr, "dump %ld dc %04x bc %04x d0 %04x dad %08x bldy %d\n", fl, dc, bc, d0, *(u32 *)&io[0xB4], *(u16 *)&io[0x54]);
        if ((dc & 0x6000) && ((bc >> 6) & 3) == 2 && (bc & 4)) {
            int Y = *(u16 *)&io[0x54] & 31; if (Y > 16) Y = 16;
            int hd = (d0 & 0x8000) && ((d0 >> 12) & 3) == 2 && *(u32 *)&io[0xB4] == 0x04000040;
            u32 sad = *(u32 *)&io[0xB0];
            u16 wi = *(u16 *)&io[0x48];
            for (int y = 0; y < 160; y++) {
                u32 w = (y == 0 || !hd) ? *(u32 *)&io[0x40] : rd32n(sad + 4 * (y - 1));
                {   u16 d3 = *(u16 *)&io[0xDE];             /* per-line BLDY from an HBlank DMA 3 table */
                    if ((d3 & 0x8000) && ((d3 >> 12) & 3) == 2 && *(u32 *)&io[0xD8] == 0x04000054 && y > 0) {
                        Y = rd16n(*(u32 *)&io[0xD4] + 2 * (y - 1)) & 31; if (Y > 16) Y = 16;
                    } else { Y = *(u16 *)&io[0x54] & 31; if (Y > 16) Y = 16; }
                }
                static u8 lit[240]; memset(lit, 0, sizeof lit);
                for (int n = 0; n < 2; n++) {
                    if (!(dc & (0x2000 << n)) || !((wi >> (8 * n)) & 0x20)) continue;
                    u16 h = n ? (u16)(w >> 16) : (u16)w;
                    int l = h >> 8, r = h & 255; if (r > 240) r = 240;
                    for (int x = l; x < r; x++) lit[x] = 1;
                }
                for (int x = 0; x < 240; x++) if (lit[x]) {
                    u16 c = scr[y][x]; int cr = c & 31, cg = (c >> 5) & 31, cb = (c >> 10) & 31;
                    cr += (31 - cr) * Y / 16; cg += (31 - cg) * Y / 16; cb += (31 - cb) * Y / 16;
                    scr[y][x] = (u16)(cr | cg << 5 | cb << 10);
                }
            }
        }
    }
    /* OBJ layer (4bpp or 8bpp, 1D mapping; affine only as plain 2x scale) */
    static const int sw[3][4] = { {8,16,32,64}, {16,32,32,64}, {8,8,16,32} }, sh_[3][4] = { {8,16,32,64}, {8,8,16,32}, {16,32,32,64} };
    for (int o = getenv("NOOBJ") ? -1 : 127; o >= 0; o--) {
        u16 a0 = *(u16 *)&oam[o * 8], a1 = *(u16 *)&oam[o * 8 + 2], a2 = *(u16 *)&oam[o * 8 + 4];
        int aff = (a0 >> 8) & 1, dbl = (a0 >> 9) & 1;
        if (!aff && dbl) continue;
        int shp = a0 >> 14, sz = a1 >> 14; if (shp == 3) continue;
        int w = sw[shp][sz], h = sh_[shp][sz], sc = (aff && dbl) ? 2 : 1;
        int x0 = a1 & 511, y0 = a0 & 255; if (x0 >= 240) x0 -= 512; if (y0 >= 160) y0 -= 256;
        int tile = a2 & 0x3FF, bank = a2 >> 12;
        for (int y = 0; y < h * sc; y++) for (int x = 0; x < w * sc; x++) {
            int sx = x0 + x, sy = y0 + y; if (sx < 0 || sx >= 240 || sy < 0 || sy >= 160) continue;
            int tx = x / sc, ty = y / sc;
            if (a0 & 0x2000) {                    /* 8bpp: tiles are 64 bytes, one palette of 256 */
                int t = tile + ((ty / 8) * (w / 8) + tx / 8) * 2;
                u8 ci = vram[0x10000 + ((t * 32) & 0x7FFF) + (ty & 7) * 8 + (tx & 7)];
                if (ci) scr[sy][sx] = *(u16 *)&pal[0x200 + ci * 2];
                continue;
            }
            int t = tile + (ty / 8) * (w / 8) + tx / 8;
            u8 b = vram[0x10000 + ((t * 32) & 0x7FFF) + (ty & 7) * 4 + (tx & 7) / 2];
            int ci = (tx & 1) ? b >> 4 : b & 15;
            if (ci) scr[sy][sx] = *(u16 *)&pal[0x200 + bank * 32 + ci * 2];
        }
    }
    for (int i = 0; i < 240 * 160; i++) {
        u16 c = scr[i / 240][i % 240];
        u8 px[3] = { (u8)((c & 31) << 3), (u8)(((c >> 5) & 31) << 3), (u8)(((c >> 10) & 31) << 3) };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

static int script_n[4096]; static u32 script_k[4096]; static int nscript;
static u32 keys_for_frame(long vf)
{
    long acc = 0;
    for (int i = 0; i < nscript; i++) { acc += script_n[i]; if (vf < acc) return script_k[i]; }
    return nscript ? script_k[nscript - 1] : 0;
}

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: gbaprof rom seconds script [dumps] [prefix]\n"); return 1; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END); romsize = (u32)ftell(f); fseek(f, 0, SEEK_SET);
    rom = malloc(romsize + 4); fread(rom, 1, romsize, f); fclose(f);
    const char *sramf = getenv("SRAM_FILE");
    if (sramf) { FILE *sf = fopen(sramf, "rb"); if (sf) { fread(sram, 1, sizeof sram, sf); fclose(sf); } else memset(sram, 0xFF, sizeof sram); }
    double secs = atof(argv[2]);
    if (getenv("FIXDT")) fixdt = atoi(getenv("FIXDT"));
    for (char *p = argv[3]; *p && nscript < 4096; ) {
        script_n[nscript] = (int)strtol(p, &p, 10); if (*p == ':') p++;
        script_k[nscript++] = (u32)strtoul(p, &p, 16); if (*p == ',') p++;
    }
    if (argc > 4) for (char *p = argv[4]; *p && ndump < 256; ) { dump_list[ndump++] = (int)strtol(p, &p, 10); if (*p == ',') p++; else if (*p) p++; }
    if (argc > 5) outpre = argv[5];
    /* IRQ stub BIOS */
    u32 stub[] = { 0xE92D500F, 0xE3A00301, 0xE28FE000, 0xE510F004, 0xE8BD500F, 0xE25EF004 };
    *(u32 *)&bios[0x18] = 0xEA000042;
    memcpy(&bios[0x128], stub, sizeof stub);
    mode = 0x13; irqdis = 1; thumb = 0;
    bank_sp[0] = 0x03007F00; bank_sp[1] = 0x03007FA0; bank_sp[2] = 0x03007FE0; r[13] = bank_sp[2];
    r[15] = 0x08000000; seqfetch = 0;
    u64 limit = (u64)(secs * 16777216.0);
    u64 next_vbl = 160 * LINE_CYC;
    long vframe = 0;
    keys_now = keys_for_frame(0);
    u64 icount = 0;
    if (getenv("WAITPC")) { wait_lo = (u32)strtoul(getenv("WAITPC"), 0, 16); wait_hi = wait_lo + 16; }
    if (getenv("CALLEE")) callee_addr = (u32)strtoul(getenv("CALLEE"), 0, 16) & ~1u;
    const char *profp = getenv("PROF");
    u64 *hrom = profp ? calloc(romsize / 2 + 1, 8) : 0, *hiw = profp ? calloc(0x4000, 8) : 0;
    u64 prof_from = (u64)(getenv("PROF_FROM") ? atof(getenv("PROF_FROM")) * 16777216.0 : 0);
    while (cycles < limit) {
        /* video timing */
        while (cycles >= frame_start + FRAME_CYC) { frame_start += FRAME_CYC; vframe++; next_vbl = frame_start + 160 * LINE_CYC; keys_now = keys_for_frame(vframe); }
        if (cycles >= next_vbl) {
            next_vbl = frame_start + FRAME_CYC + 160 * LINE_CYC;
            if (*(u16 *)&io[4] & 8) *(u16 *)&io[0x202] |= 1;
        }
        if (!irqdis && (*(u16 *)&io[0x208] & 1) && (*(u16 *)&io[0x200] & *(u16 *)&io[0x202])) {
            enter_irq(r[15]);
            continue;
        }
        u32 pc = r[15];
        u64 c0 = cycles;
        if (mode == 0x1F && r[13] < min_sp && r[13] > 0x03000000) { min_sp = r[13]; min_sp_pc = pc; min_sp_lr = r[14]; memcpy(min_stack, &iwram[0x7E00 - 0x800], 0x800); }
        if (mode == 0x12 && r[13] < min_isp && r[13] > 0x03000000) min_isp = r[13];
        if (callee_addr && (pc & ~1u) == callee_addr && ncall_hist < 1000000) call_hist[ncall_hist++] = r[14];
        branched = 0;
        icount++;
        if (thumb) {
            /* fetch timing with the ROM prefetch model */
            int isrom = (pc >> 24) >= 8;
            if (isrom && seqfetch && (waitcnt & 0x4000) && pf > 0) { pf--; cycles += 1; }
            else { int k = wait_of(pc, 0, seqfetch); cycles += k; if (!isrom) busy(k); }
            u16 op = (u16)rd16n(pc);
            r[15] = pc + 2; seqfetch = 1;
            thumb_step(op, pc);
        } else {
            int isrom = (pc >> 24) >= 8;
            int k = wait_of(pc, 1, seqfetch); cycles += k; if (!isrom) busy(k);
            u32 op = rd32n(pc);
            r[15] = pc + 4; seqfetch = 1;
            arm_step(op, pc);
        }
        if (wait_hi && pc >= wait_lo && pc < wait_hi) wait_acc += cycles - c0;
        if (profp && c0 >= prof_from) {
            if ((pc >> 24) >= 8) { u32 o = (pc & 0x1FFFFFF) >> 1; if (o <= romsize / 2) hrom[o] += cycles - c0; }
            else if ((pc >> 24) == 3) hiw[(pc & 0x7FFF) >> 1] += cycles - c0;
        }
    }
    if (profp) {
        FILE *pf_ = fopen(profp, "w");
        for (u32 i = 0; i <= romsize / 2; i++) if (hrom[i]) fprintf(pf_, "%08x %llu\n", 0x08000000 + i * 2, (unsigned long long)hrom[i]);
        for (u32 i = 0; i < 0x4000; i++) if (hiw[i]) fprintf(pf_, "%08x %llu\n", 0x03000000 + i * 2, (unsigned long long)hiw[i]);
        fclose(pf_);
    }
    if (callee_addr) { FILE *cf = fopen("callers.txt", "w"); for (long i = 0; i < ncall_hist; i++) fprintf(cf, "%08x\n", call_hist[i]); fclose(cf); }
    double t = cycles / 16777216.0;
    printf("emulated %.2f s, %llu instr, %ld flips, avg %.2f fps\n", t, (unsigned long long)icount, flips, flips / t);
    /* fps per second of emulated time */
    long fi = 0;
    for (int s = 0; s < (int)t; s++) {
        long n = 0; while (fi < flips && fi < 100000 && flip_cyc[fi] < (u64)(s + 1) * 16777216ull) { fi++; n++; }
        printf("%s%ld", s ? " " : "per-second: ", n);
    }
    printf("\n");
    if (getenv("STACK")) printf("lowest stack: sys %08x irq %08x (at pc %08x lr %08x)\n", min_sp, min_isp, min_sp_pc, min_sp_lr);
    if (getenv("STACK")) { FILE *sf = fopen("stackdump.bin", "wb"); fwrite(min_stack, 1, 0x800, sf); fclose(sf); }
    if (getenv("HIST")) {      /* how many VBlanks each displayed frame took */
        /* HIST=seconds, or HIST=first_frame-last_frame (displayed-frame numbers) */
        long h[16] = {0}; double from = atof(getenv("HIST")) * 16777216.0;
        long f0 = 0, f1 = 100000; const char *dash = strchr(getenv("HIST"), '-');
        if (dash) { f0 = atol(getenv("HIST")); f1 = atol(dash + 1); from = 0; }
        u64 c0 = 0, c1 = 0;
        for (long i = 1; i < flips && i < 100000; i++) {
            if (flip_cyc[i - 1] < from || i <= f0 || i > f1) continue;
            if (!c0) c0 = flip_cyc[i - 1]; c1 = flip_cyc[i];
            long v = (long)((flip_cyc[i] - flip_cyc[i - 1] + FRAME_CYC / 2) / FRAME_CYC); if (v > 15) v = 15; h[v]++;
        }
        if (wait_hi) {   /* busy time per frame (flip to flip minus waiting), in 1/10 VBlank steps */
            long bh[80] = {0};
            for (long i = 1; i < flips && i < 100000; i++) {
                if (flip_cyc[i - 1] < from || i <= f0 || i > f1) continue;
                u64 busy = (flip_cyc[i] - flip_cyc[i - 1]) - (wait_cyc[i] - wait_cyc[i - 1]);
                long b = (long)(busy * 10 / FRAME_CYC); if (b > 79) b = 79; bh[b]++;
            }
            printf("busy per frame (VBlanks): "); for (int b = 0; b < 80; b++) if (bh[b]) printf(" %d.%d:%ld", b / 10, b % 10, bh[b]); printf("\n");
        }
        long nf = 0; for (int i = 1; i < 16; i++) nf += h[i];
        printf("vblanks per frame:"); for (int i = 1; i < 16; i++) if (h[i]) printf(" %d:%ld", i, h[i]);
        printf("   -> %.2f fps over %ld frames\n", nf && c1 > c0 ? nf * 16777216.0 / (double)(c1 - c0) : 0.0, nf);
    }
    if (sramf) { FILE *sf = fopen(sramf, "wb"); if (sf) { fwrite(sram, 1, sizeof sram, sf); fclose(sf); } }
    return 0;
}
