#include <common.h>
#include <spm/camdrv.h>
#include <spm/dispdrv.h>
#include <spm/fontmgr.h>
#include <spm/icondrv.h>
#include <msl/math.h>
#include <spm/pausewin.h>
#include <wii/gx.h>
#include <wii/mtx.h>

#include "connection_status.h"
#include "mailbox.h"

namespace mod::connection_status {

#define ICON_CONNECTED 402     // the Archipelago icons the patcher adds (tools/wicon.py)
#define ICON_DISCONNECTED 404  // the grey one
#define CLIENT_TIMEOUT 180     // frames (3 seconds) without the client's echo: it's gone

// Bottom left of the screen (0 at the centre, y going up)
#define ICON_X -270.0f         // the icon's top middle (on this camera icons hang down from it)
#define ICON_Y -188.0f         // so its middle lines up with the text's
#define ICON_SCALE 0.6f        // 40 pixels -> 24
#define TEXT_X -252.0f         // the text's top left
#define TEXT_Y -193.0f
#define TEXT_SCALE 0.7f

#define WINDOW_IN_USE 0x1      // a pause menu window's flags

// "CONNECTED" in rainbow colours, its letters bobbing in a wave
#define WAVE_HEIGHT 2.5f       // how far each letter bobs up and down
#define WAVE_SPEED 0.12f       // radians per frame
#define WAVE_SPREAD 0.6f       // radians between neighbouring letters
#define RAINBOW_SPEED 4        // hue degrees per frame
#define RAINBOW_SPREAD 30      // hue degrees between neighbouring letters

static s32 sFrame;             // counts frames while the status shows, for the wave and rainbow

// Whether the pause menu is open: its own window is in use.
static bool pauseMenuOpen()
{
    spm::pausewin::PluswinWork * menu = spm::pausewin::pausewin_pluswinWp;
    spm::pausewin::PausewinWork * windows = spm::pausewin::pausewin_wp;
    if (menu == nullptr || windows == nullptr)
        return false;
    s32 id = menu->entryIds[0];
    if (id < 0 || id >= static_cast<s32>(windows->num))
        return false;
    return (spm::pausewin::pausewinGetEntry(id)->flags & WINDOW_IN_USE) != 0;
}

// Whether the client is connected to the server, and still running.
static bool connected()
{
    const mailbox::Mailbox & mb = mailbox::gMailbox;
    return mb.serverConnected != 0 && mb.gameFrames - mb.clientFrame < CLIENT_TIMEOUT;
}

// A fully bright, fully saturated colour for a hue (0-359 degrees).
static wii::gx::GXColor hueColour(s32 hue)
{
    s32 part = hue % 60 * 255 / 60;  // how far into the hue's sixth of the circle
    switch (hue / 60)
    {
        case 0: return {255, static_cast<u8>(part), 0, 255};
        case 1: return {static_cast<u8>(255 - part), 255, 0, 255};
        case 2: return {0, 255, static_cast<u8>(part), 255};
        case 3: return {0, static_cast<u8>(255 - part), 255, 255};
        case 4: return {static_cast<u8>(part), 0, 255, 255};
        default: return {255, 0, static_cast<u8>(255 - part), 255};
    }
}

// "CONNECTED" one letter at a time: each its own colour, bobbing up and down.
static void drawRainbowWave(const char * text)
{
    f32 x = TEXT_X;
    for (s32 i = 0; text[i] != 0; i++)
    {
        char letter[2] = {text[i], 0};
        wii::gx::GXColor colour = hueColour((sFrame * RAINBOW_SPEED + i * RAINBOW_SPREAD) % 360);
        spm::fontmgr::FontDrawColor(&colour);
        f32 bob = WAVE_HEIGHT * static_cast<f32>(msl::math::sin(sFrame * WAVE_SPEED + i * WAVE_SPREAD));
        spm::fontmgr::FontDrawString(x, TEXT_Y + bob, letter);
        x += spm::fontmgr::FontGetMessageWidth(letter) * TEXT_SCALE;
    }
}

static void draw(s32 cameraId, void * param)
{
    (void) cameraId;
    bool on = param != nullptr;

    wii::mtx::Vec3 position = {ICON_X, ICON_Y, 0.0f};
    spm::icondrv::iconDispGx(ICON_SCALE, &position, 16, on ? ICON_CONNECTED : ICON_DISCONNECTED);  // 16: needed for it to show

    spm::fontmgr::FontDrawStart();
    spm::fontmgr::FontDrawEdge();
    spm::fontmgr::FontDrawScale(TEXT_SCALE);
    spm::fontmgr::FontDrawNoiseOff();
    spm::fontmgr::FontDrawRainbowColorOff();
    if (on)
    {
        drawRainbowWave("CONNECTED");
        return;
    }
    wii::gx::GXColor grey = {160, 160, 160, 255};
    spm::fontmgr::FontDrawColor(&grey);
    spm::fontmgr::FontDrawString(TEXT_X, TEXT_Y, "DISCONNECTED");
}

void tick()
{
    if (!pauseMenuOpen())
        return;
    sFrame++;
    // The parameter only says which to draw
    spm::dispdrv::dispEntry(spm::camdrv::CAM_ID_DEBUG_3D, 2, 0.0f, draw, connected() ? reinterpret_cast<void *>(1) : nullptr);
}

}
