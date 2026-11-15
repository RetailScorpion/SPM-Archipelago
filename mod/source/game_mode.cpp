#include <common.h>
#include <evt_cmd.h>
#include <msl/string.h>
#include <spm/evt_msg.h>
#include <spm/evt_seq.h>
#include <spm/evtmgr.h>
#include <spm/evtmgr_cmd.h>
#include <spm/spmario.h>
#include <wii/os.h>

#include "code_patching.h"
#include "game_mode.h"
#include "options_menu.h"
#include "save_data.h"

namespace mod::game_mode {

// One randomizer write: the word there before, the randomizer's, and whether it's an instruction
// (the processor's caches need refreshing after writing it)
struct Write
{
    u32 * at;
    u32 original;
    u32 patched;
    bool code;
};

// Enough for every write the modules make at startup (about 300) and the few they change later
#define MAX_WRITES 1024
static Write sWrites[MAX_WRITES];
static s32 sWriteCount;

// Whether the game's own words are in memory now (a vanilla file's map was the last to load)
static bool sVanillaApplied;

bool vanilla()
{
    return save_data::getGswByte(AP_GSW_MODE) == FILE_MODE_VANILLA;
}

static u8 modeOf(const spm::nandmgr::SaveFile * save)
{
    return static_cast<u8>(save->spmarioGlobals.gsw[AP_GSW_MODE]);
}

bool fileIsVanilla(const spm::nandmgr::SaveFile * save)
{
    return modeOf(save) == FILE_MODE_VANILLA;
}

bool fileIsArchipelago(const spm::nandmgr::SaveFile * save)
{
    // Files from before the question were all Archipelago files
    return modeOf(save) != FILE_MODE_VANILLA;
}

// ---- The question ----

#define ASK_KEY "ap_mode_ask"
#define CHOICES_KEY "ap_mode_choices"
#define CHOICE_ARCHIPELAGO 0

// The answer given on the file select, for the file being made (FILE_MODE_*; FILE_MODE_UNSET = none)
static u8 sPicked = FILE_MODE_UNSET;

// The new file's first map, loaded again once it's answered (the fallback, asked in game)
static char sFirstMap[32];
// The question is due (the new file's map has loaded) / up
static bool sDue;
static bool sAsked;
// The file select has been shown since the title screen: a new file can only start after it. The
// opening that plays by itself before the title screen also loads at story value 0 with no answer
// (user, 2026-11-08: it was asked there too)
static bool sFileSelectSeen;

bool asking()
{
    if (spm::spmario::gp->gsw0 != 0 || save_data::getGswByte(AP_GSW_MODE) != FILE_MODE_UNSET)
        return false;
    // Answered on the file select: the new file is that. Kept until the title screen, and written
    // again if the new file's save bytes are found without it
    if (sPicked != FILE_MODE_UNSET)
    {
        save_data::setGswByte(AP_GSW_MODE, sPicked);
        wii::os::OSReport("[spmap] mode: new file is %s (chosen on the file select)\n",
                          sPicked == FILE_MODE_VANILLA ? "vanilla" : "Archipelago");
        return false;
    }
    return true;
}

void onFileSelect()
{
    sFileSelectSeen = true;
}

void forgetAnswer()
{
    sPicked = FILE_MODE_UNSET;
    sFileSelectSeen = false;
}

// Whether a map is one of the opening's (aa1_*, aa2_*), where a new file starts. Not aa3_01 (the
// ending) or aa4_01 (the title screen's background, which also loads with story value 0: it took the
// file select's answer before the new file's opening did; user, 2026-11-08: asked twice).
static bool isOpeningMap(const char * map)
{
    return msl::string::strncmp(map, "aa1_", 4) == 0 || msl::string::strncmp(map, "aa2_", 4) == 0;
}

bool onMapChange(spm::seqdrv::SeqWork * wp)
{
    // (asking() also gives the new file the file select's answer, if there is one)
    if (wp->p0 == nullptr || !isOpeningMap(wp->p0) || !asking())
        return false;
    msl::string::strncpy(sFirstMap, wp->p0, sizeof(sFirstMap) - 1);
    sFirstMap[sizeof(sFirstMap) - 1] = 0;
    // Not a new file (the opening playing before the title screen): left as it is, nothing asked
    sDue = sFileSelectSeen;
    sAsked = false;
    if (sDue)
        wii::os::OSReport("[spmap] mode: new file, asking Archipelago or vanilla on %s\n", sFirstMap);
    return true;
}

// ap_mode_hold(): pauses every other script (the opening's), which the map's reload ends anyway.
static EVT_DEFINE_USER_FUNC(ap_mode_hold)
{
    (void) isFirstCall;
    spm::evtmgr::evtStopOther(evt, 0xFF);
    return 2;
}
static constexpr s32 ap_mode_hold_parameter_count = 0;

// ap_mode_chosen(choice): saves the answer.
static EVT_DEFINE_USER_FUNC(ap_mode_chosen)
{
    (void) isFirstCall;
    s32 choice = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    u8 mode = choice == CHOICE_ARCHIPELAGO ? FILE_MODE_ARCHIPELAGO : FILE_MODE_VANILLA;
    save_data::setGswByte(AP_GSW_MODE, mode);
    wii::os::OSReport("[spmap] mode: new file is %s\n", mode == FILE_MODE_VANILLA ? "vanilla" : "Archipelago");
    return 2;
}
static constexpr s32 ap_mode_chosen_parameter_count = 1;

EVT_BEGIN(ap_mode_ask_evt)
    USER_FUNC(ap_mode_hold)
    USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(ASK_KEY), 0, 0)
    USER_FUNC(spm::evt_msg::evt_msg_select, 0, PTR(CHOICES_KEY))
    USER_FUNC(spm::evt_msg::evt_msg_continue)
    USER_FUNC(ap_mode_chosen, LW(0))
    USER_FUNC(spm::evt_seq::evt_seq_set_seq, spm::seqdrv::SEQ_MAPCHANGE, PTR(sFirstMap), 0)
    RETURN()
EVT_END()

// ap_mode_pick(choice): the file select's answer, kept for the file being made.
static EVT_DEFINE_USER_FUNC(ap_mode_pick)
{
    (void) isFirstCall;
    s32 choice = spm::evtmgr_cmd::evtGetValue(evt, evt->pCurData[0]);
    sPicked = choice == CHOICE_ARCHIPELAGO ? FILE_MODE_ARCHIPELAGO : FILE_MODE_VANILLA;
    return 2;
}
static constexpr s32 ap_mode_pick_parameter_count = 1;

/*
    The name entry's "Use this name?" (main.dol 0x803E9C18): GF(0) = 0, the question and its Yes / No,
    and on Yes GF(0) = 1, which the name entry waits for. The mod's copy asks Archipelago or Vanilla
    after a Yes, before setting GF(0). The game's script starts with a call of this one.
*/
#define NAME_CONFIRM_SCRIPT 0x803E9C18
#define NAME_ASK_KEY "nameent_01"
#define NAME_CHOICES_KEY "nameent_02"

EVT_BEGIN(ap_name_confirm_evt)
    SET(GF(0), 0)
    USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(NAME_ASK_KEY), 0, 0)
    USER_FUNC(spm::evt_msg::evt_msg_select, 0, PTR(NAME_CHOICES_KEY))
    USER_FUNC(spm::evt_msg::evt_msg_continue)
    IF_EQUAL(LW(0), 0)
        USER_FUNC(spm::evt_msg::evt_msg_print, 0, PTR(ASK_KEY), 0, 0)
        USER_FUNC(spm::evt_msg::evt_msg_select, 0, PTR(CHOICES_KEY))
        USER_FUNC(spm::evt_msg::evt_msg_continue)
        USER_FUNC(ap_mode_pick, LW(0))
        SET(GF(0), 1)
    END_IF()
    RETURN()
EVT_END()

void init()
{
    s32 * script = reinterpret_cast<s32 *>(NAME_CONFIRM_SCRIPT);
    const s32 expected[] = {(2 << 16) | 0x32, GF(0), 0, (5 << 16) | 0x5C};  // SET(GF(0), 0), USER_FUNC(...)
    for (s32 i = 0; i < 4; i++)
    {
        if (script[i] != expected[i])
        {
            wii::os::OSReport("[spmap] mode: the name entry's script isn't what was expected, asked in game instead\n");
            return;
        }
    }
    script[0] = (1 << 16) | spm::evtmgr_cmd::EVT_OPC_RUN_CHILD_EVT;
    script[1] = reinterpret_cast<s32>(ap_name_confirm_evt);
    script[2] = spm::evtmgr_cmd::EVT_OPC_END_EVT;  // RETURN()
    wii::os::OSReport("[spmap] mode: asked after naming a new file\n");
}

void tick()
{
    if (!sDue || sAsked || !asking())
        return;
    sAsked = true;
    spm::evtmgr::evtEntry(ap_mode_ask_evt, 0xFF, 0);
}

const char * message(const char * msgName)
{
    if (msl::string::strcmp(msgName, ASK_KEY) == 0)
        return "<system>\nWhich game would you like to\nplay on this file?\n<o>";
    if (msl::string::strcmp(msgName, CHOICES_KEY) == 0)
        return "<select 0 -1 200 40>\nArchipelago\nVanilla";
    return nullptr;
}

static Write * find(u32 * at)
{
    for (s32 i = 0; i < sWriteCount; i++)
        if (sWrites[i].at == at)
            return &sWrites[i];
    return nullptr;
}

static void store(u32 * at, u32 value, bool code)
{
    Write * write = find(at);
    if (write == nullptr)
    {
        if (value == *at)
            return;  // nothing changes
        if (sWriteCount == MAX_WRITES)
        {
            wii::os::OSReport("[spmap] mode: too many writes, %p written for good\n", at);
            *at = value;
            if (code)
                code_patching::refreshCode(at, sizeof(u32));
            return;
        }
        // Never written before, so what's there is the game's own (even with a vanilla file's map in)
        write = &sWrites[sWriteCount++];
        write->at = at;
        write->original = *at;
        write->code = code;
    }
    write->patched = value;
    // With the game's own words in, a change waits for the next Archipelago file
    if (sVanillaApplied)
        return;
    *at = value;
    if (code)
        code_patching::refreshCode(at, sizeof(u32));
}

void writeWord(void * at, u32 value)
{
    store(static_cast<u32 *>(at), value, false);
}

void writeHalf(void * at, u16 value)
{
    // The 4-byte word holding it, with the other half kept
    u32 address = reinterpret_cast<u32>(at);
    u32 * word = reinterpret_cast<u32 *>(address & ~3u);
    Write * write = find(word);
    u32 current = write != nullptr ? write->patched : *word;
    u32 shift = (address & 2) ? 0 : 16;  // big-endian: the lower address is the high half
    u32 combined = (current & ~(0xFFFFu << shift)) | (static_cast<u32>(value) << shift);
    store(word, combined, false);
}

void writeInstruction(u32 * at, u32 instruction)
{
    store(at, instruction, true);
}

// PowerPC "b": opcode 18, the distance, and bit 0 set for a call
static u32 branchInstruction(void * at, void * destination, bool link)
{
    u32 distance = reinterpret_cast<u32>(destination) - reinterpret_cast<u32>(at);
    return 0x48000000 | (link ? 1 : 0) | (distance & 0x03FFFFFC);
}

void writeBranch(void * at, void * destination, bool link)
{
    store(static_cast<u32 *>(at), branchInstruction(at, destination, link), true);
}

void writeBranchRaw(void * at, void * destination)
{
    code_patching::writeBranch(at, destination);
}

void apply()
{
    bool wantVanilla = vanilla();
    if (wantVanilla == sVanillaApplied)
        return;
    if (wantVanilla)
    {
        for (s32 i = sWriteCount - 1; i >= 0; i--)
        {
            *sWrites[i].at = sWrites[i].original;
            if (sWrites[i].code)
                code_patching::refreshCode(sWrites[i].at, sizeof(u32));
        }
    }
    else
    {
        for (s32 i = 0; i < sWriteCount; i++)
        {
            *sWrites[i].at = sWrites[i].patched;
            if (sWrites[i].code)
                code_patching::refreshCode(sWrites[i].at, sizeof(u32));
        }
    }
    sVanillaApplied = wantVanilla;
    wii::os::OSReport("[spmap] mode: %s file, %d randomizer writes %s\n", wantVanilla ? "vanilla" : "Archipelago",
                      sWriteCount, wantVanilla ? "taken out" : "put in");
}

}
