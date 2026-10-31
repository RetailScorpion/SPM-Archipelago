# AI usage disclosure

This statement covers the whole Super Paper Mario Archipelago project (this repository: the game mod, the APworld, the client, the patcher, the helper tools and the documentation). I, RetailScorpion, am the project's only developer until further notice, and I'm making this statement for all of it.

*Last updated: 2026-10-31*

## How AI was used

This project was made with heavy use of AI. **Claude** (Anthropic's AI model), used through Claude Code, wrote the code and did most of the technical work, directed by me:

- **Code:** essentially all of the code in this repository was written by Claude: the C++ mod that runs in the game, the APworld, the Archipelago client, the patcher, the tracker and the other tools in `client/` and `tools/`. Commits it wrote carry a `Co-Authored-By: Claude` line.
- **Research:** Claude did the reverse engineering the mod relies on: reading the game's disassembly and script data, finding addresses, and working out formats (for example the sound archive). It built on public research and tools by SeekyCt and others (see the README's credits).
- **Documentation:** Claude wrote the README, DESIGN.md, the setup guide and the release notes, from my instructions and decisions.

## What I did

- The design: what gets randomized, the options, how the game's story and progression are reworked, and every decision recorded in DESIGN.md.
- Directing the work, reviewing it, and deciding what goes in.
- Testing everything in the game and reporting what was needed to be changed or fixed.
- Mapping logic while playing (which locations need which abilities).

## Art

- **The pause menu's Options button** (`assets/pause/options_button.png`) was made by me, without AI.
- **The Archipelago item icon** (`assets/icons/ap_progression.png`) was made by me, without AI. The grey icon for filler items (`ap_filler.png`) is a straight greyscale conversion of it.
  - Early builds (up to and including 0.1.3) used a different icon, produced by a script Claude wrote that restyled the Archipelago logo. All versions from 0.1.4 onward use my icon, and that script and its images have been removed from the project.
- **The Save Block icon** (`assets/icons/save_block.png`) was put together by a script Claude wrote, from the game's own save block textures.
- `assets/icons/ap_blank.png` is an empty, fully transparent icon (shown when a trap gives nothing), so it contains no art.

From now on, all art in this project will be made by real people. When a feature needs an image, I'll either supply it or commission it, and Claude will only convert it to the game's format.

## In the game

Nothing in the mod, the client or the tools uses AI while you play. The AI was only used to make them.

I'll update this statement if how AI is used in the project changes.
