"""Party editor: add and remove characters, Pixls and the Save Block, and set coins, in the running game, for testing.

Writes the pouch's character and Pixl lists directly, the way pouchAddItem fills them, through Dolphin.
Only the inventory changes: no location flags, nothing sent to the Archipelago server.
  - A removed Pixl that was selected hands the selection to the first one left. The Pixl following
    Mario stays until the next map change (or until you pick another in the Pixl menu).
  - The character Mario is playing as can't be removed: switch to Mario first.
  - Mario himself is never listed.
  - The Save Block (key item 45) needs the mod version that has it; on older ones item 45 is a 7-3 prop.

Usage: py -3 -E party.py
"""

import struct
import tkinter as tk

import dolphin_memory_engine as dme

import theme
from tracker import MARIO_WORK, connect_dolphin, current_map

POUCH = 0x804CEA28          # MarioPouchWork (US 1.0)
COINS = POUCH + 0x1C        # s32
MAX_COINS = 999
KEY_ITEMS = POUCH + 0x20     # u16[32], 0 = empty
KEY_ITEM_SLOTS = 32
CHARACTERS = POUCH + 0xB4   # PouchCharOrPixlInfo[4]: u8 selectable, u8 selected, u16 itemType
PIXLS = POUCH + 0xC4        # PouchCharOrPixlInfo[16]
CHARACTER_SLOTS = 4
PIXL_SLOTS = 16
MARIO_CHARACTER = MARIO_WORK + 0x38  # s8: 0 Mario, 1 Peach, 2 Bowser, 3 Luigi
CHARACTER_ITEM_BASE = 216            # Mario 216, Peach 217, Bowser 218, Luigi 219

CHARACTER_NAMES = {217: "Peach", 218: "Bowser", 219: "Luigi"}
PIXL_NAMES = {221: "Thoreau", 222: "Boomer", 223: "Slim", 224: "Thudley", 225: "Carrie", 226: "Fleep",
              227: "Cudge", 228: "Dottie", 229: "Piccolo", 230: "Barry", 231: "Dashell"}
KEY_ITEM_NAMES = {45: "Save Block"}  # mod saveblock.h SAVE_BLOCK_ITEM_ID
REFRESH_MS = 500


def read_list(addr, count):
    """[(selectable, selected, itemType)] for every slot."""
    data = dme.read_bytes(addr, 4 * count)
    return [struct.unpack_from(">BBH", data, 4 * i) for i in range(count)]


def write_list(addr, entries, count):
    data = b"".join(struct.pack(">BBH", *e) for e in entries)
    dme.write_bytes(addr, data + b"\0" * (4 * (count - len(entries))))


def held(entries):
    return [e for e in entries if e[2] != 0]


def toggle_character(item, want):
    entries = held(read_list(CHARACTERS, CHARACTER_SLOTS))
    have = any(e[2] == item for e in entries)
    if want and not have:
        if len(entries) >= CHARACTER_SLOTS:
            return "No free character slot"
        entries.append((1, 1, item))  # what pouchAddItem writes for a character
    elif not want and have:
        if dme.read_bytes(MARIO_CHARACTER, 1)[0] == item - CHARACTER_ITEM_BASE:
            return f"You're playing as {CHARACTER_NAMES[item]}: switch to Mario first"
        entries = [e for e in entries if e[2] != item]
    write_list(CHARACTERS, entries, CHARACTER_SLOTS)
    return f"{CHARACTER_NAMES[item]} {'added' if want else 'removed'}"


def read_key_items():
    data = dme.read_bytes(KEY_ITEMS, 2 * KEY_ITEM_SLOTS)
    return [v for v in struct.unpack(f">{KEY_ITEM_SLOTS}H", data) if v != 0]


def toggle_key_item(item, want):
    items = read_key_items()
    if want and item not in items:
        if len(items) >= KEY_ITEM_SLOTS:
            return "No free key item slot"
        items.append(item)
    elif not want and item in items:
        items = [i for i in items if i != item]
    items += [0] * (KEY_ITEM_SLOTS - len(items))
    dme.write_bytes(KEY_ITEMS, struct.pack(f">{KEY_ITEM_SLOTS}H", *items))
    return f"{KEY_ITEM_NAMES[item]} {'added' if want else 'removed'}"


def set_coins(value):
    before = struct.unpack(">i", dme.read_bytes(COINS, 4))[0]
    dme.write_bytes(COINS, struct.pack(">i", value))
    return f"Coins: {before} -> {value} (the counter may show the old amount until it redraws)"


def toggle_pixl(item, want):
    entries = held(read_list(PIXLS, PIXL_SLOTS))
    have = any(e[2] == item for e in entries)
    note = ""
    if want and not have:
        if len(entries) >= PIXL_SLOTS:
            return "No free Pixl slot"
        # What pouchAddItem writes; the first Pixl also gets selected (none selected crashes the menu)
        entries.append((1, 0 if any(e[1] for e in entries) else 1, item))
    elif not want and have:
        was_selected = any(e[2] == item and e[1] for e in entries)
        entries = [e for e in entries if e[2] != item]
        if was_selected and entries:
            entries[0] = (entries[0][0], 1, entries[0][2])
            note = f"; {PIXL_NAMES.get(entries[0][2], entries[0][2])} selected (change maps to swap the one following you)"
    write_list(PIXLS, entries, PIXL_SLOTS)
    return f"{PIXL_NAMES[item]} {'added' if want else 'removed'}{note}"


def main():
    root = tk.Tk()
    theme.apply(root)
    root.title("SPM party")
    root.attributes("-topmost", True)
    font = ("Consolas", 12)
    status = tk.Label(root, font=font, anchor="w", justify="left", fg=theme.MUTED, wraplength=360)
    status.pack(fill="x", padx=8, pady=(8, 4))

    boxes = {}

    def section(title, names, toggle):
        frame = tk.LabelFrame(root, text=title, font=font, padx=6, pady=4)
        frame.pack(fill="x", padx=8, pady=4)
        for i, (item, name) in enumerate(names.items()):
            var = tk.BooleanVar()

            def clicked(item=item, var=var):
                try:
                    connect_dolphin()
                    status.config(text=toggle(item, var.get()))
                except RuntimeError as e:
                    status.config(text=str(e))
                refresh(schedule=False)

            box = tk.Checkbutton(frame, text=name, variable=var, font=font, command=clicked, anchor="w")
            box.grid(row=i // 3, column=i % 3, sticky="w", padx=4)
            boxes[item] = (var, box)

    section("Characters", CHARACTER_NAMES, toggle_character)
    section("Pixls", PIXL_NAMES, toggle_pixl)
    section("Key items", KEY_ITEM_NAMES, toggle_key_item)

    def coins_clicked():
        try:
            connect_dolphin()
            status.config(text=set_coins(MAX_COINS))
        except RuntimeError as e:
            status.config(text=str(e))

    coins = tk.Button(root, text=f"{MAX_COINS} coins", font=font, command=coins_clicked)
    coins.pack(anchor="w", padx=8, pady=(4, 8))

    def refresh(schedule=True):
        try:
            connect_dolphin()
            if not current_map():
                raise RuntimeError("No save loaded")
            items = {e[2] for e in held(read_list(CHARACTERS, CHARACTER_SLOTS))}
            pixls = held(read_list(PIXLS, PIXL_SLOTS))
            items |= {e[2] for e in pixls}
            items |= set(read_key_items())
            selected = next((e[2] for e in pixls if e[1]), None)
            for item, (var, box) in boxes.items():
                var.set(item in items)
                box.config(state="normal", fg=theme.HIGHLIGHT if item == selected else theme.TEXT)
            coins.config(state="normal")
            if status.cget("text").startswith("Waiting"):
                status.config(text="Ready (selected Pixl in red)")
        except RuntimeError as e:
            dme.un_hook()
            for var, box in boxes.values():
                box.config(state="disabled")
            coins.config(state="disabled")
            status.config(text=f"Waiting for the game: {e}")
        if schedule:
            root.after(REFRESH_MS, refresh)

    status.config(text="Waiting for the game")
    refresh()
    root.mainloop()


if __name__ == "__main__":
    main()
