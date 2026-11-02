#pragma once

/*
    The Archipelago banner on the title screen.

    RetailScorpion's banner (assets/title/archipelago_banner.png) is added to the item icon pictures
    by the patcher, and drawn under the Super Paper Mario logo. It pops in once the logo has dropped
    into place, and hops while the logo sways. The characters it would cover (Mario, Peach, Thoreau)
    are moved out of its way, and Tippi is drawn over it. RetailScorpion's flower
    (assets/title/archipelago_flower.png) pops in behind the whole title, over the sky.

    How: the title screen's own animation (its work, game_addresses::seq_title_wp) says which frame
    the logo is at; the banner is drawn right after the title's layout, in the same camera. The
    title's call that animates its layout is redirected to the mod, which animates it, shifts the
    characters and hides everything over the sky; the mod then draws the flower, those panes and the
    banner in its own order.
*/

namespace mod::title_banner {

// Moves the characters out of the banner's way. Call once at startup.
void init();

// Call every frame while the title screen is up.
void tick();

}
