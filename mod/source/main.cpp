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
#include "climb_speed.h"
#include "connection_status.h"
#include "cutscene_skips.h"
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
#include "options_menu.h"
#include "pixl_joins.h"
#include "pure_hearts.h"
#include "received_items.h"
#include "sammer_duels.h"
#include "save_block.h"
#include "sound_effect_shuffle.h"
#include "spot_flags.h"
#include "story_gifts.h"
#include "story_progress.h"
#include "text_skip.h"
#include "tippi_always_on.h"
#include "title_banner.h"
#include "tower_doors.h"

namespace mod {

// Must match the APworld's world_version (apworld/spm/archipelago.json); shown on the title screen
#define MOD_VERSION "0.2.0"

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
    story_progress::resetContext();
    received_items::onNoSaveLoaded();
}

static spm::seqdef::SeqFunc * titleScreenReal;
static void titleScreen(spm::seqdrv::SeqWork * wp)
{
    noSaveLoaded();
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
    fileSelectScreenReal(wp);
}

// Every frame while playing.
static spm::seqdef::SeqFunc * gameFrameReal;
static void gameFrame(spm::seqdrv::SeqWork * wp)
{
    mailbox::gMailbox.modState = mailbox::STATE_IN_GAME;
    mailbox::gMailbox.gameFrames++;
    debug_warp::tick();
    received_items::publishSavedCount();

    gameFrameReal(wp);  // the game's own frame

    story_progress::sync();
    npc_story_fixes::tick();
    archipelago_item::tick();
    key_item_storage::tick();
    save_block::tick();
    climb_speed::tick();
    connection_status::tick();
    music_shuffle::tick();
    received_items::tick();
}

// Once at the start of every map change. wp->p0 is the map being loaded (wp->p1 its door); none
// of its scripts have run yet.
static spm::seqdef::SeqFunc * mapChangeReal;
static void mapChange(spm::seqdrv::SeqWork * wp)
{
    cutscene_skips::onMapChange(wp);  // first: these may change which map is loaded
    sammer_duels::onMapChange(wp);
    received_items::onMapChange();

    story_progress::onMapChange(wp->p0);  // then the story value; the rest use the new one
    tippi_always_on::onMapChange(story_progress::gStory.context, spm::spmario::gp->gsw0);
    pixl_joins::onMapChange();
    story_gifts::onMapChange();
    character_joins::onMapChange();
    npc_story_fixes::onMapChange();
    hub_story_fixes::onMapChange(wp->p0);
    pure_hearts::onMapChange();
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
    hub_story_fixes::init();
    cutscene_skips::init();
    spot_flags::init();
    title_banner::init();
    tippi_always_on::init();
    climb_speed::init();
    archipelago_item::init();

    using spm::seqdef::seq_data;
    using namespace spm::seqdrv;
    hookScreen(seq_data[SEQ_TITLE].main, titleScreenReal, titleScreen);
    hookScreen(seq_data[SEQ_GAMEOVER].main, gameOverScreenReal, gameOverScreen);
    hookScreen(seq_data[SEQ_LOAD].main, fileSelectScreenReal, fileSelectScreen);
    hookScreen(seq_data[SEQ_GAME].main, gameFrameReal, gameFrame);
    hookScreen(seq_data[SEQ_MAPCHANGE].init, mapChangeReal, mapChange);

    received_items::init();
}

}
