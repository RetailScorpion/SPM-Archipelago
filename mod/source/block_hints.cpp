#include <common.h>
#include <msl/string.h>
#include <spm/icondrv.h>
#include <spm/item_data.h>
#include <spm/spmario.h>
#include <spm/swdrv.h>
#include <wii/os.h>

#include "block_hints.h"
#include "game_addresses.h"
#include "mailbox.h"

namespace mod::block_hints {

#define HINT_ICON_NAME "ap_block_hint"
#define ITEM_COIN 1
#define BLOCK_COUNT 3

#define HINT_HEIGHT 45.0f  // above the block's position (its bottom; blocks are 25 tall)
#define BOB_HEIGHT 4.0f    // the coin bobs up and down by this much
#define BOB_PERIOD 80      // frames for one bob (up and back down)

// relF's layout (relF.rel), for the gn1_03 code's data
#define RELF_SECTION_INFO 0x10     // module header: the section table (an address once loaded)
#define RELF_BSS_SECTION 6
#define CODE_HISTORY 0x250         // bss: the last hits, a ring buffer
#define CODE_WRITE_POS 0x2B4       // bss: where the next hit goes
#define CODE_PATTERN 0xB4BA8       // section 5: the code
#define CODE_LENGTH 25

struct Room
{
    const char * map;
    f32 x[BLOCK_COUNT], y, z;  // the blocks, left to right (the init script's positions)
    s32 solved;                // Chapter 5's value once the pipe is up
};

static const Room sRooms[] = {
    {"gn1_02", {-1112.5f, -1062.5f, -1012.5f}, 225.0f, -12.5f, 183},
    {"gn1_03", {937.5f, 987.5f, 1037.5f}, 75.0f, 0.0f, 185},
};
#define ROOM_GN1_02 0
#define ROOM_GN1_03 1

static s32 sRoom = -1;  // the room being played, or -1
static s32 sFrame;

// gn1_02: the hit scripts' progress (LSW(1)) -> the block to hit next: left, right, middle
static s32 nextBlockGn1_02()
{
    static const s32 order[] = {0, 2, 1};
    s32 progress = spm::swdrv::_swByteGet(1);
    return progress >= 0 && progress < BLOCK_COUNT ? order[progress] : 0;
}

// gn1_03: the code's entry after the longest run of recent hits that matches its start
static s32 nextBlockGn1_03()
{
    u8 * section5 = reinterpret_cast<u8 *>(game_addresses::relF_section5);
    // relF's start, from the mailbox (main.cpp works it out at startup)
    u32 * sections = *reinterpret_cast<u32 **>(mailbox::gMailbox.relfBase + RELF_SECTION_INFO);
    u8 * bss = reinterpret_cast<u8 *>(sections[RELF_BSS_SECTION * 2] & ~1u);
    const s32 * history = reinterpret_cast<const s32 *>(bss + CODE_HISTORY);
    s32 writePos = *reinterpret_cast<const s32 *>(bss + CODE_WRITE_POS);
    const s32 * code = reinterpret_cast<const s32 *>(section5 + CODE_PATTERN);
    if (writePos < 0 || writePos >= CODE_LENGTH)
        return code[0];
    // Longest k: the last k hits (oldest first) equal code[0..k-1]
    for (s32 k = CODE_LENGTH - 1; k > 0; k--)
    {
        bool match = true;
        for (s32 i = 0; i < k && match; i++)
        {
            s32 hit = history[(writePos - k + i + CODE_LENGTH * 2) % CODE_LENGTH];
            match = hit == code[i];
        }
        if (match)
            return code[k];
    }
    return code[0];
}

static void removeHint()
{
    if (spm::icondrv::iconNameToPtr(HINT_ICON_NAME) != nullptr)
        spm::icondrv::iconDelete(HINT_ICON_NAME);
}

void onMapChange(const char * nextMap)
{
    sRoom = -1;
    for (s32 i = 0; i < static_cast<s32>(sizeof(sRooms) / sizeof(sRooms[0])); i++)
    {
        if (nextMap != nullptr && msl::string::strcmp(nextMap, sRooms[i].map) == 0)
            sRoom = i;
    }
}

void tick()
{
    if (sRoom < 0 || spm::spmario::gp->gsw0 >= sRooms[sRoom].solved)
    {
        removeHint();
        return;
    }
    const Room & room = sRooms[sRoom];
    s32 block = sRoom == ROOM_GN1_02 ? nextBlockGn1_02() : nextBlockGn1_03();
    if (block < 0 || block >= BLOCK_COUNT)
    {
        removeHint();
        return;
    }
    spm::icondrv::IconEntry * icon = spm::icondrv::iconNameToPtr(HINT_ICON_NAME);
    if (icon == nullptr)
    {
        spm::icondrv::iconEntry(HINT_ICON_NAME, spm::item_data::itemDataTable[ITEM_COIN].iconId);
        icon = spm::icondrv::iconNameToPtr(HINT_ICON_NAME);
        wii::os::OSReport("[spmap] block hints: coin over the next block in %s\n", room.map);
        if (icon == nullptr)
            return;
    }
    // Bob up and down: a triangle wave, -1 to 1 and back
    sFrame = (sFrame + 1) % BOB_PERIOD;
    s32 half = BOB_PERIOD / 2;
    f32 phase = static_cast<f32>(sFrame < half ? sFrame : BOB_PERIOD - sFrame) / half * 2.0f - 1.0f;
    icon->position.x = room.x[block];
    icon->position.y = room.y + HINT_HEIGHT + BOB_HEIGHT * phase;
    icon->position.z = room.z;
}

}
