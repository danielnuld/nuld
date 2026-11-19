/* NULD watch-list parser shared by the launcher and Neutrino loader.
   xeRAbora protocol records: RAWL + direct entries [+ RANL/RAN2] [+ RAH1].
   Only the NULD launcher appends RATL unlock-title entries after that.
   Protocol structures originally by hacan359 (xeRAbora, MIT).
*/
#ifndef NULD_RA_WL_PARSE_H
#define NULD_RA_WL_PARSE_H
#include <stdint.h>
#include <string.h>
#include "ra_snap.h"

#define NULD_RA_TITLES_MAGIC 0x4C544152u /* RATL */
#define NULD_RA_TITLE_BYTES 32u
#define NULD_RA_TITLES_MAX 128u
#define NULD_RA_WIRE_MAX (sizeof(struct ra_watch_file) + 4u * RA_WATCH_MAX + \
                          sizeof(struct ra_node_file) + sizeof(struct ra_node) * RA_NODE_MAX + \
                          sizeof(struct ra_hash_file))
#define NULD_RA_FILE_MAX (NULD_RA_WIRE_MAX + NULD_RA_TITLES_MAX * NULD_RA_TITLE_BYTES + 8u)

struct nuld_ra_list_info {
    unsigned count, bytes, node_count, snapshot_bytes;
    unsigned node_offset, node_magic;
    unsigned hash_offset, title_offset, title_count;
    char image_hash[RA_HASH_LEN + 1];
};

/* Do not assume that a byte array received over UDP is word-aligned. */
static inline uint32_t nuld_ra_word(const unsigned char *p)
{
    uint32_t w;
    memcpy(&w, p, sizeof(w));
    return w;
}

/* Return 1 only when the complete file fits the negotiated format and
   every node is safe to walk inside an interrupt handler. A direct-only
   prefix is never enough to certify a partially malformed set. */
static inline int nuld_ra_parse_list(const unsigned char *p, int n, int with_titles,
                                     struct nuld_ra_list_info *info)
{
    struct ra_watch_file hdr;
    unsigned direct_end, end, pos, i, sum = 0;
    if (!p || !info || n < (int)sizeof(hdr) || (unsigned)n > NULD_RA_FILE_MAX)
        return 0;
    memset(info, 0, sizeof(*info));
    memcpy(&hdr, p, sizeof(hdr));
    if (hdr.magic != RA_WATCH_MAGIC || hdr.version != RA_WATCH_VERSION ||
        hdr.count == 0 || hdr.count > RA_WATCH_MAX ||
        hdr.bytes == 0 || hdr.bytes > RA_SNAP_MAX_BYTES)
        return 0;
    direct_end = sizeof(hdr) + 4u * hdr.count;
    if ((unsigned)n < direct_end)
        return 0;
    for (i = 0; i < hdr.count; i++) {
        unsigned z = RA_WATCH_SIZE(nuld_ra_word(p + sizeof(hdr) + i * 4u));
        if (z != 1 && z != 2 && z != 4)
            return 0;
        sum += z;
    }
    if (sum != hdr.bytes)
        return 0;
    end = (unsigned)n;
    if (with_titles && end >= direct_end + 8u &&
        nuld_ra_word(p + end - 4u) == NULD_RA_TITLES_MAGIC) {
        unsigned titles = nuld_ra_word(p + end - 8u);
        if (titles > NULD_RA_TITLES_MAX ||
            titles * NULD_RA_TITLE_BYTES > end - direct_end - 8u)
            return 0;
        info->title_count = titles;
        end -= 8u + titles * NULD_RA_TITLE_BYTES;
        info->title_offset = end;
    }
    pos = direct_end;
    if (pos < end && end - pos != sizeof(struct ra_hash_file)) {
        unsigned magic, count, stride;
        if (end - pos < sizeof(struct ra_node_file))
            return 0;
        magic = nuld_ra_word(p + pos);
        count = nuld_ra_word(p + pos + 4u);
        if ((magic != RA_NODE_MAGIC && magic != RA_NODE_LEGACY_MAGIC) ||
            count == 0 || count > RA_NODE_MAX)
            return 0;
        stride = magic == RA_NODE_MAGIC ? sizeof(struct ra_node) : 8u;
        if (count * stride > end - pos - sizeof(struct ra_node_file) ||
            hdr.bytes + count * RA_NODE_PAIR_BYTES > RA_SNAP_MAX_BYTES)
            return 0;
        info->node_count = count;
        info->node_magic = magic;
        info->node_offset = pos + sizeof(struct ra_node_file);
        for (i = 0; i < count; i++) {
            unsigned w = nuld_ra_word(p + info->node_offset + i * stride);
            unsigned parent = RA_NODE_PARENT(w);
            unsigned z = RA_NODE_SIZE(w);
            if ((w & ~(magic == RA_NODE_MAGIC ? 0x07FFFFFFu : 0x0000FFFFu)) ||
                RA_NODE_WIDTH(w) > 32 || (z != 1 && z != 2 && z != 4) ||
                parent >= (RA_NODE_FROM_NODE(w) ? i : hdr.count))
                return 0;
        }
        pos += sizeof(struct ra_node_file) + count * stride;
    }
    if (pos < end) {
        if (end - pos != sizeof(struct ra_hash_file) ||
            nuld_ra_word(p + pos) != RA_HASH_MAGIC)
            return 0;
        for (i = 0; i < RA_HASH_LEN; i++) {
            unsigned char c = p[pos + 4u + i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F')))
                return 0;
            info->image_hash[i] = c;
        }
        info->image_hash[RA_HASH_LEN] = 0;
        info->hash_offset = pos;
        pos += sizeof(struct ra_hash_file);
    }
    if (pos != end)
        return 0;
    info->count = hdr.count;
    info->bytes = hdr.bytes;
    info->snapshot_bytes = hdr.bytes + info->node_count * RA_NODE_PAIR_BYTES;
    return 1;
}
#endif /* NULD_RA_WL_PARSE_H */
