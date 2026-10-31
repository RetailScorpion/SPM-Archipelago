"""The Super Paper Mario Client: connects the Archipelago server to the game running in Dolphin.

It runs in the Archipelago Launcher. Every half second it:
  1. finds the mod's mailbox in the game's memory (game_link.py), if it hasn't yet
  2. writes the seed's settings into the mailbox (cutscenes, starting Pure Hearts)
  3. when a save is loaded, writes every location's item into the game (placement.py)
  4. reads checked locations and sends them to the server, puts items from the server into the
     mailbox, and checks the goal (checks.py)

Connect before entering a level: a level's items are only changed once the client has written them.
"""

import asyncio

import Utils
from CommonClient import ClientCommandProcessor, CommonContext, get_base_parser, gui_enabled, logger, server_loop

from . import checks, placement
from .game_link import STATE_IN_GAME, Mailbox, MailboxError
from .items import GAME

POLL_SECONDS = 0.5


class SPMCommandProcessor(ClientCommandProcessor):
    def _cmd_dolphin(self):
        """Show the connection to Dolphin and the game."""
        ctx = self.ctx
        if isinstance(ctx, SPMContext):
            logger.info(ctx.game_status)


class SPMContext(CommonContext):
    """Everything the client keeps track of."""

    game = GAME
    items_handling = 0b111  # every item, including this world's own (found here: only counted)
    command_processor = SPMCommandProcessor

    def __init__(self, server_address, password):
        super().__init__(server_address, password)
        self.mailbox = None
        self.game_status = "Waiting for Dolphin"
        self.scouted = {}            # location id -> the item there (from the server)
        self.placed = False          # items written into the game for the current save
        self.queued_upto = None      # how many server items have been put in the mailbox
        self.goal_sent = False
        self.last_frame = None       # the mod's frame counter at the last check
        self.last_flags = None       # the save flags at the last check
        self.coin_locations = set()  # locations whose item the loaded save already got delivered
        self.was_in_game = False     # whether a save was loaded at the last check
        self.stalled_polls = 0       # checks in a row with the frame counter not moving
        self.mailbox_addr = None     # where the mailbox was found last
        self.slot_options = {}
        self.starting_chapters = []
        self.shared_checks = None    # (seed, slot, flags) last written for the tracker
        self.shared_at = 0.0         # when it was written

    async def server_auth(self, password_requested: bool = False):
        if password_requested and not self.password:
            await super().server_auth(password_requested)
        await self.get_username()
        await self.send_connect()

    def on_package(self, cmd: str, args: dict):
        """Messages from the server: on connecting, ask what every location holds."""
        if cmd == "Connected":
            self.slot_options = args.get("slot_data", {}).get("options", {})
            self.starting_chapters = args.get("slot_data", {}).get("starting_chapters", [])
            self.scouted = {}
            self.placed = False
            self.goal_sent = False
            every_location = list(self.missing_locations | self.checked_locations)
            Utils.async_start(self.send_msgs([{"cmd": "LocationScouts", "locations": every_location, "create_as_hint": 0}]))
        elif cmd == "LocationInfo":
            for item in args["locations"]:
                self.scouted[item.location] = item
            self.placed = False

    def make_gui(self):
        ui = super().make_gui()
        ui.base_title = "Super Paper Mario Client"
        return ui


# ---- The loop ----

def find_mailbox(ctx: SPMContext):
    """Finds the mod's mailbox in the game's memory."""
    mb = Mailbox()
    mb.connect()
    # Found again in the same place (the game was paused, say the Wii Remote disconnected): same mod,
    # same items. A save reload or a reset is caught by the mod leaving the game first (was_in_game)
    if mb.addr != ctx.mailbox_addr:
        ctx.placed = False
        ctx.mailbox_addr = mb.addr
    ctx.mailbox = mb
    status = f"Connected to the game (mailbox at {mb.addr:#010x})"
    if status != ctx.game_status:
        ctx.game_status = status
        logger.info(status)


def send_seed_settings(ctx: SPMContext):
    """Writes the seed's settings into the mailbox. Done before a save is loaded too, since a new
    file's opening reads them right away."""
    ctx.mailbox.set_play_cutscenes(not ctx.slot_options.get("skip_cutscenes", True))
    ctx.mailbox.set_starting_hearts(ctx.starting_chapters)


def update_game(ctx: SPMContext):
    """Places items, reads checks and sends items, while a save is loaded."""
    if not ctx.placed and ctx.scouted:
        placement.write_placements(ctx)
        ctx.placed = True
    if not checks.game_running(ctx) or ctx.mailbox is None:
        return
    flags = ctx.mailbox.read_flags()
    checks.check_locations(ctx, flags)
    checks.feed_items(ctx, flags)
    # Spots whose item the loaded save already got hold a coin (from the next map load)
    coins = placement.delivered_not_found(ctx, flags)
    if ctx.scouted and coins != ctx.coin_locations:
        ctx.coin_locations = coins
        placement.write_placements(ctx)
    checks.check_goal(ctx)


def poll_game(ctx: SPMContext):
    """One check of the game."""
    if ctx.slot is not None:
        checks.share_checks(ctx)
    if ctx.mailbox is None:
        find_mailbox(ctx)
    if ctx.slot is not None:
        send_seed_settings(ctx)

    in_game = ctx.slot is not None and ctx.mailbox.mod_state() == STATE_IN_GAME
    if in_game and not ctx.was_in_game:
        # A save was (re)loaded: that reloads the map scripts with vanilla items, so place them again
        ctx.placed = False
        ctx.last_flags = None
    ctx.was_in_game = in_game
    if in_game:
        update_game(ctx)


async def game_watcher(ctx: SPMContext):
    """Checks the game every half second until the client closes."""
    while not ctx.exit_event.is_set():
        try:
            poll_game(ctx)
        except MailboxError as e:
            if ctx.game_status != str(e):
                ctx.game_status = str(e)
                logger.info(f"Game: {e}")
            ctx.mailbox = None
        except RuntimeError as e:
            # Dolphin closed, or the game stopped in the middle of a read
            logger.info(f"Lost Dolphin ({e}); reconnecting")
            ctx.mailbox = None
        await asyncio.sleep(POLL_SECONDS)


def main(*args):
    """Starts the client (the Launcher calls this)."""
    Utils.init_logging("SPMClient", exception_logger="Client")

    async def run(connect, password):
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
    asyncio.run(run(parsed.connect, parsed.password))
    colorama.deinit()
