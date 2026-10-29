"""Super Paper Mario client: connects the Archipelago server to the game through Dolphin.

The mod (loaded by the patched game) keeps a mailbox in memory (game_link.py). This client:
  - writes each location's item into the game: the item argument of the script instruction that spawns
    it (relF stays loaded all game), the Pixl join table, and the names shown for other worlds' items
  - reports a location when its save flag is set
  - feeds items from the server into the mailbox, from the loaded save's count; items for this world
    found here were already given by the game, so they're only counted
  - reports the goal when Chapter 8's story value reaches GOAL_STORY_VALUE
Connect before entering levels: placements only apply once the client has written them.
"""

import asyncio
import random

import Utils
from BaseClasses import ItemClassification
from CommonClient import ClientCommandProcessor, CommonContext, get_base_parser, gui_enabled, logger, server_loop
from NetUtils import ClientStatus

from .data import ID_BASE, LOCATIONS
from .game_link import (GIFT_FLAG_BASE, JOIN_ITEM_OTHER_WORLD, LOC_KIND_OTHER_WORLD, LOC_KIND_OTHER_WORLD_FILLER,
                        LOC_KIND_TRAP, NO_SAVE, RECV_SIZE,
                        STATE_IN_GAME, Mailbox, MailboxError)

GAME = "Super Paper Mario"
AP_ITEM = 63               # the mod's stand-in for other worlds' items (apitem.cpp)
COIN = 1                  # what a spot holds once its item was already delivered to the loaded save
SERVER_ONLY_FIRST = 1001   # SPM ids from here (Pure Hearts, traps) aren't game items: shown as the AP item
HEARTS = range(1001, 1009) # Pure Hearts, always delivered from the server (mod doors.h PURE_HEART_ITEM_FIRST)
TRAP = 1100                # Cursya Trap (mod apitem.h TRAP_ITEM_ID): found here, the mod plays it itself
# What a trap here looks like: one of the key items placed in the game's own spots (Pixls have no
# world-item sprite, characters crash the world-item pickup)
DISGUISES = sorted({vanilla for _, _, category, vanilla, _, _ in LOCATIONS if category == "key_item"})
GOAL_CHAPTER = 8
GOAL_STORY_VALUE = 417     # ls4_12, the final boss arena
POLL_SECONDS = 0.5
STALLED_POLLS_RECONNECT = 5  # polls without the game running before looking for the mailbox again


class SPMCommandProcessor(ClientCommandProcessor):
    def _cmd_dolphin(self):
        """Show the connection to Dolphin and the game."""
        ctx = self.ctx
        if isinstance(ctx, SPMContext):
            logger.info(ctx.game_status)


class SPMContext(CommonContext):
    game = GAME
    items_handling = 0b111  # every item, including this world's own (local ones are only counted)
    command_processor = SPMCommandProcessor

    def __init__(self, server_address, password):
        super().__init__(server_address, password)
        self.mailbox = None
        self.game_status = "Waiting for Dolphin"
        self.scouted = {}          # AP location id -> NetworkItem
        self.placed = False        # placements written for the current game session
        self.queued_upto = None    # server item index queued into the mailbox
        self.goal_sent = False
        self.last_frame = None     # the game's frame counter at the last poll
        self.last_flags = None     # the save flags at the last poll
        self.coin_locations = set()  # location indexes whose item the loaded save already got delivered
        self.was_in_game = False   # the mod reported a loaded save at the last poll
        self.stalled_polls = 0     # polls in a row with the mod's frame count not moving
        self.slot_options = {}
        self.starting_chapters = []

    async def server_auth(self, password_requested: bool = False):
        if password_requested and not self.password:
            await super().server_auth(password_requested)
        await self.get_username()
        await self.send_connect()

    def on_package(self, cmd: str, args: dict):
        if cmd == "Connected":
            self.slot_options = args.get("slot_data", {}).get("options", {})
            self.starting_chapters = args.get("slot_data", {}).get("starting_chapters", [])
            self.scouted = {}
            self.placed = False
            self.goal_sent = False
            Utils.async_start(self.send_msgs([{"cmd": "LocationScouts", "locations": list(self.missing_locations | self.checked_locations), "create_as_hint": 0}]))
        elif cmd == "LocationInfo":
            for item in args["locations"]:
                self.scouted[item.location] = item
            self.placed = False

    def make_gui(self):
        ui = super().make_gui()
        ui.base_title = "Super Paper Mario Client"
        return ui

    # Game side

    def item_shown(self, network_item) -> tuple[int, bool]:
        """(SPM item id shown at a location, belongs to another world)."""
        if network_item.player == self.slot and network_item.item - ID_BASE < SERVER_ONLY_FIRST:
            return network_item.item - ID_BASE, False
        return AP_ITEM, True

    def disguise(self, location: int) -> int:
        """A trap's look: a key item not received yet (the same one each time for a location)."""
        received = {item.item - ID_BASE for item in self.items_received if item.player == self.slot}
        choices = [item for item in DISGUISES if item not in received] or DISGUISES
        return random.Random(f"{self.seed_name}-{location}").choice(choices)

    def received_here(self, flags) -> set[int]:
        """Locations (indexes) holding this world's items that the loaded save got delivered rather than
        found there (found after it was saved, or sent by mistake): they hold a coin instead, so
        picking them up again doesn't give the item twice. Pure Hearts never come from their spot."""
        found = set()
        for item in self.items_received[:self.queued_upto or 0]:
            index = item.location - ID_BASE
            if (item.player == self.slot and 0 <= index < len(LOCATIONS) and item.item - ID_BASE not in HEARTS
                    and not Mailbox.flag_set(flags, LOCATIONS[index][4])):
                found.add(index)
        return found

    def write_placements(self):
        mb = self.mailbox
        names, failed = [], 0
        for i, (name, kind, _, vanilla, flag, patches) in enumerate(LOCATIONS):
            network_item = self.scouted.get(ID_BASE + i)
            if network_item is None:
                continue
            shown, foreign = self.item_shown(network_item)
            placed = shown
            if i in self.coin_locations:
                shown = COIN
                if kind == "join":
                    mb.set_join_item(vanilla, COIN)
                elif kind == "gift":
                    mb.set_gift_item(flag - GIFT_FLAG_BASE, COIN)
                for offset, original in patches:
                    if not mb.patch_relf(offset, original, COIN, (placed, self.disguise(ID_BASE + i))):
                        failed += 1
                continue
            if network_item.player == self.slot and network_item.item - ID_BASE == TRAP:
                # Spawned items look like a key item; joins and gifts give the AP item. The mod turns
                # either into the trap's item-get
                names.append((flag, "Cursya Trap", "", LOC_KIND_TRAP))
                shown = self.disguise(ID_BASE + i)
            elif foreign:
                names.append((flag, self.item_names.lookup_in_slot(network_item.item, network_item.player),
                              self.player_names.get(network_item.player, "someone"),
                              LOC_KIND_OTHER_WORLD if network_item.flags & (ItemClassification.progression | ItemClassification.useful)
                              else LOC_KIND_OTHER_WORLD_FILLER))
            if kind == "join":
                mb.set_join_item(vanilla, JOIN_ITEM_OTHER_WORLD if foreign else shown)
            elif kind == "gift":
                mb.set_gift_item(flag - GIFT_FLAG_BASE, JOIN_ITEM_OTHER_WORLD if foreign else shown)
            for offset, original in patches:
                if not mb.patch_relf(offset, original, shown, (placed, COIN)):
                    failed += 1
        mb.write_loc_names(names)
        if failed:
            logger.warning(f"{failed} item placements didn't match the game; are you on US 1.0 with the matching mod?")
        logger.info(f"Placed items at {len(self.scouted)} locations")

    def game_running(self) -> bool:
        """The mod ran in a loaded save since the last poll. A Dolphin reset keeps memory: the old mod's
        mailbox still says a save is loaded while the game reboots, with memory half old, half new.
        Nothing is read then."""
        frame = self.mailbox.game_frames()
        running = self.last_frame is not None and frame != self.last_frame
        self.stalled_polls = 0 if running else self.stalled_polls + 1
        self.last_frame = frame
        if self.stalled_polls >= STALLED_POLLS_RECONNECT:
            # Paused, or reset with the new mod's mailbox elsewhere: look for it again (quietly: the
            # status only changes, and is logged, if it's somewhere else now)
            self.stalled_polls = 0
            self.last_frame = None
            self.mailbox = None
            return False
        return running

    def check_locations(self, flags):
        # Only flags that read the same twice in a row (one poll apart) count
        stable, self.last_flags = self.last_flags == flags, flags
        if not stable:
            return
        new = []
        for i, (_, _, _, _, flag, _) in enumerate(LOCATIONS):
            location = ID_BASE + i
            if location in self.missing_locations and Mailbox.flag_set(flags, flag):
                new.append(location)
        if new:
            Utils.async_start(self.send_msgs([{"cmd": "LocationChecks", "locations": new}]))

    def feed_items(self, flags):
        mb = self.mailbox
        # Pending first: the mod updates the saved count before marking an entry handled, so with
        # nothing pending the count read after it is current
        pending = mb.recv_pending()
        saved = mb.saved_recv_count()
        if saved == NO_SAVE:
            self.queued_upto = None
            return
        # A save was loaded (or reloaded): queue again from what that save has handled
        if self.queued_upto is None or (pending == 0 and self.queued_upto != saved):
            self.queued_upto = saved
        while self.queued_upto < len(self.items_received) and mb.recv_pending() < RECV_SIZE:
            item = self.items_received[self.queued_upto]
            # Found here: the game gave it then, if the loaded save has that location (a save from before
            # it was found, or a check sent by mistake, gets it delivered instead)
            index = item.location - ID_BASE
            given_here = (item.player == self.slot and 0 <= index < len(LOCATIONS)
                          and item.item - ID_BASE not in HEARTS
                          and Mailbox.flag_set(flags, LOCATIONS[index][4]))
            mb.queue_received(item.item - ID_BASE, given_here)
            self.queued_upto += 1

    def check_goal(self):
        if self.goal_sent:
            return
        _, slots, _ = self.mailbox.story_state()
        if slots[GOAL_CHAPTER] >= GOAL_STORY_VALUE:
            self.goal_sent = True
            Utils.async_start(self.send_msgs([{"cmd": "StatusUpdate", "status": ClientStatus.CLIENT_GOAL}]))
            logger.info("Goal complete!")


async def game_watcher(ctx: SPMContext):
    while not ctx.exit_event.is_set():
        try:
            if ctx.mailbox is None:
                mb = Mailbox()
                mb.connect()
                ctx.mailbox = mb
                ctx.placed = False
                status = f"Connected to the game (mailbox at {mb.addr:#010x})"
                if status != ctx.game_status:
                    ctx.game_status = status
                    logger.info(status)
            if ctx.slot is not None:
                # Before a save is loaded too: a new file's opening checks it right away
                ctx.mailbox.set_play_cutscenes(not ctx.slot_options.get("skip_cutscenes", True))
                ctx.mailbox.set_starting_hearts(ctx.starting_chapters)
            in_game = ctx.slot is not None and ctx.mailbox.mod_state() == STATE_IN_GAME
            if in_game and not ctx.was_in_game:
                # A save was (re)loaded: that reloads relF with vanilla items, so place them again
                ctx.placed = False
                ctx.last_flags = None
            ctx.was_in_game = in_game
            if in_game:
                if not ctx.placed and ctx.scouted:
                    ctx.write_placements()
                    ctx.placed = True
                if ctx.game_running() and ctx.mailbox is not None:
                    flags = ctx.mailbox.read_flags()
                    ctx.check_locations(flags)
                    ctx.feed_items(flags)
                    # Spots whose item the loaded save already got hold a coin (from the next map load)
                    coins = ctx.received_here(flags)
                    if ctx.scouted and coins != ctx.coin_locations:
                        ctx.coin_locations = coins
                        ctx.write_placements()
                    ctx.check_goal()
        except MailboxError as e:
            if ctx.game_status != str(e):
                ctx.game_status = str(e)
                logger.info(f"Game: {e}")
            ctx.mailbox = None
        except RuntimeError as e:
            # Dolphin closed or the game stopped mid-read
            logger.info(f"Lost Dolphin ({e}); reconnecting")
            ctx.mailbox = None
        await asyncio.sleep(POLL_SECONDS)


def main(*args):
    Utils.init_logging("SPMClient", exception_logger="Client")

    async def _main(connect, password):
        ctx = SPMContext(connect, password)
        ctx.server_task = asyncio.create_task(server_loop(ctx), name="ServerLoop")
        if gui_enabled:
            ctx.run_gui()
        ctx.run_cli()
        watcher = asyncio.create_task(game_watcher(ctx), name="GameWatcher")
        await ctx.exit_event.wait()
        ctx.server_address = None
        await ctx.shutdown()
        await watcher

    parser = get_base_parser(description="Super Paper Mario Client")
    parsed = parser.parse_args(args)
    import colorama
    colorama.just_fix_windows_console()
    asyncio.run(_main(parsed.connect, parsed.password))
    colorama.deinit()
