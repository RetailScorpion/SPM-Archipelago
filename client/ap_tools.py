"""SPM Archipelago Tools: change your inventory in the running game, for testing.

  - add or remove characters, Pixls and the Save Block
  - set coins to 999
  - key item storage: move key items between your 32 slots and the mod's storage, or swap them

It writes the inventory directly through Dolphin, the same way the game's own "add item" fills it.
Only the inventory changes: no location flags, and nothing is sent to the Archipelago server.
  - A removed Pixl that was selected hands the selection to the first one left. The Pixl following
    Mario stays until the next map change (or until you pick another in the Pixl menu).
  - The character Mario is playing as can't be removed: switch to Mario first. Mario himself isn't
    listed.
  - Key item storage: the mod puts key items there when all 32 slots are full (mod
    key_item_storage.cpp; kept in the save). Stored items don't count as held.

Usage: pyw -3 -E ap_tools.py
"""

import json
import os
import struct
import tkinter as tk

import dolphin_memory_engine as dme

import theme
from game_memory import MARIO_WORK, connect_dolphin, current_map
from mailbox import GP_PTR

# ---- Where things are in the game's memory (US 1.0) ----

POUCH = 0x804CEA28                   # the inventory (MarioPouchWork)
COINS = POUCH + 0x1C                 # 4-byte number
MAX_COINS = 999
KEY_ITEMS = POUCH + 0x20             # 32 key item slots, 2 bytes each, 0 = empty
KEY_ITEM_SLOTS = 32
CHARACTERS = POUCH + 0xB4            # 4 character slots of 4 bytes: can-select, selected, item id
PIXLS = POUCH + 0xC4                 # 16 Pixl slots, the same way
CHARACTER_SLOTS = 4
PIXL_SLOTS = 16
MARIO_CHARACTER = MARIO_WORK + 0x38  # who Mario is playing as: 0 Mario, 1 Peach, 2 Bowser, 3 Luigi
CHARACTER_ITEM_BASE = 216            # characters as items: Mario 216, Peach 217, Bowser 218, Luigi 219
GSW_BYTES = 0x544                    # the save bytes, in the game's main state
KEY_STORAGE_GSW = 1792 + 32          # key item storage (mod save_data.h AP_GSW_KEY_STORAGE): 32 x 2 bytes
KEY_STORAGE_SLOTS = 32

CHARACTER_NAMES = {217: "Peach", 218: "Bowser", 219: "Luigi"}
PIXL_NAMES = {221: "Thoreau", 222: "Boomer", 223: "Slim", 224: "Thudley", 225: "Carrie", 226: "Fleep",
              227: "Cudge", 228: "Dottie", 229: "Piccolo", 230: "Barry", 231: "Dashell"}
KEY_ITEM_NAMES = {45: "Save Block"}  # mod save_block.h SAVE_BLOCK_ITEM_ID
ITEM_NAMES_FILE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "item_names_us0.json")
REFRESH_MS = 500

# ---- Characters and Pixls ----


def read_list(addr, count):
    """A character or Pixl list: [(can-select, selected, item id)] for every slot."""
    data = dme.read_bytes(addr, 4 * count)
    return [struct.unpack_from(">BBH", data, 4 * i) for i in range(count)]


def write_list(addr, entries, count):
    """Writes a character or Pixl list, with empty slots after the entries."""
    data = b"".join(struct.pack(">BBH", *e) for e in entries)
    dme.write_bytes(addr, data + b"\0" * (4 * (count - len(entries))))


def held(entries):
    """The slots of a list that hold something."""
    return [e for e in entries if e[2] != 0]


def toggle_character(item, want):
    """Adds (want = True) or removes a character. Returns a message for the window."""
    entries = held(read_list(CHARACTERS, CHARACTER_SLOTS))
    have = any(e[2] == item for e in entries)
    if want and not have:
        if len(entries) >= CHARACTER_SLOTS:
            return "No free character slot"
        entries.append((1, 1, item))  # what the game's "add item" writes for a character
    elif not want and have:
        if dme.read_bytes(MARIO_CHARACTER, 1)[0] == item - CHARACTER_ITEM_BASE:
            return f"You're playing as {CHARACTER_NAMES[item]}: switch to Mario first"
        entries = [e for e in entries if e[2] != item]
    write_list(CHARACTERS, entries, CHARACTER_SLOTS)
    return f"{CHARACTER_NAMES[item]} {'added' if want else 'removed'}"


def toggle_pixl(item, want):
    """Adds or removes a Pixl, keeping one selected. Returns a message for the window."""
    entries = held(read_list(PIXLS, PIXL_SLOTS))
    have = any(e[2] == item for e in entries)
    note = ""
    if want and not have:
        if len(entries) >= PIXL_SLOTS:
            return "No free Pixl slot"
        # What the game's "add item" writes; the first Pixl also gets selected (none selected crashes the menu)
        entries.append((1, 0 if any(e[1] for e in entries) else 1, item))
    elif not want and have:
        was_selected = any(e[2] == item and e[1] for e in entries)
        entries = [e for e in entries if e[2] != item]
        if was_selected and entries:
            entries[0] = (entries[0][0], 1, entries[0][2])
            note = f"; {PIXL_NAMES.get(entries[0][2], entries[0][2])} selected (change maps to swap the one following you)"
    write_list(PIXLS, entries, PIXL_SLOTS)
    return f"{PIXL_NAMES[item]} {'added' if want else 'removed'}{note}"


# ---- Key items and coins ----


def pack_items(items, slots):
    """A list of item ids as slot data: the items, then empty (0) slots."""
    items = [i for i in items if i]
    return struct.pack(f">{slots}H", *(items + [0] * (slots - len(items))))


def read_key_items():
    """The key items Mario carries."""
    data = dme.read_bytes(KEY_ITEMS, 2 * KEY_ITEM_SLOTS)
    return [v for v in struct.unpack(f">{KEY_ITEM_SLOTS}H", data) if v != 0]


def write_key_items(items):
    """Sets the key items Mario carries."""
    dme.write_bytes(KEY_ITEMS, pack_items(items, KEY_ITEM_SLOTS))


def toggle_key_item(item, want):
    """Adds or removes a key item (the Save Block). Returns a message for the window."""
    items = read_key_items()
    if want and item not in items:
        if len(items) >= KEY_ITEM_SLOTS:
            return "No free key item slot"
        items.append(item)
    elif not want and item in items:
        items = [i for i in items if i != item]
    write_key_items(items)
    return f"{KEY_ITEM_NAMES[item]} {'added' if want else 'removed'}"


def set_coins(value):
    """Sets Mario's coins. Returns a message for the window."""
    before = struct.unpack(">i", dme.read_bytes(COINS, 4))[0]
    dme.write_bytes(COINS, struct.pack(">i", value))
    return f"Coins: {before} -> {value} (the counter may show the old amount until it redraws)"


def key_storage_addr():
    """Where key item storage is (inside the save bytes, which move with the game's main state)."""
    return dme.read_word(GP_PTR) + GSW_BYTES + KEY_STORAGE_GSW


def read_key_storage():
    """The key items in storage."""
    data = dme.read_bytes(key_storage_addr(), 2 * KEY_STORAGE_SLOTS)
    return [v for v in struct.unpack(f">{KEY_STORAGE_SLOTS}H", data) if v != 0]


def write_key_storage(items):
    """Sets the key items in storage."""
    dme.write_bytes(key_storage_addr(), pack_items(items, KEY_STORAGE_SLOTS))


def key_item_names():
    """Every item's name, by id (from data/item_names_us0.json), with the Save Block's."""
    names = {int(k): v for k, v in json.load(open(ITEM_NAMES_FILE)).items()}
    names.update(KEY_ITEM_NAMES)
    return names


def move_key_item(action, carry, store, carry_pick, store_pick, names):
    """Changes the two lists for one storage button: "store" (carried -> storage), "carry" (storage ->
    carried) or "swap". carry_pick and store_pick are the picked positions (or None). Returns a
    message, or raises RuntimeError saying what's missing."""
    if action == "store":
        if carry_pick is None:
            raise RuntimeError("Pick a carried key item")
        if len(store) >= KEY_STORAGE_SLOTS:
            raise RuntimeError("Storage is full")
        item = carry.pop(carry_pick)
        store.append(item)
        return f"{names.get(item, item)} put in storage"
    if action == "carry":
        if store_pick is None:
            raise RuntimeError("Pick a stored key item")
        if len(carry) >= KEY_ITEM_SLOTS:
            raise RuntimeError("Key items are full: swap instead")
        item = store.pop(store_pick)
        carry.append(item)
        return f"{names.get(item, item)} taken out of storage"
    if carry_pick is None or store_pick is None:
        raise RuntimeError("Pick one on each side")
    carry[carry_pick], store[store_pick] = store[store_pick], carry[carry_pick]
    return f"Swapped {names.get(store[store_pick], store[store_pick])} for {names.get(carry[carry_pick], carry[carry_pick])}"


# ---- The window ----


class ToolsWindow:
    """The SPM Archipelago Tools window."""

    def __init__(self):
        self.root = tk.Tk()
        theme.apply(self.root)
        self.root.title("SPM Archipelago Tools")
        self.font = ("Consolas", 12)
        self.names = key_item_names()
        self.boxes = {}                          # item -> (its checkbox's value, the checkbox)
        self.lists = {"carry": [], "store": []}  # what the two storage lists show

        self.status = tk.Label(self.root, font=self.font, anchor="w", justify="left", fg=theme.MUTED, wraplength=360)
        self.status.pack(fill="x", padx=8, pady=(8, 4))
        self.add_checkboxes("Characters", CHARACTER_NAMES, toggle_character)
        self.add_checkboxes("Pixls", PIXL_NAMES, toggle_pixl)
        self.add_checkboxes("Key items", KEY_ITEM_NAMES, toggle_key_item)
        self.coins = tk.Button(self.root, text=f"{MAX_COINS} coins", font=self.font, command=self.coins_clicked)
        self.coins.pack(anchor="w", padx=8, pady=(4, 8))
        self.add_storage()

    def add_checkboxes(self, title, names, toggle):
        """A section of checkboxes, one per item: ticking adds it, unticking removes it."""
        frame = tk.LabelFrame(self.root, text=title, font=self.font, padx=6, pady=4)
        frame.pack(fill="x", padx=8, pady=4)
        for i, (item, name) in enumerate(names.items()):
            var = tk.BooleanVar()
            box = tk.Checkbutton(frame, text=name, variable=var, font=self.font, anchor="w",
                                 command=lambda item=item, var=var: self.checkbox_clicked(toggle, item, var))
            box.grid(row=i // 3, column=i % 3, sticky="w", padx=4)
            self.boxes[item] = (var, box)

    def checkbox_clicked(self, toggle, item, var):
        try:
            connect_dolphin()
            self.status.config(text=toggle(item, var.get()))
        except RuntimeError as e:
            self.status.config(text=str(e))
        self.refresh(schedule=False)

    def coins_clicked(self):
        try:
            connect_dolphin()
            self.status.config(text=set_coins(MAX_COINS))
        except RuntimeError as e:
            self.status.config(text=str(e))

    def add_storage(self):
        """The key item storage section: carried key items on the left, stored ones on the right, and
        the three buttons between them."""
        frame = tk.LabelFrame(self.root, text="Key item storage", font=self.font, padx=6, pady=4)
        frame.pack(fill="both", expand=True, padx=8, pady=4)
        self.carry_label = tk.Label(frame, text="Carrying", font=self.font, anchor="w")
        self.carry_label.grid(row=0, column=0, sticky="w")
        self.store_label = tk.Label(frame, text="Storage", font=self.font, anchor="w")
        self.store_label.grid(row=0, column=2, sticky="w")
        self.carry_list = tk.Listbox(frame, font=self.font, height=10, width=22, exportselection=False)
        self.carry_list.grid(row=1, column=0, sticky="nsew")
        self.store_list = tk.Listbox(frame, font=self.font, height=10, width=22, exportselection=False)
        self.store_list.grid(row=1, column=2, sticky="nsew")
        frame.rowconfigure(1, weight=1)
        frame.columnconfigure(0, weight=1)
        frame.columnconfigure(2, weight=1)

        buttons = tk.Frame(frame)
        buttons.grid(row=1, column=1, padx=6)
        storage_buttons = []
        for text, action in (("Store ->", "store"), ("<- Carry", "carry"), ("Swap", "swap")):
            button = tk.Button(buttons, text=text, font=self.font, command=lambda action=action: self.storage_clicked(action))
            button.pack(fill="x", pady=2)
            storage_buttons.append(button)
        self.storage_widgets = (*storage_buttons, self.carry_list, self.store_list)

    @staticmethod
    def picked(listbox, items):
        """The picked position in a list, or None."""
        sel = listbox.curselection()
        return sel[0] if sel and sel[0] < len(items) else None

    def storage_clicked(self, action):
        try:
            connect_dolphin()
            carry, store = read_key_items(), read_key_storage()
            text = move_key_item(action, carry, store, self.picked(self.carry_list, carry),
                                 self.picked(self.store_list, store), self.names)
            write_key_items(carry)
            write_key_storage(store)
            self.status.config(text=text)
        except RuntimeError as e:
            self.status.config(text=str(e))
        self.refresh(schedule=False)

    def show_list(self, listbox, key, items, label, title, slots):
        """Updates a storage list (only if it changed, so the pick stays)."""
        label.config(text=f"{title} ({len(items)}/{slots})")
        if items == self.lists[key]:
            return
        self.lists[key] = items
        sel = listbox.curselection()
        listbox.delete(0, "end")
        for item in items:
            listbox.insert("end", self.names.get(item, f"item {item}"))
        if sel and sel[0] < len(items):
            listbox.selection_set(sel[0])

    def set_enabled(self, enabled):
        """Turns every control on or off."""
        state = "normal" if enabled else "disabled"
        for _, box in self.boxes.values():
            box.config(state=state)
        self.coins.config(state=state)
        for widget in self.storage_widgets:
            widget.config(state=state)

    def refresh(self, schedule=True):
        """Shows the game's current inventory (every half second, and after each change)."""
        try:
            connect_dolphin()
            if not current_map():
                raise RuntimeError("No save loaded")
            items = {e[2] for e in held(read_list(CHARACTERS, CHARACTER_SLOTS))}
            pixls = held(read_list(PIXLS, PIXL_SLOTS))
            items |= {e[2] for e in pixls}
            items |= set(read_key_items())
            selected = next((e[2] for e in pixls if e[1]), None)
            for item, (var, box) in self.boxes.items():
                var.set(item in items)
            self.set_enabled(True)
            for item, (var, box) in self.boxes.items():
                box.config(fg=theme.HIGHLIGHT if item == selected else theme.TEXT)  # the selected Pixl in red
            self.show_list(self.carry_list, "carry", read_key_items(), self.carry_label, "Carrying", KEY_ITEM_SLOTS)
            self.show_list(self.store_list, "store", read_key_storage(), self.store_label, "Storage", KEY_STORAGE_SLOTS)
            if self.status.cget("text").startswith("Waiting"):
                self.status.config(text="Ready (selected Pixl in red)")
        except RuntimeError as e:
            dme.un_hook()
            self.set_enabled(False)
            self.status.config(text=f"Waiting for the game: {e}")
        if schedule:
            self.root.after(REFRESH_MS, self.refresh)

    def run(self):
        self.status.config(text="Waiting for the game")
        self.refresh()
        self.root.mainloop()


def main():
    ToolsWindow().run()


if __name__ == "__main__":
    main()
