# Wand, Scroll & Potion Actions — a mod for OpenYAMM 1.11

This branch (`wand-scroll-actions-1.11`) is [OpenYAMM](https://github.com/pjasicek/openyamm) **v1.11** with a set of
combat quality-of-life additions: a dedicated wand slot, one-click scroll and potion menus, and a reworked turn-based
button column that shows what your next attack will actually do.

It is an unofficial fork. Everything else plays exactly like the official 1.11 release.

## What's in it

### Dedicated wand slot

- Each character has a **wand slot behind the shoulder** on the paperdoll, separate from the main hand. Drop a wand
  on the doll and it goes there, so you can keep your sword in hand and still zap.
- **Use Wand** fires the equipped wand without swapping weapons (default key **N**). A wand held in the main hand
  still works as before.
- Wands in the new slot are saved with your game.

### Scroll and potion menus

- **Scrolls** (default **G**) and **Potions** (default **V**) buttons sit in the bottom-right corner, stacked. Each
  opens a compact list of every scroll or potion the party carries, with icons and stack counts.
- Clicking an entry uses it on the active character, wherever in the party's packs the item is. No more digging
  through four inventories mid-fight.
- The buttons show a random scroll or potion you actually carry, with a ▲ marker since the menu opens upwards.
- The mouse wheel scrolls long lists.

### Turn-based combat buttons

In turn-based mode the bottom-left corner shows a vertical column: **Attack**, **Pass** and **Wand**. Every button
shows an icon, a name and its hotkey, read from your current key bindings, so rebinding a key updates the label.

- **Attack** shows what your next attack will use:
  - the character's **attack spell**, using its coloured art from the spellbook on a parchment-coloured backing,
    while they have the mana to cast it;
  - otherwise their **bow** when no enemy is within melee reach, or their **melee weapon** when one is. The button
    switches live as monsters close in;
  - no icon when fighting bare-handed.
  During the movement phase it shows your remaining action points instead of a hotkey. Clicking it attacks, the same
  as the hotkey, against the nearest enemy. In the official game a click here left turn-based mode; the **Combat** key
  (Enter) still does that.
- **Pass** (default **B**) with Lord Kilburn's Shield, or **End Move** with MM6's leather boots during movement.
- **Wand** shows the equipped wand and, once the wand is identified, its remaining charges (red at 0). It drops into
  Pass's slot when Pass isn't available, so the column never has a gap.

All buttons share the turn indicator's plain dark plate and fonts, and the column stays clear of the portrait bar on
narrow screens.

### Bug fix

- **Life Drain (and other direct-hit spells) said "No valid target exists!" outdoors** when cast as an attack spell or
  quick spell at a monster that hadn't noticed the party yet. Outdoors now targets the monster you point at, as
  indoors already did.

### New key bindings

Settings → Controls has three new **Combat** bindings: *Open scrolls menu* (G), *Open potions menu* (V) and *Use
equipped wand* (N).

## Install (Windows, prebuilt)

You need the official **OpenYAMM 1.11** for Windows installed and started at least once, so it has downloaded its game
content.

1. Download `OpenYAMM-wand-scroll-mod-1.11.zip` from this fork's
   [Releases page](https://github.com/puzzlern/openyamm/releases).
2. Close the game. In your OpenYAMM folder (the one with `OpenYAMM.exe`, `settings.ini` and `saves`), open
   `app\1.11\` and **rename** `openyamm.exe` to `openyamm.exe.orig`. That's your way back.
3. Extract the zip **into your OpenYAMM folder**, merging folders. It adds:
   ```
   OpenYAMM\
     app\1.11\openyamm.exe          the modded game
     app\1.11\libgcc_s_seh-1.dll    runtime libraries the modded game needs
     app\1.11\libstdc++-6.dll
     app\1.11\libwinpthread-1.dll
     mods\engine\ui\gameplay\gameplay.yml       button and menu layout
     mods\engine\ui\gameplay\character.yml      paperdoll wand slot
     mods\engine\ui\menu\settings_catalog.yml   the three new key bindings
   ```
4. Start the game with `OpenYAMM.exe` as usual. If you use a shortcut, point it at `OpenYAMM.exe` with **Start in**
   set to the OpenYAMM folder, because the game looks for `mods` in the folder it starts from.

To check it worked, open `openyamm.log` in the OpenYAMM folder. It should list three `Mods: … replaces the game's file`
lines.

### Before you install: read this

- **Back up your `saves` folder.** Games saved with the mod record the wand slot, so the official 1.11 can't load them.
  Your existing saves load fine in the mod.
- **Game updates remove the mod.** When OpenYAMM updates (to 1.12 and so on), the launcher installs a fresh official
  version and the modded `openyamm.exe` is gone. The `mods` folder stays behind, though, and would apply 1.11 layouts
  to a newer game, so **delete the `mods` folder after an update** until a matching mod release is out. To keep
  playing the mod meanwhile, set `check_for_updates=off` under `[updates]` in `settings.ini`.

### Uninstall

Close the game, delete `app\1.11\openyamm.exe` and the three DLLs next to it, rename `openyamm.exe.orig` back to
`openyamm.exe`, and delete the `mods` folder (or only the three files above, if you have other mods).

## Build from source (Windows)

You need [MSYS2](https://www.msys2.org/) with the UCRT64 toolchain:

```
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja make git
```

Then, in an **MSYS2 UCRT64** shell. A plain MSYS shell won't do, because the bundled FFmpeg refuses to configure there.

```
git clone -b wand-scroll-actions-1.11 https://github.com/puzzlern/openyamm.git
cd openyamm
cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DOPENYAMM_BUILD_EDITOR=OFF -DOPENYAMM_BUILD_TESTS=OFF -DOPENYAMM_BUILD_TOOLS=OFF
cmake --build build-ucrt64 --target openyamm
```

The first build downloads and compiles its dependencies and takes a while. The result is
`build-ucrt64/game/openyamm.exe`. Install it as in steps 2–4 above, taking the three DLLs from
`C:\msys64\ucrt64\bin` and the YAML files from `assets_dev/engine/ui/` (`gameplay/gameplay.yml`,
`gameplay/character.yml`, `menu/settings_catalog.yml`).

## Credits and license

OpenYAMM is by [pjasicek](https://github.com/pjasicek/openyamm) and contributors; this mod only adds to it. Like
OpenYAMM, it is licensed under the GNU GPL v3; see [LICENSE](LICENSE). The source of every change is in this branch's
commits on top of the `v1.11` tag.
