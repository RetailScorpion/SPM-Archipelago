"""Reading checks and sending items (part of the client).

  - checked locations: every location has a save flag; when the game sets it, the client tells the
    server the location is checked
  - received items: items from the server go into the mailbox for the mod to give, starting from how
    many the loaded save has already handled (so an older save gets the missing ones again)
  - the goal: once Chapter 8 reaches the final boss arena, the client tells the server
  - the tracker: the client keeps a file of the locations the server has as checked, for the tracker
    and coords windows
"""

import time

import Utils
from CommonClient import logger
from NetUtils import ClientStatus

from .data import ID_BASE
from .game_link import NO_SAVE, RECV_SIZE, SERVER_CHECKS_HEARTBEAT, Mailbox, write_server_checks
from .locations import LOCATION_LIST, is_location_index, location_id, location_index
from .placement import HEARTS

GOAL_CHAPTER = 8
GOAL_STORY_VALUE = 417     # map ls4_12, the final boss arena
STALLED_POLLS_RECONNECT = 5  # checks in a row without the game running before looking for the mailbox again


def game_running(ctx) -> bool:
    """Whether the game has been running in a loaded save since the last check (the mod's frame
    counter moved). Dolphin's reset keeps memory, so while the game reboots the old mailbox still says
    a save is loaded, with memory half old and half new: nothing is read then."""
    frame = ctx.mailbox.game_frames()
    running = ctx.last_frame is not None and frame != ctx.last_frame
    ctx.stalled_polls = 0 if running else ctx.stalled_polls + 1
    ctx.last_frame = frame
    if ctx.stalled_polls >= STALLED_POLLS_RECONNECT:
        # Paused, or reset with the new mod's mailbox somewhere else: look for it again. (Quietly: the
        # status only changes, and is logged, if it moved.)
        ctx.stalled_polls = 0
        ctx.last_frame = None
        ctx.mailbox = None
        return False
    return running


def check_locations(ctx, flags: bytes):
    """Tells the server about locations whose save flag is now set. Only flags that read the same twice
    in a row (half a second apart) count, so a half-loaded save can't send false checks."""
    stable, ctx.last_flags = ctx.last_flags == flags, flags
    if not stable:
        return
    new = []
    for i, location in enumerate(LOCATION_LIST):
        if location_id(i) in ctx.missing_locations and Mailbox.flag_set(flags, location.flag):
            new.append(location_id(i))
    if new:
        Utils.async_start(ctx.send_msgs([{"cmd": "LocationChecks", "locations": new}]))


def found_in_this_save(ctx, item, flags: bytes) -> bool:
    """Whether a received item is one of this world's, found in the game at a location the loaded save
    has collected: the game already gave it then, so the mod only counts it. (A save from before it
    was found, or a location sent by mistake, gets it delivered instead.)"""
    index = location_index(item.location)
    return (item.player == ctx.slot and is_location_index(index)
            and item.item - ID_BASE not in HEARTS
            and Mailbox.flag_set(flags, LOCATION_LIST[index].flag))


def feed_items(ctx, flags: bytes):
    """Puts items from the server into the mailbox for the mod to give."""
    mb = ctx.mailbox
    # "Waiting" first: the mod updates the save's count before taking an item off the list, so with
    # nothing waiting, the count read afterwards is current
    waiting = mb.recv_pending()
    saved = mb.saved_recv_count()
    if saved == NO_SAVE:
        ctx.queued_upto = None
        return
    # A save was loaded (or reloaded): start again from what that save has handled
    if ctx.queued_upto is None or (waiting == 0 and ctx.queued_upto != saved):
        ctx.queued_upto = saved
    while ctx.queued_upto < len(ctx.items_received) and mb.recv_pending() < RECV_SIZE:
        item = ctx.items_received[ctx.queued_upto]
        mb.queue_received(item.item - ID_BASE, found_in_this_save(ctx, item, flags))
        ctx.queued_upto += 1


def share_checks(ctx):
    """Keeps the file of the server's checked locations up to date for the tracker and coords
    windows (rewritten when it changes, and every few seconds so they know the client is running)."""
    flags = frozenset(LOCATION_LIST[location_index(location)].flag for location in ctx.checked_locations
                      if is_location_index(location_index(location)))
    shared = (ctx.seed_name, ctx.auth, flags)
    if shared != ctx.shared_checks or time.time() - ctx.shared_at >= SERVER_CHECKS_HEARTBEAT:
        try:
            write_server_checks(ctx.seed_name or "", ctx.auth or "", flags)
        except OSError as e:
            logger.warning(f"Couldn't share checks with the tracker: {e}")
        ctx.shared_checks, ctx.shared_at = shared, time.time()


def check_goal(ctx):
    """Tells the server the goal is done once Chapter 8 reaches the final boss arena."""
    if ctx.goal_sent:
        return
    _, slots, _ = ctx.mailbox.story_state()
    if slots[GOAL_CHAPTER] >= GOAL_STORY_VALUE:
        ctx.goal_sent = True
        Utils.async_start(ctx.send_msgs([{"cmd": "StatusUpdate", "status": ClientStatus.CLIENT_GOAL}]))
        logger.info("Goal complete!")
