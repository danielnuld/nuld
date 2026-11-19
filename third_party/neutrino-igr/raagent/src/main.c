/*
  raagent.irx - RetroAchievements agent for the ORBIT fork of Neutrino (phase 16b), on Neutrino's ministack.

  Loaded into the game by the launcher's -cfg=ra (smap, ministack ip=<ps2 ip>, raagent srv=<client> me=<ps2 ip>);
  the loader appends mbox=<hex>, the EE mailbox in module storage. At start the agent DMAs the address of its
  snapshot buffer into mailbox[0]; ee_core (ra.c) then DMAs one snapshot per frame into that buffer. The agent
  announces the console to xeRAbora's client (RAP1 until RAO1), sends every new, untorn snapshot as RA15 packets and
  a heartbeat (RAH1) every ten seconds. Wire format: xeRAbora protocol/PROTOCOL.md and OPL modules/network/raudp
  (AFL-3.0): fixed-width text fields, "id=" last, the values after it.
*/

#include <irx.h>
#include <loadcore.h>
#include <thbase.h>
#include <intrman.h>
#include <sifman.h>
#include <sysclib.h>
#include <sysmem.h>
#include <stdio.h>
#include "smap.h"
#include "ministack_udp.h"
#include "ra_snap.h"

IRX_ID("raagent", 1, 0);

#define CLIENT_PORT 18194
#define AGENT_PORT  18195
#define PAYLOAD     1472 // 1500 MTU - IP - UDP
#define AGENT_BYTES (PAYLOAD - 256) // compact static buffer for single-packet sets
#define RA_TX_GAP_US 6000 // paced multipart packets, below SMAP's measured 215 packets/s

typedef struct {
    eth_header_t eth;
    ip_header_t ip;
    udp_header_t udp;
    uint8_t payload[PAYLOAD];
} __attribute__((packed, aligned(4))) ra_pkt_t;

static ra_pkt_t pkt;
static udp_socket_t *sock;
static uint32_t srv_ip, mbox, snap_capacity = AGENT_BYTES;
static char image_hash[RA_HASH_LEN + 1];
static char me[16] = "0.0.0.0";
static volatile int answered;
static uint32_t rx, sent, torn;
static volatile uint32_t note_n, note_id, note_pts; // phase 19: unlocks the client announced (RAU1)
// Ordinary one-packet launches retain the low-RAM footprint validated on
// Tekken 5. Larger xeRAbora 2.1 sets allocate bigger buffers only when the
// validated loader snapshot size demands them; allocation failure disarms
// telemetry rather than starting with a dangerously undersized DMA target.
static uint8_t small_snap[RA_SNAP_TOTAL_FOR(AGENT_BYTES)] __attribute__((aligned(64)));
static uint8_t small_stage[AGENT_BYTES];
static uint8_t *snap = small_snap, *stage = small_stage;
static void *large_snap, *large_stage;

static uint32_t parse_ip(const char *s)
{
    uint32_t ip = 0, part = 0;
    for (; *s; s++)
        if (*s == '.')
            ip = ip << 8 | part, part = 0;
        else
            part = part * 10 + (*s - '0');
    return ip << 8 | part;
}

static uint32_t num(const char **p)
{
    uint32_t v = 0;
    while (**p == ' ')
        (*p)++;
    while (**p >= '0' && **p <= '9')
        v = v * 10 + *(*p)++ - '0';
    return v;
}

static int rx_handler(udp_socket_t *s, void *arg, const uint8_t *hdr, uint16_t hdr_len) // smap receive context
{
    uint32_t w[9];
    char *t = (char *)w + 2;
    (void)s, (void)arg;
    // the payload starts at 42 (ETH + IP + UDP), but the SMAP RX FIFO is read in 32-bit words: from 42 the console
    // returned the bytes from 40 (PCSX2 honoured 42), so RAO1 never matched there. Read from 40, the tag at byte 2.
    // No further than the datagram (UDP length, frame bytes 38-39) and 32 bytes: "RAU1 <id> <points>" fits
    int len = hdr_len >= 40 ? (hdr[38] << 8 | hdr[39]) - 8 : 4, n = len + 2 > 32 ? 32 : (len + 2 + 3) & ~3;
    smap_fifo_read(40, w, n < 8 ? 8 : n);
    t[len < 30 ? len : 30] = 0;
    rx++;
    if (!strncmp(t, "RAO1", 4))
        answered = 1;
    else if (!strncmp(t, "RAU1 ", 5)) { // repeats of one unlock: shown once
        const char *p = t + 5;
        uint32_t id = num(&p), pts = num(&p);
        if (id != 0 && id != note_id)
            note_id = id, note_pts = pts, note_n++;
    }
    return -1; // done with the frame: smap moves the RX FIFO read pointer past it
}

static void send_text(const char *t)
{
    int n = strlen(t);
    memcpy(pkt.payload, t, n);
    udp_packet_send(sock, (udp_packet_t *)&pkt, n);
}

static void send_snapshot(const volatile struct ra_snap *s, uint32_t nb)
{
    static uint32_t seq;
    char id[16];
    uint32_t part, parts = (nb + RA_SNAP_CHUNK_BYTES - 1) / RA_SNAP_CHUNK_BYTES;
    uint32_t snapshot_seq = seq++ % 1000000;
    int i, h;
    if (parts == 0 || parts > RA_SNAP_PARTS) return;
    for (i = 0; i < 15; i++) {
        char c = s->game_id[i];
        id[i] = c >= 0x20 && c < 0x7f ? c : '~';
    }
    id[15] = 0;
    for (part = 0; part < parts; part++) {
        uint32_t off = part * RA_SNAP_CHUNK_BYTES;
        uint32_t len = nb - off;
        if (len > RA_SNAP_CHUNK_BYTES) len = RA_SNAP_CHUNK_BYTES;
        h = sprintf((char *)pkt.payload,
                    "RA15 seq=%06u sz=%04u us=00000 mx=00000 rxq=000 fail=000000 err=+000 sk=000000 lk=0000 sq=%06u "
                    "ds=%06u bad=%04u n=%04u vb=%04u pt=%u np=%u rc=%07u fc=%07u id=%s ",
                    snapshot_seq, 0, s->seq % 1000000, s->dma_skip % 1000000,
                    torn % 10000, s->count % 10000, len, part, parts,
                    s->read_cycles > 9999999 ? 9999999 : s->read_cycles,
                    s->frame_cycles > 9999999 ? 9999999 : s->frame_cycles, id);
        if (h <= 0 || h > RA_SNAP_HEAD_BYTES || h + len > PAYLOAD) return;
        // sz is fixed width and at byte 19 in the RA15 header.
        char sz[5];
        sprintf(sz, "%04u", (unsigned)(h + len));
        memcpy(pkt.payload + 19, sz, 4);
        memcpy(pkt.payload + h, stage + off, len);
        udp_packet_send(sock, (udp_packet_t *)&pkt, h + len);
        sent++;
        // One-packet sets remain unchanged. Pace multipart sends instead
        // of trying to enqueue all 9 parts in a single IOP timeslice.
        if (part + 1 < parts) DelayThread(RA_TX_GAP_US);
    }
}

// the mailbox (ra_snap.h): the snapshot buffer's address, then the unlock notices (count, achievement id, points)
static void mbox_write(void)
{
    static uint32_t word[4] __attribute__((aligned(16)));
    SifDmaTransfer_t d;
    int st;
    if (mbox == 0)
        return;
    word[0] = (uint32_t)snap, word[1] = note_n, word[2] = note_id, word[3] = note_pts;
    d.src = word, d.dest = (void *)mbox, d.size = 16, d.attr = 0;
    CpuSuspendIntr(&st);
    sceSifSetDma(&d, 1);
    CpuResumeIntr(st);
}

static void agent_thread(void *arg)
{
    const volatile struct ra_snap *s = (const volatile struct ra_snap *)snap;
    uint32_t last = 0, tick = 0, announce_period = ~0u;
    char t[64];
    (void)arg;
    udp_packet_init((udp_packet_t *)&pkt, srv_ip, CLIENT_PORT);
    uint32_t note_sent = 0;
    mbox_write(); // EE now knows the correctly sized DMA buffer
    for (;; DelayThread(4000), tick++) {
        if (note_n != note_sent)
            note_sent = note_n, mbox_write(); // existing RAU1 notices
        if (!answered) {
            if (tick % 250 == 0)
                sprintf(t, "RAP1 %s %d", me, AGENT_PORT), send_text(t);
            continue;
        }
        if (tick % 2500 == 0)
            sprintf(t, "RAH1 %u %u %06u %08x %x", (unsigned)rx, (unsigned)note_n,
                    (unsigned)(s->seq % 1000000), (unsigned)s->magic,
                    (unsigned)mbox), send_text(t);
        // An image hash from RAH1 permits a second PC to identify a running
        // game without having performed NULD's original support check.
        if (image_hash[0] && s->magic == RA_SNAP_MAGIC &&
            s->frames / 60 != announce_period) {
            char serial[16];
            memcpy(serial, (const void *)s->game_id, 15);
            serial[15] = 0;
            announce_period = s->frames / 60;
            sprintf(t, "RAI1 %.15s %s", serial, image_hash);
            send_text(t);
        }
        if (s->magic != RA_SNAP_MAGIC || s->seq == last)
            continue;
        uint32_t sq = s->seq, nb = s->bytes;
        if (nb == 0 || nb > snap_capacity || nb > RA_SNAP_MAX_BYTES)
            continue;
        memcpy(stage, snap + RA_SNAP_HDR, nb);
        if (*(volatile uint32_t *)(snap + RA_SNAP_TRAILER_OFF(nb)) != sq || s->seq != sq) {
            torn++;
            continue;
        }
        send_snapshot(s, nb);
        last = sq;
    }
}

static int valid_hash(const char *s)
{
    int i;
    for (i = 0; i < RA_HASH_LEN; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return 0;
    }
    return s[RA_HASH_LEN] == 0;
}

int _start(int argc, char *argv[])
{
    iop_thread_t th;
    uint32_t requested = AGENT_BYTES;
    int i;
    for (i = 1; i < argc; i++)
        if (!strncmp(argv[i], "srv=", 4))
            srv_ip = parse_ip(argv[i] + 4);
        else if (!strncmp(argv[i], "me=", 3))
            strncpy(me, argv[i] + 3, sizeof(me) - 1);
        else if (!strncmp(argv[i], "mbox=", 5))
            mbox = strtoul(argv[i] + 5, NULL, 16);
        else if (!strncmp(argv[i], "size=", 5))
            requested = strtoul(argv[i] + 5, NULL, 10);
        else if (!strncmp(argv[i], "hash=", 5) &&
                 strlen(argv[i] + 5) == RA_HASH_LEN && valid_hash(argv[i] + 5))
            memcpy(image_hash, argv[i] + 5, RA_HASH_LEN + 1);

    if (!requested || requested > RA_SNAP_MAX_BYTES)
        return MODULE_NO_RESIDENT_END;
    if (requested > AGENT_BYTES) {
        // Allocate only when needed. Existing one-packet sets keep the
        // proven ~2.5 KiB IOP footprint; failed large allocations disarm RA.
        large_snap = AllocSysMemory(ALLOC_FIRST, RA_SNAP_TOTAL_FOR(requested) + 63, NULL);
        large_stage = AllocSysMemory(ALLOC_FIRST, requested, NULL);
        if (!large_snap || !large_stage) {
            if (large_snap) FreeSysMemory(large_snap);
            if (large_stage) FreeSysMemory(large_stage);
            return MODULE_NO_RESIDENT_END;
        }
        snap = (uint8_t *)(((uint32_t)large_snap + 63u) & ~63u);
        stage = (uint8_t *)large_stage;
        snap_capacity = requested;
        memset(snap, 0, RA_SNAP_TOTAL_FOR(requested));
    }
    printf("raagent: client %08x, me %s, mailbox %08x, buffer %p, %u bytes\n",
           (unsigned)srv_ip, me, (unsigned)mbox, snap, (unsigned)snap_capacity);
    if (srv_ip == 0 || (sock = udp_bind(AGENT_PORT, rx_handler, NULL)) == NULL)
        return MODULE_NO_RESIDENT_END;
    th.attr = TH_C;
    th.thread = agent_thread;
    th.priority = 0x50;
    th.stacksize = 0x800;
    th.option = 0;
    int tid = CreateThread(&th);
    if (tid < 0 || StartThread(tid, NULL) < 0)
        return MODULE_NO_RESIDENT_END;
    return MODULE_RESIDENT_END;
}
