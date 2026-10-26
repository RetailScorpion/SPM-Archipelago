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

import Utils
from CommonClient import ClientCommandProcessor, CommonContext, get_base_parser, gui_enabled, logger, server_loop
from NetUtils import ClientStatus

from .data import ID_BASE, LOCATIONS
from .game_link import JOIN_ITEM_OTHER_WORLD, NO_SAVE, RECV_SIZE, STATE_IN_GAME, Mailbox, MailboxError

GAME = "Super Paper Mario"
AP_ITEM = 63               # the mod's stand-in for other worlds' items (apitem.cpp)
GOAL_CHAPTER = 8
GOAL_STORY_VALUE = 417     # ls4_12, the final boss arena
POLL_SECONDS = 0.5


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

    async def server_auth(self, password_requested: bool = False):
        if password_requested and not self.password:
            await super().server_auth(password_requested)
        await self.get_username()
        await self.send_connect()

    def on_package(self, cmd: str, args: dict):
        if cmd == "Connected":
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
        if network_item.player == self.slot:
            return network_item.item - ID_BASE, False
        return AP_ITEM, True

    def write_placements(self):
        mb = self.mailbox
        names, failed = [], 0
        for i, (name, kind, _, vanilla, flag, patches) in enumerate(LOCATIONS):
            network_item = self.scouted.get(ID_BASE + i)
            if network_item is None:
                continue
            shown, foreign = self.item_shown(network_item)
            if foreign:
                names.append((flag, self.item_names.lookup_in_slot(network_item.item, network_item.player),
                              self.player_names.get(network_item.player, "someone")))
            if kind == "join":
                mb.set_join_item(vanilla, JOIN_ITEM_OTHER_WORLD if foreign else shown)
            for offset, original in patches:
                if not mb.patch_relf(offset, original, shown):
                    failed += 1
        mb.write_loc_names(names)
        if failed:
            logger.warning(f"{failed} item placements didn't match the game; are you on US 1.0 with the matching mod?")
        logger.info(f"Placed items at {len(self.scouted)} locations")

    def check_locations(self):
        flags = self.mailbox.read_flags()
        new = []
        for i, (_, _, _, _, flag, _) in enumerate(LOCATIONS):
            location = ID_BASE + i
            if location in self.missing_locations and Mailbox.flag_set(flags, flag):
                new.append(location)
        if new:
            Utils.async_start(self.send_msgs([{"cmd": "LocationChecks", "locations": new}]))

    def feed_items(self):
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
        own_locations = {ID_BASE + i for i in range(len(LOCATIONS))}
        while self.queued_upto < len(self.items_received) and mb.recv_pending() < RECV_SIZE:
            item = self.items_received[self.queued_upto]
            given_here = item.player == self.slot and item.location in own_locations
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
                ctx.game_status = f"Connected to the game (mailbox at {mb.addr:#010x})"
                logger.info(ctx.game_status)
            if ctx.slot is not None and ctx.mailbox.mod_state() == STATE_IN_GAME:
                if not ctx.placed and ctx.scouted:
                    ctx.write_placements()
                    ctx.placed = True
                ctx.check_locations()
                ctx.feed_items()
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
