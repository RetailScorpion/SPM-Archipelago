/*
    Where the mod starts.

    The game loads the mod at boot (through SeekyCt's rel loader, built into the patched main.dol),
    and the loader calls main() below once. main() sets up every feature, then hooks into the game's
    "sequences", the screens the game switches between:
      - title screen, game over, file select: no save is loaded (noSaveLoaded)
      - the game itself: runs every frame while playing (gameFrame)
      - map change: runs once each time a new map starts loading (mapChange)
    Each feature lives in its own file; this one only calls them at the right moments, in the
    right order.
*/

#include <common.h>
#include <spm/fontmgr.h>
#include <spm/seqdef.h>
#include <spm/seqdrv.h>
#include <spm/spmario.h>
#include <wii/gx.h>
#include <wii/os.h>

#include "archipelago_item.h"
#include "character_joins.h"
#include "block_hints.h"
#include "climb_speed.h"
#include "connection_status.h"
#include "cutscene_skips.h"
#include "elevator_skips.h"
#include "npc_culling.h"
#include "endgame_requirements.h"
#include "debug_warp.h"
#include "file_select_hearts.h"
#include "flip_ability.h"
#include "game_addresses.h"
#include "game_messages.h"
#include "hub_story_fixes.h"
#include "key_item_storage.h"
#include "mailbox.h"
#include "music_shuffle.h"
#include "npc_story_fixes.h"
#include "check_stages.h"
#include "key_groups.h"
#include "options_menu.h"
#include "pixl_joins.h"
#include "pure_hearts.h"
#include "received_items.h"
#include "sammer_duels.h"
#include "save_block.h"
#include "sound_effect_shuffle.h"
#include "tattle_checks.h"
#include "starting_character.h"
#include "spot_flags.h"
#include "story_gifts.h"
#include "story_progress.h"
#include "text_skip.h"
#include "tippi_always_on.h"
#include "title_banner.h"
#include "tower_doors.h"
#include "tower_top.h"
#include "squirps_shots.h"
#include "sammer_ninjas.h"
#include "connect_gate.h"
#include "pose_waits.h"
#include "pause_chapters.h"
#include "fast_travel.h"
#include "wallet.h"
#include "flopside_pipe.h"
#include "game_mode.h"

namespace mod {

// Must match the APworld's world_version (apworld/spm/archipelago.json); shown on the title screen
#define MOD_VERSION "0.2.2"

// Where relF's script section starts inside the relF.rel file
#define RELF_SECTION5_FILE_OFFSET 0xAF300

// ---- Title screen ----

// The title screen's text. Room is left after it so tools/make_dev_disc.py can write "SPM
// Archipelago Dev Build" over it in the dev disc's copy of the mod
static const char sTitleText[48] = "SPM Archipelago " MOD_VERSION;

// Draws text centred across the screen at height y (0 = middle, y going up).
static void drawCentredText(const char * text, f32 y, wii::gx::GXColor colour)
{
    f32 scale = 0.8f;
    spm::fontmgr::FontDrawStart();
    spm::fontmgr::FontDrawEdge();
    spm::fontmgr::FontDrawColor(&colour);
    spm::fontmgr::FontDrawScale(scale);
    spm::fontmgr::FontDrawNoiseOff();
    spm::fontmgr::FontDrawRainbowColorOff();
    f32 x = -((spm::fontmgr::FontGetMessageWidth(text) * scale) / 2);
    spm::fontmgr::FontDrawString(x, y, text);
}

// ---- The game's screens ----

// No save is loaded (title screen, game over, file select). The file select reads every save
// slot, so the client mustn't read checks then; and loading a save reloads the map scripts with
// their vanilla items, so everything waiting is dropped and sent again for the next save.
static void noSaveLoaded()
{
    mailbox::gMailbox.modState = mailbox::STATE_TITLE;
    mailbox::gMailbox.seedPlaced = 0;  // the next save loads with vanilla items until the client places
    story_progress::resetContext();
    received_items::onNoSaveLoaded();
    save_block::onNoSaveLoaded();
    connect_gate::onNoSaveLoaded();
}

static spm::seqdef::SeqFunc * titleScreenReal;
static void titleScreen(spm::seqdrv::SeqWork * wp)
{
    noSaveLoaded();
    game_mode::forgetAnswer();  // a new file's answer is given on the file select after this
    titleScreenReal(wp);
    title_banner::tick();
    drawCentredText(sTitleText, 200.0f, {255, 255, 255, 255});
}

static spm::seqdef::SeqFunc * gameOverScreenReal;
static void gameOverScreen(spm::seqdrv::SeqWork * wp)
{
    noSaveLoaded();
    gameOverScreenReal(wp);
}

static spm::seqdef::SeqFunc * fileSelectScreenReal;
static void fileSelectScreen(spm::seqdrv::SeqWork * wp)
{
    noSaveLoaded();
    game_mode::onFileSelect();
    options_menu::fileSelectTick();  // first: while its window is open, the file select gets no buttons
    fileSelectScreenReal(wp);
}

// Every frame while playing.
static spm::seqdef::SeqFunc * gameFrameReal;
static void gameFrame(spm::seqdrv::SeqWork * wp)
{
    // A vanilla file: the client leaves it alone (no items, no checks), and only the quality-of-life
    // features run (game_mode)
    mailbox::gMailbox.gameFrames++;
    if (game_mode::asking())
    {
        // A new file, before it's told Archipelago or vanilla: just the question
        mailbox::gMailbox.modState = mailbox::STATE_TITLE;
        gameFrameReal(wp);
        game_mode::tick();
        return;
    }
    bool randomizer = !game_mode::vanilla();
    mailbox::gMailbox.modState = randomizer ? mailbox::STATE_IN_GAME : mailbox::STATE_IN_GAME_VANILLA;
    debug_warp::tick();
    if (randomizer)
        received_items::publishSavedCount();
    squirps_shots::rapidFireTick();  // before the game's frame, which reads the buttons
    npc_culling::tick();  // before the game's frame, so its NPC update sees this frame's culling

    gameFrameReal(wp);  // the game's own frame

    climb_speed::tick();
    tower_top::tick();
    squirps_shots::tick();
    sammer_ninjas::tick();
    block_hints::tick();
    music_shuffle::tick();
    if (!randomizer)
        return;
    story_progress::sync();
    connect_gate::tick();
    pause_chapters::tick();
    fast_travel::tick();
    npc_story_fixes::tick();
    archipelago_item::tick();
    key_item_storage::tick();
    save_block::tick();
    endgame_requirements::tick();
    starting_character::tick();
    wallet::tick();
    tattle_checks::tick();
    connection_status::tick();
    received_items::tick();
}

// Once at the start of every map change. wp->p0 is the map being loaded (wp->p1 its door); none
// of its scripts have run yet.
static spm::seqdef::SeqFunc * mapChangeReal;
static void mapChange(spm::seqdrv::SeqWork * wp)
{
    // A new file not yet told Archipelago or vanilla: its first map loads as it is, for the question
    // (the answer loads it again)
    if (game_mode::onMapChange(wp))
    {
        options_menu::carryOverToNewFile();
        mapChangeReal(wp);
        return;
    }
    cutscene_skips::onMapChange(wp);  // first: these may change which map is loaded
    // The randomizer's changes to the game in or out, by the file's mode
    game_mode::apply();
    elevator_skips::onMapChange();
    npc_culling::onMapChange();
    sammer_ninjas::onMapChange();
    if (game_mode::vanilla())
    {
        block_hints::onMapChange(wp->p0);
        mapChangeReal(wp);
        return;
    }
    connect_gate::onMapChange(wp->p0);
    sammer_duels::onMapChange(wp);
    received_items::onMapChange();

    story_progress::onMapChange(wp->p0);  // then the story value; the rest use the new one
    tippi_always_on::onMapChange(story_progress::gStory.context, spm::spmario::gp->gsw0);
    pixl_joins::onMapChange();
    story_gifts::onMapChange();
    character_joins::onMapChange();
    starting_character::onMapChange();  // after character_joins, which makes held characters selectable
    npc_story_fixes::onMapChange();
    check_stages::onMapChange();
    key_groups::onMapChange();
    hub_story_fixes::onMapChange(wp->p0);
    flopside_pipe::onMapChange();
    pure_hearts::onMapChange();
    endgame_requirements::onMapChange(wp->p0);
    block_hints::onMapChange(wp->p0);
    save_block::onMapChange(wp->p0);
    archipelago_item::onMapChange();

    mapChangeReal(wp);  // the game's own map change
}

// Points one of the game's screens at the mod's version, keeping the game's own to call.
static void hookScreen(spm::seqdef::SeqFunc *& slot, spm::seqdef::SeqFunc *& real, spm::seqdef::SeqFunc * replacement)
{
    real = slot;
    slot = replacement;
}

// ---- Start ----

void main()
{
    wii::os::OSReport("[spmap] SPM Archipelago " MOD_VERSION " loaded\n");

    mailbox::init();
    // relF stays loaded for the whole game, so its address can be given to the client now
    mailbox::gMailbox.relfBase = reinterpret_cast<u32>(game_addresses::relF_section5) - RELF_SECTION5_FILE_OFFSET;
    story_progress::init();
    mailbox::gMailbox.storyState = &story_progress::gStory;

    game_messages::init();
    game_mode::init();
    squirps_shots::init();
    pose_waits::init();
    pause_chapters::init();
    fast_travel::init();
    tower_doors::init();
    flip_ability::init();
    file_select_hearts::init();
    save_block::init();
    options_menu::init();
    text_skip::init();
    music_shuffle::init();
    sound_effect_shuffle::init();
    pixl_joins::init();
    character_joins::init();
    npc_story_fixes::init();
    check_stages::init();
    key_groups::init();
    hub_story_fixes::init();
    cutscene_skips::init();
    elevator_skips::init();
    spot_flags::init();
    title_banner::init();
    tippi_always_on::init();
    climb_speed::init();
    endgame_requirements::init();
    archipelago_item::init();
    tattle_checks::init();

    using spm::seqdef::seq_data;
    using namespace spm::seqdrv;
    hookScreen(seq_data[SEQ_TITLE].main, titleScreenReal, titleScreen);
    hookScreen(seq_data[SEQ_GAMEOVER].main, gameOverScreenReal, gameOverScreen);
    hookScreen(seq_data[SEQ_LOAD].main, fileSelectScreenReal, fileSelectScreen);
    hookScreen(seq_data[SEQ_GAME].main, gameFrameReal, gameFrame);
    hookScreen(seq_data[SEQ_MAPCHANGE].init, mapChangeReal, mapChange);

    wallet::init();
    received_items::init();
}

}
