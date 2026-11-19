"""Apply the ORBIT IGR integration edits to a Neutrino v1.8.0 checkout (cwd = neutrino root)."""
import re
from pathlib import Path


def edit(path, old, new, count=1):
    p = Path(path)
    s = p.read_text()
    assert s.count(old) == count, (path, old[:60], s.count(old))
    p.write_text(s.replace(old, new))


# 1. ee_core <-> loader interface: the IGR settings
edit("common/include/eecore_config.h",
     "    uint32_t mod_checksum_4k[EEC_MOD_CHECKSUM_COUNT];\n} __attribute__((packed, aligned(4)));",
     "    uint32_t mod_checksum_4k[EEC_MOD_CHECKSUM_COUNT];\n\n"
     "    // ORBIT: In Game Reset (0 = off). libpad button masks (PAD_* bits), and the ELF to return to\n"
     "    uint16_t IgrExitCombo;\n    uint16_t IgrOffCombo;\n    char IgrExitPath[64];\n"
     "    uint16_t IgrMenuCombo; // phase 13: opens the in-game menu\n"
     "    // ORBIT phase 16b: achievements telemetry (ra.c), the watch block in module storage; RaMbox NULL = off\n"
     "    uint32_t *RaWatch;\n    uint32_t RaCount;\n    uint32_t RaBytes;\n"
     "    // xeRAbora 2.1 RAN2/RANL chains are normalized in protected module storage.\n"
     "    uint32_t *RaNodeList;\n    uint32_t RaNodeCount;\n    uint32_t *RaNodeWork;\n"
     "    uint8_t *RaMbox;\n"
     "    // NULD phase 19: titles for the unlock notice, EEC_RA_TITLE bytes each (u32 id, title + NUL)\n"
     "    uint8_t *RaTitles;\n    uint32_t RaTitleCount;\n"
     "    // ORBIT phase 18: the in-game menu's language\n"
     "    uint8_t IgrLang; // 0 Spanish, 1 English, 2 Portuguese\n"
     "} __attribute__((packed, aligned(4)));\n\n"
     "#define EEC_RA_TITLE 32 // NULD phase 19: one notice title in the watch list's tail\n"
     "#define EEC_RA_TITLES_MAGIC 0x4C544152 // \"RATL\": the tail's last word, after its count")

# 2. ee_core: build igr.c; hook libpad when the game asks for an IOP reset and right after its ELF is loaded
edit("ee/ee_core/Makefile", "CHEATCORE_EE_OBJS = cheat_engine.o cheat_api.o",
     "CHEATCORE_EE_OBJS = cheat_engine.o cheat_api.o igr.o menu.o ra.o resetspu_irx.o # ORBIT In Game Reset, achievements")
edit("ee/ee_core/src/iopmgr.c", '#include "eecore_config.h"\n', '#include "eecore_config.h"\n#include "igr.h"\n')
edit("ee/ee_core/src/iopmgr.c",
     "    struct _iop_reset_pkt *reset_pkt = (struct _iop_reset_pkt *)sdd->src;\n\n    New_Reset_Iop2(",
     "    struct _iop_reset_pkt *reset_pkt = (struct _iop_reset_pkt *)sdd->src;\n\n"
     "    // ORBIT IGR: the game's code is in memory now, hook its libpad open call (as OPL syshook.c)\n"
     "    if (IGR_Enabled() && padOpen_hooked == 0)\n"
     "        padOpen_hooked = Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_HOOK);\n\n"
     "    // ORBIT ra.c: the agent's IOP buffer dies with this reset; no snapshot DMA until the new agent writes it\n"
     "    if (eec.RaMbox != NULL)\n"
     "        *(volatile u32 *)(0x20000000 | (u32)eec.RaMbox) = 0;\n\n"
     "    New_Reset_Iop2(")
edit("ee/ee_core/src/iopmgr.c",
     "//---------------------------------------------------------------------------\n// Replace SifSetDma, SifSetReg and SifGetReg syscalls in kernel\n",
     "//---------------------------------------------------------------------------\n"
     "// ORBIT IGR: put the original syscalls back before the IGR resets the IOP to the ROM modules\n"
     "void Remove_Kernel_Hooks(void)\n{\n"
     "    SetSyscall(__NR_SifSetDma, Old_SifSetDma);\n"
     "    SetSyscall(__NR_SifSetReg, Old_SifSetReg);\n"
     "    SetSyscall(__NR_SifGetReg, Old_SifGetReg);\n}\n\n"
     "//---------------------------------------------------------------------------\n// Replace SifSetDma, SifSetReg and SifGetReg syscalls in kernel\n")
edit("ee/ee_core/src/main.c", '#include "eecore_config.h"\n', '#include "eecore_config.h"\n#include "igr.h"\n')
edit("ee/ee_core/src/main.c",
     "            apply_patches(argv[0]);\n",
     "            apply_patches(argv[0]);\n"
     "            if (IGR_Enabled()) // ORBIT IGR: hook libpad in the freshly loaded game\n"
     "                padOpen_hooked = Install_PadOpen_Hook(0x00100000, 0x01ff0000, PADOPEN_HOOK);\n")

# 3. loader: -igr=<mask> (exit), -igroff=<mask> (power off), -igrexit=<elf path>
edit("ee/loader/src/main.c",
     '        else if (!strncmp(argv[i], "-qb", 3))\n',
     '        else if (!strncmp(argv[i], "-igr=", 5)) // ORBIT IGR\n'
     '            igr_exit = strtoul(&argv[i][5], NULL, 0);\n'
     '        else if (!strncmp(argv[i], "-igroff=", 8))\n'
     '            igr_off = strtoul(&argv[i][8], NULL, 0);\n'
     '        else if (!strncmp(argv[i], "-igrexit=", 9))\n'
     '            igr_path = &argv[i][9];\n'
     '        else if (!strncmp(argv[i], "-igrmenu=", 9))\n'
     '            igr_menu = strtoul(&argv[i][9], NULL, 0);\n'
     '        else if (!strncmp(argv[i], "-qb", 3))\n')
edit("ee/loader/src/main.c",
     "static int parse_cmdline_args(int argc, char *argv[], int *out_iELFArgcStart)\n",
     "static unsigned igr_exit, igr_off, igr_menu; // ORBIT IGR settings, copied into sys.eecore before ee_core starts\n"
     "static const char *igr_path = \"\";\n\n"
     "static int parse_cmdline_args(int argc, char *argv[], int *out_iELFArgcStart)\n")
edit("ee/loader/src/main.c",
     "    *set_ee_core = sys.eecore;",
     "    sys.eecore.IgrExitCombo = igr_exit; // ORBIT IGR\n"
     "    sys.eecore.IgrOffCombo  = igr_off;\n"
     "    sys.eecore.IgrMenuCombo = igr_menu;\n"
     "    strncpy(sys.eecore.IgrExitPath, igr_path, sizeof(sys.eecore.IgrExitPath) - 1);\n"
     "    *set_ee_core = sys.eecore;")
edit("ee/loader/src/main.c",
     '    printf("  -qb               Quick-Boot directly into load environment\\n");\n',
     '    printf("  -qb               Quick-Boot directly into load environment\\n");\n'
     '    printf("  -igr=<mask>       ORBIT: libpad button mask that returns to -igrexit (0 = off)\\n");\n'
     '    printf("  -igroff=<mask>    ORBIT: libpad button mask that powers the console off\\n");\n'
     '    printf("  -igrexit=<elf>    ORBIT: ELF to run on return, read by the ROM IOP (rom0:OSDSYS = reboot)\\n");\n'
     '    printf("  -igrmenu=<mask>   ORBIT: libpad button mask that opens the in-game menu\\n");\n')
# 4. loader: write the D-cache back before ee_core starts. It copies ee_core and patches the kernel's code through
#    the cache, then ExecPS2s without a flush (ps2sdk's own ELF loaders flush first). On the console some builds of
#    ee_core never started (black screen) while PCSX2, which does not model the caches, ran them all
edit("ee/loader/src/main.c",
     "    ExecPS2((void *)eh->entry, NULL, ee_core_argc, ee_core_argv);",
     "    FlushCache(0); // ORBIT: see patch_neutrino.py\n"
     "    FlushCache(2);\n"
     "    ExecPS2((void *)eh->entry, NULL, ee_core_argc, ee_core_argv);")

# 5. phase 13 (in-game menu): GSM records the game's DISPFB1/2 and PMODE writes, so the menu knows the shown frame.
#    The breakpoint mask widens from PMODE/SMODE2/DISPLAY1/2 to the whole 0x00-0xF0 block (DISPFB1 is 0x70,
#    DISPFB2 0x90); the extra registers pass through. Without -gsm, GSM runs capture-only when the menu is on: no
#    SetGsCrt hook, every value written as is
G = "ee/ee_core/src/gsm_api.c"
edit(G, "    u64 last_display1;\n    u64 last_display2;\n};",
     "    u64 last_display1;\n    u64 last_display2;\n"
     "    u64 last_dispfb1; // ORBIT: the shown frame, for the in-game menu\n"
     "    u64 last_dispfb2;\n"
     "    u64 last_pmode;\n"
     "    u64 last_smode2;\n};")
edit(G, "            pstate->last_display1 = value;\n            *dest = mod_DISPLAY(pstate, value);",
     "            pstate->last_display1 = value;\n"
     "            *dest = pstate->GsmVideoMode == EECORE_GSM_VMODE_NONE ? value : mod_DISPLAY(pstate, value);")
edit(G, "            pstate->last_display2 = value;\n            *dest = mod_DISPLAY(pstate, value);",
     "            pstate->last_display2 = value;\n"
     "            *dest = pstate->GsmVideoMode == EECORE_GSM_VMODE_NONE ? value : mod_DISPLAY(pstate, value);")
edit(G, "        case (u32)GS_REG_SMODE2:\n            // Store game requested mode\n",
     "        case (u32)GS_REG_SMODE2:\n"
     "            pstate->last_smode2 = value; // ORBIT\n"
     "            if (pstate->GsmVideoMode == EECORE_GSM_VMODE_NONE) { // ORBIT: capture only\n"
     "                *dest = value;\n"
     "                break;\n"
     "            }\n"
     "            // Store game requested mode\n")
edit(G, "        case (u32)GS_REG_PMODE:\n        case (u32)GS_REG_SIGLBLID:\n            *dest = value;\n"
        "            break;\n        default:\n            BGERROR(COLOR_FUNC_GSM, 4);",
     "        case (u32)GS_REG_PMODE:\n"
     "            pstate->last_pmode = value; // ORBIT\n"
     "            *dest = value;\n"
     "            break;\n"
     "        case (u32)GS_REG_DISPFB1: // ORBIT: the shown frame, for the in-game menu\n"
     "            pstate->last_dispfb1 = value;\n"
     "            *dest = value;\n"
     "            break;\n"
     "        case (u32)GS_REG_DISPFB2:\n"
     "            pstate->last_dispfb2 = value;\n"
     "            *dest = value;\n"
     "            break;\n"
     "        case (u32)GS_REG_SIGLBLID:\n            *dest = value;\n"
     "            break;\n        default:\n"
     "            if (((u32)dest & 0x1fffef0f) == 0x12000000) { // ORBIT: the rest of the wider mask passes through\n"
     "                *dest = value;\n"
     "                break;\n"
     "            }\n"
     "            BGERROR(COLOR_FUNC_GSM, 4);")
edit(G, "        default:\n            BGERROR(COLOR_FUNC_GSM, 3);",
     "        default:\n"
     "            if (((u32)source & 0x1fffef0f) == 0x12000000) { // ORBIT: wider mask, pass through\n"
     "                regs->gpr[rt] = *source;\n"
     "                break;\n"
     "            }\n"
     "            BGERROR(COLOR_FUNC_GSM, 3);")
edit(G, "    // Hook SetGsCrt\n    pstate->org_SetGsCrt = GetSyscallHandler(__NR_SetGsCrt);\n"
        "    SetSyscall(__NR_SetGsCrt, (void *)(((u32)(hook_SetGsCrt) & ~0xE0000000) | 0x80000000));\n",
     "    // Hook SetGsCrt (ORBIT: not in capture-only mode)\n"
     "    if (pstate->GsmVideoMode != EECORE_GSM_VMODE_NONE) {\n"
     "        pstate->org_SetGsCrt = GetSyscallHandler(__NR_SetGsCrt);\n"
     "        SetSyscall(__NR_SetGsCrt, (void *)(((u32)(hook_SetGsCrt) & ~0xE0000000) | 0x80000000));\n"
     "    }\n")
edit(G, "_ee_mtdabm(0x1fffef5f);", "_ee_mtdabm(0x1fffef0f); // ORBIT: was 0x1fffef5f")
edit(G, "        _ee_mtdabm(0x1fffff5f);\n    }\n}\n",
     "        _ee_mtdabm(0x1fffff0f); // ORBIT: was 0x1fffff5f\n    }\n"
     "    if (pstate->GsmVideoMode == EECORE_GSM_VMODE_NONE) // ORBIT: no SetGsCrt hook arms it later\n"
     "        _ee_enable_bpc(EE_BPC_DWE | EE_BPC_DUE | EE_BPC_DKE);\n}\n")
edit(G, "    SetSyscall(__NR_SetGsCrt, pstate->org_SetGsCrt);\n}\n",
     "    if (pstate->org_SetGsCrt != NULL) // ORBIT: not hooked in capture-only mode\n"
     "        SetSyscall(__NR_SetGsCrt, pstate->org_SetGsCrt);\n}\n\n"
     "// ORBIT: the game's latest writes (0 = not seen yet): DISPFB1, DISPFB2, PMODE, DISPLAY1, DISPLAY2, SMODE2\n"
     "void GSM_GetDisplay(u64 out[6])\n{\n"
     "    out[0] = state.last_dispfb1;\n"
     "    out[1] = state.last_dispfb2;\n"
     "    out[2] = state.last_pmode;\n"
     "    out[3] = state.last_display1;\n"
     "    out[4] = state.last_display2;\n"
     "    out[5] = state.last_smode2;\n}\n")
edit("ee/ee_core/src/main.c",
     "if ((eec.GsmVideoMode != EECORE_GSM_VMODE_NONE) && ((eec.flags & EECORE_FLAG_UNHOOK) == 0)) {",
     "if ((eec.GsmVideoMode != EECORE_GSM_VMODE_NONE || eec.IgrMenuCombo) && ((eec.flags & EECORE_FLAG_UNHOOK) == 0)) {")

# 6. phase 16b, achievements: -ra=<watch list file>. The list goes after the modules and cheats (it never changes);
#    the 64-byte mailbox and the snapshot buffer, which change every frame, go past the 32 x 4 KB module checksum
#    that ee_core verifies on every IOP reset. The agent (raagent.irx, loaded by the launcher's -cfg=ra) gets the
#    mailbox address as "mbox=<hex>": a placeholder appended before the modules are installed, filled in after
L = "ee/loader/src/main.c"
edit(L, '        else if (!strncmp(argv[i], "-igrmenu=", 9))\n',
     '        else if (!strncmp(argv[i], "-ra=", 4)) // ORBIT phase 16b\n'
     '            ra_path = &argv[i][4];\n'
     '        else if (!strncmp(argv[i], "-igrlang=", 9)) // ORBIT phase 18\n'
     '            igr_lang = !strcmp(&argv[i][9], "en") ? 1 : !strcmp(&argv[i][9], "pt") ? 2 : 0;\n'
     '        else if (!strncmp(argv[i], "-igrmenu=", 9))\n')
edit(L, 'static const char *igr_path = "";\n',
     'static const char *igr_path = "";\n'
     'static const char *ra_path; // ORBIT phase 16b: watch list file\n'
     'static int igr_lang;        // ORBIT phase 18: in-game menu in English (1) or Portuguese (2)\n')
edit(L, '    printf("  -igrmenu=<mask>   ORBIT: libpad button mask that opens the in-game menu\\n");\n',
     '    printf("  -igrmenu=<mask>   ORBIT: libpad button mask that opens the in-game menu\\n");\n'
     '    printf("  -ra=<file>        ORBIT: RetroAchievements watch list for raagent.irx (phase 16b)\\n");\n'
     '    printf("  -igrlang=<es|en|pt>  ORBIT: language of the in-game menu (phase 18)\\n");\n')
edit(L, "static int parse_cmdline_args(int argc, char *argv[], int *out_iELFArgcStart)\n", r'''#include "../../../common/include/ra_wl_parse.h"
// ORBIT phase 17: the watch list file is read while the boot environment's devices are still there (before the IOP
// reboot into the load environment: without -qb, a nuld game, mass0: is gone by ra_place). Heap, not BSS: a 16 KB
// static array in this loader broke every launch (phase 16b)
static uint8_t *ra_file;
static int ra_file_len;
static void ra_read(const char *path)
{
    int fd = open(path, O_RDONLY), n = fd < 0 ? -1 : lseek(fd, 0, SEEK_END);
    if (n > 0 && n <= NULD_RA_FILE_MAX && (ra_file = malloc(n)) != NULL) {
        lseek(fd, 0, SEEK_SET);
        ra_file_len = read(fd, ra_file, n);
    }
    if (fd >= 0)
        close(fd);
    printf("ORBIT ra: %s read, %d bytes\n", path, ra_file_len);
}

// ORBIT phase 16b: place the watch list at end, the mailbox and snapshot buffer past the module checksum, and write
// the mailbox address into the agent's "mbox=00000000" argument. Returns the new end of module storage.
static uint8_t *ra_place(const char *path, irxtab_t *tab, uint8_t *end)
{
    struct nuld_ra_list_info parsed;
    struct ra_watch_file *h = (struct ra_watch_file *)ra_file;
    uint32_t i, j, *list, *work = NULL;
    struct ra_node *nodes = NULL;
    uint8_t *cursor, *mbox, *titles;
    uint32_t limit;
    int k, d;
    if (!ra_file || !nuld_ra_parse_list(ra_file, ra_file_len, 1, &parsed)) {
        printf("ORBIT ra: %s is not a complete watch list: no telemetry\n", path);
        return end;
    }
    // All checks above happen BEFORE loading game memory. The old direct-only
    // loader kept a possibly malformed tail and could truncate achievements.
    list = (uint32_t *)(((uint32_t)end + 15) & ~15u);
    cursor = (uint8_t *)(list + parsed.count);
    if (parsed.node_count) {
        nodes = (struct ra_node *)(((uint32_t)cursor + 15) & ~15u);
        work = (uint32_t *)(nodes + parsed.node_count);
        cursor = (uint8_t *)(work + parsed.node_count * 2u);
    }
    mbox = (uint8_t *)sys.eecore.ModStorageStart + EEC_MOD_CHECKSUM_COUNT * 4096;
    if (cursor > mbox)
        mbox = (uint8_t *)(((uint32_t)cursor + 63) & ~63u);
    titles = mbox + 64 + RA_SNAP_TOTAL_FOR(parsed.snapshot_bytes);
    // EE module storage is below the 32 MiB end of physical RAM. Never
    // overflow it with the 256-node working state, snapshot or RATL titles.
    limit = (uint32_t)titles + parsed.title_count * EEC_RA_TITLE;
    if (limit < (uint32_t)mbox || limit > 0x02000000u) {
        printf("ORBIT ra: no EE storage for %u values + %u chains: no telemetry\n",
               parsed.bytes, parsed.node_count);
        return end;
    }
    memcpy(list, ra_file + sizeof(*h), 4 * parsed.count);
    if (parsed.node_count) {
        unsigned stride = parsed.node_magic == RA_NODE_MAGIC ? sizeof(struct ra_node) : 8;
        for (i = 0; i < parsed.node_count; i++) {
            struct ra_node node;
            const uint8_t *at = ra_file + parsed.node_offset + i * stride;
            node.scale = 1;
            memcpy(&node, at, stride);
            if (parsed.node_magic == RA_NODE_LEGACY_MAGIC)
                node.w |= RA_NODE_LEGACY; // old RANL rejects a null base
            nodes[i] = node;
            work[i * 2] = 0; // resolved value, written in VBLANK
            work[i * 2 + 1] = 0; // precomputed direct-parent offset/size
            if (!RA_NODE_FROM_NODE(node.w)) {
                unsigned parent = RA_NODE_PARENT(node.w);
                uint32_t e = list[parent], addr = RA_WATCH_ADDR(e);
                unsigned z = RA_WATCH_SIZE(e), off = 0;
                if ((addr >= 0x00080000u && addr < 0x02000000u && z <= 0x02000000u - addr) ||
                    (addr >= 0x02000000u && addr < 0x02004000u && z <= 0x02004000u - addr)) {
                    for (j = 0; j < parent; j++)
                        off += RA_WATCH_SIZE(list[j]);
                    work[i * 2 + 1] = (off << 4) | z;
                }
            }
        }
    }
    memset(mbox, 0, 64 + RA_SNAP_TOTAL_FOR(parsed.snapshot_bytes));
    if (parsed.title_count)
        memcpy(titles, ra_file + parsed.title_offset,
               parsed.title_count * EEC_RA_TITLE);
    // Tell the agent where to DMA, how large a complete snapshot is, and the
    // image hash, if one was supplied in the RAH1 trailer.
    for (i = 0; i < (uint32_t)tab->count; i++) {
        char *arg = (char *)tab->modules[i].args;
        for (k = 0; arg && k < (int)tab->modules[i].arg_len; k++) {
            if (k + 13 <= (int)tab->modules[i].arg_len &&
                !memcmp(arg + k, "mbox=00000000", 13))
                for (d = 0; d < 8; d++)
                    arg[k + 5 + d] = "0123456789abcdef"[((uint32_t)mbox >> (28 - 4 * d)) & 15];
            if (k + 10 <= (int)tab->modules[i].arg_len &&
                !memcmp(arg + k, "size=00000", 10)) {
                unsigned value = parsed.snapshot_bytes;
                for (d = 4; d >= 0; d--) {
                    arg[k + 5 + d] = '0' + value % 10;
                    value /= 10;
                }
            }
            if (parsed.hash_offset && k + 37 <= (int)tab->modules[i].arg_len &&
                !memcmp(arg + k, "hash=................................", 37))
                memcpy(arg + k + 5, parsed.image_hash, RA_HASH_LEN);
        }
    }
    sys.eecore.RaWatch = list;
    sys.eecore.RaCount = parsed.count;
    sys.eecore.RaBytes = parsed.bytes; // direct bytes; nodes add 8 each
    sys.eecore.RaNodeList = (uint32_t *)nodes;
    sys.eecore.RaNodeCount = parsed.node_count;
    sys.eecore.RaNodeWork = work;
    sys.eecore.RaMbox = mbox;
    sys.eecore.RaTitles = titles;
    sys.eecore.RaTitleCount = parsed.title_count;
    printf("ORBIT ra: %u entries, %u chains, %u snapshot bytes, %u titles\n",
           parsed.count, parsed.node_count, parsed.snapshot_bytes, parsed.title_count);
    return titles + parsed.title_count * EEC_RA_TITLE;
}

static int parse_cmdline_args(int argc, char *argv[], int *out_iELFArgcStart)
''')
edit(L, "    uint8_t *irxptr_end = build_irx_table(sDVDFile != NULL);\n",
     "    if (ra_path != NULL) // ORBIT phase 16b: room for the mailbox address in the agent's arguments\n"
     "        for (i = 0; i < drv.mod.count; i++)\n"
     "            if (drv.mod.mod[i].args != NULL && !strcmp(drv.mod.mod[i].sFileName, \"raagent.irx\") &&\n"
     "                drv.mod.mod[i].arg_len + 63 <= 256) {\n"
     "                char *a = drv.mod.mod[i].args + drv.mod.mod[i].arg_len;\n"
     "                strcpy(a, \"mbox=00000000\"); a += 14;\n"
     "                strcpy(a, \"size=00000\"); a += 11;\n"
     "                strcpy(a, \"hash=................................\");\n"
     "                drv.mod.mod[i].arg_len += 63;\n"
     "            }\n"
     "    uint8_t *irxptr_end = build_irx_table(sDVDFile != NULL);\n")
edit(L, "    mod_ee_core.sFileName = sys.eecore_elf;\n",
     "    if (ra_path != NULL) // ORBIT phase 17: before the IOP reboot, while mass0: is still mounted\n"
     "        ra_read(ra_path);\n"
     "    mod_ee_core.sFileName = sys.eecore_elf;\n")
edit(L, "    // Add simple checksum over the module data\n",
     "    if (ra_path != NULL) // ORBIT phase 16b\n"
     "        irxptr_end = ra_place(ra_path, irxtable, irxptr_end);\n"
     "    sys.eecore.IgrLang = igr_lang; // ORBIT phase 18 (the menu's save buffer stays in ee_core: #12)\n\n"
     "    // Add simple checksum over the module data\n")

# 6. udpfs: a lost link is found again. udprdma gives up after 4 x 500 ms without an ACK and stays disconnected for
#    good, so every later read of the game fails (SMG port: "slice: bad blob" at the next scene, 2026-11-09). Now a
#    read (and a write) first rediscovers the server, sequence numbers from 0 (udpfs_server.py resets on seq 0 and
#    keeps its handles), and a failed block read is asked again over the new link.
U = "iop/udpfs/src/"
edit(U + "udprdma.c",
     "int udprdma_is_connected(udprdma_socket_t *socket)\n",
     "/* NULD: back to a new socket's state (as udprdma_create), for a discovery after a lost link */\n"
     "void udprdma_reset(udprdma_socket_t *socket)\n"
     "{\n"
     "    if (socket == NULL) return;\n"
     "    socket->state = STATE_DISCONNECTED;\n"
     "    socket->tx_seq_nr = 0;\n"
     "    socket->tx_seq_nr_acked = 0;\n"
     "    socket->rx_seq_nr_expected = 0;\n"
     "    socket->rx_window_count = 0;\n"
     "    socket->rx_nack_sent = 0;\n"
     "    socket->rx_buffer = NULL;\n"
     "    socket->rx_hdr_buffer = NULL;\n"
     "}\n\n"
     "int udprdma_is_connected(udprdma_socket_t *socket)\n")
edit(U + "udprdma.h",
     "int udprdma_is_connected(udprdma_socket_t *socket);\n",
     "int udprdma_is_connected(udprdma_socket_t *socket);\n\n"
     "/* NULD: back to a new socket's state, for a discovery after a lost link */\n"
     "void udprdma_reset(udprdma_socket_t *socket);\n")
edit(U + "udpfs_core.c",
     "/*\n * Core UDPFS initialization (socket + discovery)\n */\n",
     "/*\n * NULD: the server found again after a lost link (as at init), for up to a minute\n */\n"
     "static int _reconnect(void)\n"
     "{\n"
     "    int i;\n\n"
     "    if (g_socket == NULL)\n"
     "        return -1;\n"
     "    for (i = 0; i < 30; i++) {\n"
     "        udprdma_reset(g_socket);\n"
     "        if (udprdma_discover(g_socket, 2000) == UDPRDMA_OK) {\n"
     "            M_PRINTF(\"UDPFS: reconnected\\n\");\n"
     "            return 0;\n"
     "        }\n"
     "    }\n"
     "    M_PRINTF(\"UDPFS: server lost\\n\");\n"
     "    return -1;\n"
     "}\n\n"
     "/*\n * Core UDPFS initialization (socket + discovery)\n */\n")
edit(U + "udpfs_core.c",
     "    if (!udprdma_is_connected(g_socket))\n        return -EIO;\n\n    if (handle < 0)\n        return -EBADF;\n\n"
     "    while (sectors_left > 0) {\n",
     "    if (!udprdma_is_connected(g_socket) && _reconnect() != 0) // NULD\n        return -EIO;\n\n"
     "    if (handle < 0)\n        return -EBADF;\n\n"
     "    while (sectors_left > 0) {\n")
edit(U + "udpfs_core.c",
     "        ret = _recv_with_result(g_socket, &req, sizeof(req), buffer, chunk * sector_size, 30000);\n",
     "        ret = _recv_with_result(g_socket, &req, sizeof(req), buffer, chunk * sector_size, 30000);\n"
     "        for (int tries = 0; ret < 0 && tries < 3 && _reconnect() == 0; tries++) // NULD: over the new link\n"
     "            ret = _recv_with_result(g_socket, &req, sizeof(req), buffer, chunk * sector_size, 30000);\n")
edit(U + "udpfs_core.c",
     "    if (!udprdma_is_connected(g_socket))\n        return -EIO;\n\n    if (handle < 0)\n        return -EBADF;\n\n"
     "    /* Build BWRITE_REQ */\n",
     "    if (!udprdma_is_connected(g_socket) && _reconnect() != 0) // NULD\n        return -EIO;\n\n"
     "    if (handle < 0)\n        return -EBADF;\n\n"
     "    /* Build BWRITE_REQ */\n")
# (fhi_read / fhi_write: no early return when disconnected, the core reconnects)
edit(U + "udpfs_fhi.c", "    if (!udpfs_core_is_connected())\n        return 0;\n\n", "", count=2)
print("patched")
