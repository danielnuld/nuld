/*
  RetroAchievements telemetry for the NULD Neutrino fork (AFL-3.0).

  xeRAbora v2.1: sample direct words and RAN2/RANL pointer nodes in the
  same VBLANK, then DMA a coherent snapshot into the IOP agent. Node input
  and precomputed offsets live in loader-owned module storage (not ee_core
  BSS). All memory dereferences are range checked inside the IRQ.
  Based on the NULD/ORBIT agent and hacan359's xeRAbora ee_core sampler.
*/
#include <kernel.h>
#include <sifdma.h>
#include "eecore_config.h"
#include "ee_asm.h"
#include "igr.h"
#include "ra_snap.h"

extern int isceSifSetDma(SifDmaTransfer_t *dmat, int count);
extern int isceSifDmaStat(int trid);

#define RA_START_DELAY 600
#define RA_RAM_LOW  0x00080000u
#define RA_RAM_HIGH 0x02000000u
#define RA_SCRATCH_HIGH 0x02004000u // xeRAbora maps 16 KiB at RAM end into 0x70000000

static u32 frames, seq, skip, fail, last_ticks, note_seen;
u32 ra_note_id, ra_note_points;
static int dma_id;
static inline u32 ticks(void) { u32 t; asm volatile("mfc0 %0, $9" : "=r"(t)); return t; }

static inline u32 ra_get_le(const u8 *p, unsigned size)
{
    u32 v = p[0];
    if (size >= 2) v |= (u32)p[1] << 8;
    if (size == 4) v |= ((u32)p[2] << 16) | ((u32)p[3] << 24);
    return v;
}

static inline void ra_put_le(u8 *p, u32 v)
{
    p[0] = (u8)v; p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24);
}

/* Ordinary RAM is readable through its uncached mirror. xeRAbora 2.1
   also watches 16 KiB of PS2 scratchpad (logical 0x02000000..0x02003fff),
   whose CPU address is 0x70000000; never OR it with UNCACHED_SEG. */
static inline int ra_readable(u32 addr, u32 size)
{
    return size && ((addr >= RA_RAM_LOW && addr < RA_RAM_HIGH &&
                     size <= RA_RAM_HIGH - addr) ||
                    (addr >= RA_RAM_HIGH && addr < RA_SCRATCH_HIGH &&
                     size <= RA_SCRATCH_HIGH - addr));
}
static inline volatile u8 *ra_source(u32 addr)
{
    if (addr >= RA_RAM_HIGH)
        return (volatile u8 *)(0x70000000u + addr - RA_RAM_HIGH);
    return (volatile u8 *)UNCACHED_SEG(addr);
}

int RA_OnVblank(void)
{
    u32 now = ticks(), frame_ticks = now - last_ticks, iop;
    u32 n, direct, node_count, total, parts, every, i, j, off;
    u8 *buf, *v;
    struct ra_snap *s;
    last_ticks = now;
    if (eec.RaMbox == NULL || ++frames <= RA_START_DELAY)
        return 0;
    volatile u32 *mb = (volatile u32 *)UNCACHED_SEG(eec.RaMbox);
    iop = mb[0];
    if (!iop)
        return 0;
    if (mb[1] != note_seen) {
        note_seen = mb[1], ra_note_id = mb[2], ra_note_points = mb[3];
        return 1;
    }
    n = eec.RaCount;
    direct = eec.RaBytes;
    node_count = eec.RaNodeCount;
    if (!eec.RaWatch || n == 0 || n > RA_WATCH_MAX || direct == 0 ||
        direct > RA_SNAP_MAX_BYTES || node_count > RA_NODE_MAX ||
        (node_count && (!eec.RaNodeList || !eec.RaNodeWork)) ||
        node_count * RA_NODE_PAIR_BYTES > RA_SNAP_MAX_BYTES - direct)
        return 0;
    total = direct + node_count * RA_NODE_PAIR_BYTES;
    parts = (total + RA_SNAP_CHUNK_BYTES - 1) / RA_SNAP_CHUNK_BYTES;
    // Do not exceed the ~215 packet/s measured SMAP ceiling: one- and
    // two-packet sets retain full-speed frames, larger sets are paced.
    every = (parts + 1) / 2;
    if (every == 0 || frames % every != 0)
        return 0;
    // Never wait on a prior SIF DMA or the game's own SIF1 operation.
    if ((dma_id && isceSifDmaStat(dma_id) >= 0) ||
        (*(volatile u32 *)0x1000C400 & 0x100)) {
        skip++;
        return 0;
    }
    buf = eec.RaMbox + 64;
    s = (struct ra_snap *)UNCACHED_SEG(buf);
    v = (u8 *)UNCACHED_SEG(buf + RA_SNAP_HDR);
    s->magic = RA_SNAP_MAGIC;
    s->seq = ++seq;
    s->frames = frames;
    s->dma_skip = skip;
    s->dma_fail = fail;
    s->count = n;
    s->bytes = total;
    for (i = 0; i < sizeof(s->game_id); i++)
        s->game_id[i] = i < sizeof(eec.GameID) ? eec.GameID[i] : 0;

    off = 0;
    for (i = 0; i < n; i++) {
        u32 e = eec.RaWatch[i], addr = RA_WATCH_ADDR(e), z = RA_WATCH_SIZE(e);
        if ((z != 1 && z != 2 && z != 4) || z > direct - off)
            return 0;
        if (!ra_readable(addr, z)) {
            for (j = 0; j < z; j++) v[off++] = 0;
        } else if (z == 4 && !(addr & 3)) {
            u32 x = *(volatile u32 *)ra_source(addr);
            v[off++] = x; v[off++] = x >> 8;
            v[off++] = x >> 16; v[off++] = x >> 24;
        } else if (z == 2 && !(addr & 1)) {
            u16 x = *(volatile u16 *)ra_source(addr);
            v[off++] = x; v[off++] = x >> 8;
        } else {
            for (j = 0; j < z; j++)
                v[off++] = *ra_source(addr + j);
        }
    }
    if (off != direct)
        return 0;

    // Each resolved node appends an (address,value) pair. A zero address
    // means unresolved; a resolved value of zero is still valid.
    for (i = 0; i < node_count; i++) {
        const struct ra_node *nodes = (const struct ra_node *)eec.RaNodeList;
        uint32_t *work = eec.RaNodeWork;
        u32 w = nodes[i].w, parent = RA_NODE_PARENT(w);
        u32 z = RA_NODE_SIZE(w), base = 0, addr = 0, value = 0;
        int valid = z == 1 || z == 2 || z == 4;
        if (RA_NODE_WIDTH(w) > 32)
            valid = 0;
        if (valid && RA_NODE_FROM_NODE(w)) {
            if (parent >= i || ra_get_le((const u8 *)&v[direct + parent * 8], 4) == 0)
                valid = 0;
            else
                base = work[parent * 2];
        } else if (valid) {
            u32 layout = work[i * 2 + 1], from = layout >> 4, psize = layout & 15;
            if (!layout || parent >= n || (psize != 1 && psize != 2 && psize != 4) ||
                from > direct || psize > direct - from)
                valid = 0;
            else
                base = ra_get_le((const u8 *)&v[from], psize);
        }
        if (valid && (w & RA_NODE_LEGACY) && base == 0)
            valid = 0;
        if (valid) {
            u32 width = RA_NODE_WIDTH(w);
            base >>= RA_NODE_SHIFT(w);
            if (width && width < 32) base &= (1u << width) - 1;
            addr = (base * nodes[i].scale + nodes[i].offset) & 0x1FFFFFFFu;
            if (!ra_readable(addr, z))
                addr = 0;
            else
                for (j = 0; j < z; j++)
                    value |= (u32)(*ra_source(addr + j)) << (j * 8);
        }
        work[i * 2] = value;
        ra_put_le(&v[off], addr);
        ra_put_le(&v[off + 4], value);
        off += 8;
    }
    s->read_cycles = ticks() - now;
    s->frame_cycles = frame_ticks;
    s->seq_end = seq;
    *(volatile u32 *)UNCACHED_SEG(buf + RA_SNAP_TRAILER_OFF(total)) = seq;
    SifDmaTransfer_t t = {buf, (void *)iop, RA_SNAP_DMA_SIZE(total), 0};
    if ((dma_id = isceSifSetDma(&t, 1)) == 0)
        fail++;
    return 0;
}
