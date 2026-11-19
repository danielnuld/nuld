/* Hosted regression test for the same binary parser used by NULD and its
   patched Neutrino loader. Run with: make test-ra-protocol. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../third_party/neutrino-igr/raagent/ra_wl_parse.h"
static unsigned char b[NULD_RA_FILE_MAX + 16];
static const char image[] = "0123456789abcdef0123456789abcdef";
static void word(unsigned p, unsigned v) { memcpy(b + p, &v, 4); }
static unsigned start(unsigned n, unsigned bytes)
{
    memset(b, 0, sizeof(b));
    word(0, RA_WATCH_MAGIC); word(4, RA_WATCH_VERSION); word(8, n); word(12, bytes);
    for (unsigned i = 0; i < n; i++) word(16 + i * 4, RA_WATCH_PACK(0x80000 + i, 1));
    return 16 + 4 * n;
}
static unsigned node(unsigned at, unsigned magic, unsigned count)
{
    word(at, magic); word(at + 4, count);
    unsigned stride = magic == RA_NODE_MAGIC ? sizeof(struct ra_node) : 8;
    for (unsigned i = 0; i < count; i++) {
        word(at + 8 + i * stride, RA_NODE_PACK(0, 0, 1, 0, 0));
        word(at + 12 + i * stride, 0);
        if (stride == 12) word(at + 16 + i * stride, 1);
    }
    return at + 8 + count * stride;
}
static unsigned hash(unsigned at)
{
    word(at, RA_HASH_MAGIC); memcpy(b + at + 4, image, 32);
    return at + sizeof(struct ra_hash_file);
}
static int valid(unsigned n, int titles, struct nuld_ra_list_info *info)
{
    return nuld_ra_parse_list(b, (int)n, titles, info);
}
int main(void)
{
    struct nuld_ra_list_info m;
    unsigned p = start(1, 1);
    assert(valid(p,0,&m) && m.count==1 && m.node_count==0 && !m.hash_offset);
    assert(!valid(p-1,0,&m)); assert(!valid(p+1,0,&m));
    word(12,2); assert(!valid(p,0,&m)); word(12,1);
    word(16,RA_WATCH_PACK(0x80000,3)); assert(!valid(p,0,&m));
    word(16,RA_WATCH_PACK(0x80000,1));
    word(0,0); assert(!valid(p,0,&m)); word(0,RA_WATCH_MAGIC);
    word(4,2); assert(!valid(p,0,&m)); word(4,RA_WATCH_VERSION);

    p=hash(p);
    assert(valid(p,0,&m) && m.hash_offset == 20 && !strcmp(m.image_hash,image));
    b[p-1]='z'; assert(!valid(p,0,&m)); b[p-1]='f';
    assert(!valid(p-1,0,&m) && !valid(p+1,0,&m));
    word(20,0x12345678);assert(!valid(p,0,&m));word(20,RA_HASH_MAGIC);

    p=start(1,1);p=node(p,RA_NODE_MAGIC,2);
    /* Node 1 can reference node 0: depth-one dependency. */
    word(20+8+12,RA_NODE_PACK(0,1,4,0,0));
    assert(valid(p,0,&m) && m.node_count==2 && m.snapshot_bytes==17);
    p=hash(p);
    assert(valid(p,0,&m) && m.node_count==2 && !strcmp(m.image_hash,image));
    word(20+8+12,RA_NODE_PACK(1,1,4,0,0));assert(!valid(p,0,&m)); // forward/self ref
    word(20+8+12,RA_NODE_PACK(0,1,4,0,0));
    word(20+8,RA_NODE_PACK(0,0,1,0,33));assert(!valid(p,0,&m)); // width > 32
    word(20+8,RA_NODE_PACK(0,0,1,0,0));
    word(24,RA_NODE_MAX+1);assert(!valid(p,0,&m));word(24,2);

    p=start(1,1);p=node(p,RA_NODE_LEGACY_MAGIC,1);
    assert(valid(p,0,&m) && m.node_magic==RA_NODE_LEGACY_MAGIC);
    p=hash(p);assert(valid(p,0,&m));
    /* RATL is NULD's own final title footer, outside xeRAbora wire data. */
    unsigned title_at=p;
    memset(b+title_at,0,NULD_RA_TITLE_BYTES);
    word(title_at,100);memcpy(b+title_at+4,"TEST",4);
    p+=NULD_RA_TITLE_BYTES;
    word(p,1);word(p+4,NULD_RA_TITLES_MAGIC);p+=8;
    assert(!valid(p,0,&m));
    assert(valid(p,1,&m) && m.title_count==1 && m.title_offset==title_at);
    word(p-8,129);assert(!valid(p,1,&m));word(p-8,1);
    word(p-4,0);assert(!valid(p,1,&m));word(p-4,NULD_RA_TITLES_MAGIC);

    p=start(RA_WATCH_MAX,RA_WATCH_MAX);
    p=node(p,RA_NODE_MAGIC,RA_NODE_MAX);
    assert(valid(p,0,&m) && m.count==RA_WATCH_MAX && m.node_count==256);
    p=hash(p);
    assert(valid(p,0,&m) && !strcmp(m.image_hash,image) && p<=NULD_RA_WIRE_MAX);
    assert(!valid(p-1,0,&m));
    word(16+4*RA_WATCH_MAX+4,257);assert(!valid(p,0,&m));
    puts("PASS: NULD xeRAbora 2.1 RAN2/RAH1, 256 nodes, RANL, RATL, and corrupted suffixes");
    return 0;
}
