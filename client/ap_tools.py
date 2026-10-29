"""SPM Archipelago Tools: add and remove characters, Pixls and the Save Block, and set coins, in the running game, for testing.

Writes the pouch's character and Pixl lists directly, the way pouchAddItem fills them, through Dolphin.
Only the inventory changes: no location flags, nothing sent to the Archipelago server.
  - A removed Pixl that was selected hands the selection to the first one left. The Pixl following
    Mario stays until the next map change (or until you pick another in the Pixl menu).
  - The character Mario is playing as can't be removed: switch to Mario first.
  - Mario himself is never listed.
  - The Save Block (key item 45) needs the mod version that has it; on older ones item 45 is a 7-3 prop.
  - Key item storage: the mod puts key items there when all 32 slots are full (mod keystorage.h, kept in
    the save). Move or swap them with carried ones here; stored items don't count as held.

Usage: pyw -3 -E ap_tools.py
"""

import json
import os
import struct
import tkinter as tk

import dolphin_memory_engine as dme

import theme
from mailbox import GP_PTR
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
GSW_BYTES = 0x544                    # SpmarioGlobals gsw[2048]
KEY_STORAGE_GSW = 1792 + 32          # mod savedata.h AP_GSW_KEY_STORAGE: u16[32], big-endian, 0 = empty
KEY_STORAGE_SLOTS = 32
ITEM_NAMES_FILE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "item_names_us0.json")
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


def write_key_items(items):
    items = [i for i in items if i]
    dme.write_bytes(KEY_ITEMS, struct.pack(f">{KEY_ITEM_SLOTS}H", *(items + [0] * (KEY_ITEM_SLOTS - len(items)))))


def key_storage_addr():
    return dme.read_word(GP_PTR) + GSW_BYTES + KEY_STORAGE_GSW


def read_key_storage():
    data = dme.read_bytes(key_storage_addr(), 2 * KEY_STORAGE_SLOTS)
    return [v for v in struct.unpack(f">{KEY_STORAGE_SLOTS}H", data) if v != 0]


def write_key_storage(items):
    items = [i for i in items if i]
    dme.write_bytes(key_storage_addr(), struct.pack(f">{KEY_STORAGE_SLOTS}H", *(items + [0] * (KEY_STORAGE_SLOTS - len(items)))))


def key_item_names():
    names = {int(k): v for k, v in json.load(open(ITEM_NAMES_FILE)).items()}
    names.update(KEY_ITEM_NAMES)
    return names


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
    root.title("SPM Archipelago Tools")
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

    # Key item storage: carried key items on the left, stored ones on the right
    names = key_item_names()
    store_frame = tk.LabelFrame(root, text="Key item storage", font=font, padx=6, pady=4)
    store_frame.pack(fill="both", expand=True, padx=8, pady=4)
    carry_label = tk.Label(store_frame, text="Carrying", font=font, anchor="w")
    carry_label.grid(row=0, column=0, sticky="w")
    store_label = tk.Label(store_frame, text="Storage", font=font, anchor="w")
    store_label.grid(row=0, column=2, sticky="w")
    carry_list = tk.Listbox(store_frame, font=font, height=10, width=22, exportselection=False)
    carry_list.grid(row=1, column=0, sticky="nsew")
    store_list = tk.Listbox(store_frame, font=font, height=10, width=22, exportselection=False)
    store_list.grid(row=1, column=2, sticky="nsew")
    store_frame.rowconfigure(1, weight=1)
    store_frame.columnconfigure(0, weight=1)
    store_frame.columnconfigure(2, weight=1)
    buttons = tk.Frame(store_frame)
    buttons.grid(row=1, column=1, padx=6)
    lists = {"carry": [], "store": []}

    def picked(listbox, items):
        sel = listbox.curselection()
        return sel[0] if sel and sel[0] < len(items) else None

    def storage_action(action):
        try:
            connect_dolphin()
            carry, store = read_key_items(), read_key_storage()
            c, s = picked(carry_list, carry), picked(store_list, store)
            if action == "store":
                if c is None:
                    raise RuntimeError("Pick a carried key item")
                if len(store) >= KEY_STORAGE_SLOTS:
                    raise RuntimeError("Storage is full")
                item = carry.pop(c)
                store.append(item)
                text = f"{names.get(item, item)} put in storage"
            elif action == "carry":
                if s is None:
                    raise RuntimeError("Pick a stored key item")
                if len(carry) >= KEY_ITEM_SLOTS:
                    raise RuntimeError("Key items are full: swap instead")
                item = store.pop(s)
                carry.append(item)
                text = f"{names.get(item, item)} taken out of storage"
            else:
                if c is None or s is None:
                    raise RuntimeError("Pick one on each side")
                carry[c], store[s] = store[s], carry[c]
                text = f"Swapped {names.get(store[s], store[s])} for {names.get(carry[c], carry[c])}"
            write_key_items(carry)
            write_key_storage(store)
            status.config(text=text)
        except RuntimeError as e:
            status.config(text=str(e))
        refresh(schedule=False)

    store_button = tk.Button(buttons, text="Store ->", font=font, command=lambda: storage_action("store"))
    store_button.pack(fill="x", pady=2)
    carry_button = tk.Button(buttons, text="<- Carry", font=font, command=lambda: storage_action("carry"))
    carry_button.pack(fill="x", pady=2)
    swap_button = tk.Button(buttons, text="Swap", font=font, command=lambda: storage_action("swap"))
    swap_button.pack(fill="x", pady=2)
    storage_widgets = (store_button, carry_button, swap_button, carry_list, store_list)

    def show_list(listbox, key, items, label, title, slots):
        label.config(text=f"{title} ({len(items)}/{slots})")
        if items == lists[key]:
            return
        lists[key] = items
        sel = listbox.curselection()
        listbox.delete(0, "end")
        for item in items:
            listbox.insert("end", names.get(item, f"item {item}"))
        if sel and sel[0] < len(items):
            listbox.selection_set(sel[0])

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
            for widget in storage_widgets:
                widget.config(state="normal")
            show_list(carry_list, "carry", read_key_items(), carry_label, "Carrying", KEY_ITEM_SLOTS)
            show_list(store_list, "store", read_key_storage(), store_label, "Storage", KEY_STORAGE_SLOTS)
            if status.cget("text").startswith("Waiting"):
                status.config(text="Ready (selected Pixl in red)")
        except RuntimeError as e:
            dme.un_hook()
            for var, box in boxes.values():
                box.config(state="disabled")
            coins.config(state="disabled")
            for widget in storage_widgets:
                widget.config(state="disabled")
            status.config(text=f"Waiting for the game: {e}")
        if schedule:
            root.after(REFRESH_MS, refresh)

    status.config(text="Waiting for the game")
    refresh()
    root.mainloop()


if __name__ == "__main__":
    main()
