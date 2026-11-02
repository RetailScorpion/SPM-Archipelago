#include <common.h>
#include <spm/camdrv.h>
#include <spm/dispdrv.h>
#include <spm/icondrv.h>
#include <spm/seq_title.h>
#include <wii/mtx.h>

#include "code_patching.h"
#include "game_addresses.h"
#include "title_banner.h"

namespace mod::title_banner {

// Added by the patcher after the Save Block's icon (tools/wicon.py)
#define BANNER_ICON 407
#define FLOWER_ICON 408

// The flower behind the whole title (360x360): where its bottom middle goes, so its centre is at
// (0, 70)
#define FLOWER_X 0.0f
#define FLOWER_Y -110.0f

// Where the banner's bottom middle goes (icons are drawn up from there). Screen coordinates have 0
// at the centre and y going up; the logo is 380x177, centred at (0, 100)
#define BANNER_X 0.0f
#define BANNER_Y 8.0f

// The title's layout is drawn at z 1000 in render mode 4, where smaller z is drawn later
#define TITLE_RENDER_MODE 4
#define BANNER_Z 900.0f

// The title's two animations (SeqTitleWork animNum) and the frames that matter in them
#define ANIM_OPENING 0
#define ANIM_LOOP 1
#define LOGO_SETTLED 40.0f   // opening: the logo has dropped in and stopped squashing
#define HOP_FRAMES 50.0f     // loop: the logo sways from side to side every 50 frames...
#define HOP_HEIGHT 10.0f
static const f32 sHopWindows[][2] = {{230.0f, 380.0f}, {630.0f, 780.0f}};  // ...in these frames

// The pop-in: the banner's size over the frames after the logo settles (frame, scale), springy
// like the logo's own landing
static const f32 sPopKeys[][2] = {{0.0f, 0.0f}, {6.0f, 1.25f}, {11.0f, 0.9f}, {16.0f, 1.05f}, {20.0f, 1.0f}};

// The pop-in's scale `frames` after it started.
static f32 popScale(f32 frames)
{
    const s32 last = sizeof(sPopKeys) / sizeof(sPopKeys[0]) - 1;
    if (frames <= sPopKeys[0][0])
        return 0.0f;
    for (s32 i = 0; i < last; i++)
    {
        if (frames < sPopKeys[i + 1][0])
        {
            f32 t = (frames - sPopKeys[i][0]) / (sPopKeys[i + 1][0] - sPopKeys[i][0]);
            return sPopKeys[i][1] + (sPopKeys[i + 1][1] - sPopKeys[i][1]) * t;
        }
    }
    return sPopKeys[last][1];
}

// How high the banner is off its place at a loop frame: a hop (a parabola) every HOP_FRAMES while
// the logo sways.
static f32 hopHeight(f32 frame)
{
    for (auto & window : sHopWindows)
    {
        if (frame >= window[0] && frame < window[1])
        {
            f32 p = (frame - window[0]) / HOP_FRAMES;
            p -= static_cast<s32>(p);  // how far through this hop, 0 to 1
            return HOP_HEIGHT * 4.0f * p * (1.0f - p);
        }
    }
    return 0.0f;
}

// ---- Moving the characters out of the banner's way ----

// The characters (and their shadows and trails, which move with them) the banner would cover, and
// how far each moves: straight away from the banner, just far enough to clear it. Thoreau goes
// down and right instead, into the gap between Bowser's spikes and Mario's cap
static const struct
{
    const char * pane;
    f32 x, y;
} sMoves[] = {
    {"mario", 4.0f, -24.0f},           {"mario_kage", 4.0f, -24.0f},
    {"peach", 28.0f, -12.0f},          {"peach_kage", 28.0f, -12.0f},
    {"torunage", 30.0f, -24.0f},       {"torunage_laster", 30.0f, -24.0f},  // Thoreau
    // Small adjustments around them (user's picks)
    {"anna", 5.0f, 0.0f},              {"anna_laster", 5.0f, 0.0f},          // Tippi
    {"koopa", 0.0f, -2.0f},            {"koopa_kage", 0.0f, -2.0f},          // Bowser
    {"bom", 2.0f, 4.0f},               {"bom_laster", 2.0f, 4.0f},           // Boomer
};

// The layout's panes after the sky and clouds, in the order they're drawn. The layout draws only
// the sky and clouds itself (these are hidden while it draws); the mod then draws the flower, these,
// the banner, and the rest. Tippi and her rainbow trail go after the banner, over it
static const char * const sUnderBanner[] = {
    "bom_laster", "bom", "koopa_kage", "koopa", "torunage_laster", "torunage", "luigi_kage", "luigi",
    "peach_kage", "peach", "logo_kage", "logo", "mario_kage", "mario",
};
static const char * const sOverBanner[] = {
    "anna_laster", "anna", "c", "2007", "nintendo", "surashu", "I", "S", "pushu_2", "pushu_botan",
};

// The title layout's parts the mod uses (nw4r::lyt Layout and Pane, as the game's code uses them)
#define LAYOUT_ANIMATE 0x2C        // in the layout's list of functions: Animate(layout, option)
#define LAYOUT_ROOT_PANE 0x10
#define PANE_CALCULATE_MTX 0x10    // in a pane's list of functions: CalculateMtx(pane, drawInfo)
#define PANE_DRAW 0x14             // Draw(pane, drawInfo)
#define PANE_FIND_BY_NAME 0x3C     // FindPaneByName(pane, name, recursive)
#define PANE_TRANSLATION 0x2C      // x, y, z
#define PANE_FLAGS 0xB7
#define PANE_VISIBLE 0x01          // hidden panes aren't positioned or drawn

typedef void (LayoutAnimate)(void * layout, u32 option);
typedef void (PaneDrawStep)(void * pane, const nw4r::lyt::DrawInfo & drawInfo);
typedef void * (PaneFindByName)(void * pane, const char * name, bool recursive);

// Entry `offset` in an object's list of functions (its first word points at the list).
static void * virtualFunction(void * object, u32 offset)
{
    u8 * functions = *static_cast<u8 **>(object);
    return *reinterpret_cast<void **>(functions + offset);
}

// A pane of the title layout by name (nullptr if there's none).
static void * findPane(void * layout, const char * name)
{
    void * root = *reinterpret_cast<void **>(static_cast<u8 *>(layout) + LAYOUT_ROOT_PANE);
    return reinterpret_cast<PaneFindByName *>(virtualFunction(root, PANE_FIND_BY_NAME))(root, name, true);
}

static void setVisible(void * pane, bool visible)
{
    u8 * flags = static_cast<u8 *>(pane) + PANE_FLAGS;
    *flags = visible ? (*flags | PANE_VISIBLE) : (*flags & ~PANE_VISIBLE);
}

// Hides some of the layout's panes (from the layout's own drawing).
template <s32 N>
static void hidePanes(void * layout, const char * const (&names)[N])
{
    for (auto name : names)
    {
        void * pane = findPane(layout, name);
        if (pane != nullptr)
            setVisible(pane, false);
    }
}

// Runs instead of the title's layout animation: animates as usual, then moves the characters and
// hides the panes the mod draws itself. Every moved pane has its position animated, so the
// animation puts it back each frame and the moves never add up.
static void animateTitle(void * layout, u32 option)
{
    reinterpret_cast<LayoutAnimate *>(virtualFunction(layout, LAYOUT_ANIMATE))(layout, option);
    for (auto & move : sMoves)
    {
        void * pane = findPane(layout, move.pane);
        if (pane == nullptr)
            continue;
        f32 * translation = reinterpret_cast<f32 *>(static_cast<u8 *>(pane) + PANE_TRANSLATION);
        translation[0] += move.x;
        translation[1] += move.y;
    }
    hidePanes(layout, sUnderBanner);
    hidePanes(layout, sOverBanner);
}

// Draws panes hidden by animateTitle, over everything drawn so far.
template <s32 N>
static void drawPanes(spm::seq_title::SeqTitleWork * wp, const char * const (&names)[N])
{
    for (auto name : names)
    {
        void * pane = findPane(wp->layout, name);
        if (pane == nullptr)
            continue;
        setVisible(pane, true);
        reinterpret_cast<PaneDrawStep *>(virtualFunction(pane, PANE_CALCULATE_MTX))(pane, wp->drawInfo);
        reinterpret_cast<PaneDrawStep *>(virtualFunction(pane, PANE_DRAW))(pane, wp->drawInfo);
        setVisible(pane, false);
    }
}

// ---- Drawing ----

// Draws an icon picture with its bottom middle at (x, y), if its scale isn't 0.
static void drawIcon(s32 icon, f32 x, f32 y, f32 scale)
{
    if (scale <= 0.0f)
        return;
    wii::mtx::Vec3 position = {x, y, 0.0f};
    spm::icondrv::iconDispGx(scale, &position, 16, icon);  // 16: needed for it to show
}

// Everything over the sky and clouds: the flower, the characters and logo, the banner, then Tippi
// and the rest. The flower and banner pop in together once the logo has landed.
static void drawTitle(s32 cameraId, void * param)
{
    (void) cameraId;
    (void) param;
    spm::seq_title::SeqTitleWork * wp = game_addresses::seq_title_wp;
    if (wp == nullptr)
        return;

    f32 pop = 1.0f;
    f32 hop = 0.0f;
    if (wp->animNum == ANIM_OPENING)
        pop = popScale(wp->animFrame - LOGO_SETTLED);
    else if (wp->animNum == ANIM_LOOP)
        hop = hopHeight(wp->animFrame);

    drawIcon(FLOWER_ICON, FLOWER_X, FLOWER_Y, pop);
    drawPanes(wp, sUnderBanner);
    drawIcon(BANNER_ICON, BANNER_X, BANNER_Y + hop, pop);
    drawPanes(wp, sOverBanner);
}

void init()
{
    code_patching::writeBranch(game_addresses::title_layout_animate_call, reinterpret_cast<void *>(animateTitle), true);
}

void tick()
{
    spm::dispdrv::dispEntry(spm::camdrv::CAM_ID_2D, TITLE_RENDER_MODE, BANNER_Z, drawTitle, nullptr);
}

}
