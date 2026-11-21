# Super Paper Mario

## What is randomized?
Depending on your options: the story Pixls, the optional Pixls (Barry, Dashell, Piccolo), key items (every chapter's keys, 7-4's orbs, the tablets, Cooking Disks and the Golden Card), items in chests and lying around, catch cards, the Pure Hearts, the Save Block, and Peach, Luigi and Bowser. You can start as someone other than Mario, add a check for tattling every kind of enemy, make coins need Progressive Wallets, and replace some filler with Cursya Traps.

Some chapters' doors are open from the start (`starting_chapter_mode`). The others open when you receive their Pure Heart. The Pit Keys, the Old Key, the Random House Key, ? blocks, shops and the Pit rewards are still vanilla.

## Is there logic?
It's a work in progress. With `logic: wip_logic` (the default), items are placed using the logic mapped out so far, which isn't complete yet, so a seed will probably need something it doesn't say. With `logic: no_logic`, only the rules the game itself enforces are used (chapter doors, Flopside, Chapter 8's party), so items inside a chapter can be anywhere. Either way, the Return Pipe gets you out of rooms you can't leave.

## What is the goal?
Finish Chapter 8. What opens Chapter 8's door and what the final battle needs are set by `castle_requirement` and `final_requirement`.

## What does another world's item look like?
The Archipelago icon (grey for filler). Picking it up shows "Sent <item> to <player>!". Tattling it with Tippi tells you what it is and who it's for, and hints it.
