# PolyBrik

PolyBrik runs the Polyend Tracker firmware on the **TrimUI Brick** and **TrimUI Brick Pro** handhelds. It is a port of the open source [Polyend Tracker Community Edition](https://github.com/polyend/tracker-ce) (Tracker CE) firmware, built as the Tracker Mini variant, with the Polyend synth engines, stereo samples and the Tracker Mini audio engine.

The firmware itself is unchanged in spirit: the pages, the pattern editor, the synths and the file formats are Polyend's. PolyBrik adds what a handheld without the Tracker's keyboard needs: a key map for the Brick's buttons, an on-screen action list for the eight screen keys the Mini has under its display, a legend strip that shows what every button does, and a display and audio layer for Linux.

PolyBrik is a community project. It is **not affiliated with, endorsed by or supported by Polyend** or by TrimUI.

## Contents

1. [Status](#status)
2. [Credits and upstream](#credits-and-upstream)
3. [Licenses](#licenses)
4. [Installing](#installing)
5. [Building from source](#building-from-source)
6. [Manual](#manual)
7. [Brick and Brick Pro](#brick-and-brick-pro)
8. [Known limitations](#known-limitations)
9. [Maintenance](#maintenance)
10. [How this was made](#how-this-was-made)

## Status

- Tested on the **TrimUI Brick Pro** running **NextUI**. The plain TrimUI Brick has not been tested on hardware yet; the key map supports it.
- Built as a 32-bit (armhf) static binary with the Tracker Mini firmware flags, so projects, instruments and synth patches use the Tracker Mini formats.
- Supported: every firmware page, the synth engines the Tracker Mini firmware offers (ACD, FAT, VAP, WTFM, PERC, DIRT and the open source CHIP), stereo samples, sample import, sample editing and the sample editor effects, export and render, projects and patches on the SD card.
- Not supported: MIDI (USB and DIN), USB audio and USB storage, and audio input (the sample recorder records silence). See [Known limitations](#known-limitations).

## Credits and upstream

- **Polyend Tracker Community Edition**: the firmware, synth engines and file formats, by Polyend Sp. z o.o. and the Tracker CE contributors. Source: <https://github.com/polyend/tracker-ce>. The firmware is vendored in this repository unchanged except for the changes listed in `platforms/sdl/PATCHES.md`; every change is wrapped in `#ifdef PLATFORM_SDL`, so upstream updates merge cleanly.
- **PolyBrik factory samples**: recorded/synthesized by Burç Tuncer or CC0.
- **OpenTracker preset pack**: 82 synth patches (the `OT ...` patches) from [OpenTracker](https://opentracker-fw.github.io) 2.2.5, community firmware for the original Polyend Tracker, by Marcin Spoczynski and the OpenTracker contributors (<https://github.com/sandlbn/polyend-tracker-re>). Included unchanged under the Apache License 2.0. OpenTracker is not affiliated with PolyBrik. The patches were voiced on OpenTracker's own engines; on the Polyend engines in PolyBrik they load and play, but can sound somewhat different.
- **TrimUI Brick / Brick Pro and NextUI**: the hardware and the launcher PolyBrik runs on. NextUI is a community firmware for TrimUI devices.

"Polyend", "Polyend Tracker" and "Tracker Mini" are trademarks of Polyend Sp. z o.o. "TrimUI" is a trademark of its owner. They are used here only to say what PolyBrik is compatible with.

## Licenses

PolyBrik combines code and content under several licenses. In short: **PolyBrik is free, it may be shared for free, and it may not be sold.** The details:

| Part | License | What it means for you |
|---|---|---|
| Tracker CE firmware (`tracker-ce/`) | Polyend Tracker Community Edition License: MIT, with a Wholesale Commercial Clone Restriction (`tracker-ce/LICENSE`) | You may use, copy and modify it. You may not sell the Tracker application or a port of it as a whole, sell firmware builds, or use Polyend's trademarks in a commercial product. Making and selling **music** with it is expressly allowed. |
| PolyBrik platform code (`platforms/sdl/`) | Same license as Tracker CE (`tracker-ce/LICENSE`) | As above. |
| Polyend synth and DSP libraries (`tracker-ce/synth/lib/libPolySynth.a`, `libPolyDSP.a`) | Polyend Precompiled Synthesizer & DSP Library License (`tracker-ce/synth/lib/LICENSE-PROPRIETARY.txt`) | Free to use and redistribute unmodified in free, non-commercial projects. No fee may be charged for builds that contain them. They may not be decompiled. |
| PolyBrik Factory samples (`Samples/PolyBrik Factory`) | PolyBrik Factory Samples License (`Samples/PolyBrik Factory/LICENSE.txt`) | Use them in your music, commercial or not, with no credit required. Share the pack for free, unchanged. Do not sell or repackage the samples. |
| OpenTracker preset pack (`Patches/<ENGINE>/OT ...`) | Apache License 2.0 (`Patches/OpenTracker-LICENSE.txt`) | Free to use, change and share, commercially too, keeping the license and the credit. |
| Third-party components in the firmware | Their own licenses, listed in `tracker-ce/NOTICE.md` | |
| SDL 2.30.8 (statically linked) | zlib license (`licenses/SDL2-zlib.txt`) | |
| alsa-lib 1.1.9 (statically linked) | GNU LGPL 2.1 (`licenses/LGPL-2.1.txt`) | See the note below. |
| GNU C library 2.28 (statically linked) | GNU LGPL 2.1 (`licenses/LGPL-2.1.txt`) | See the note below. |
| GCC runtime libraries (statically linked) | GPL 3 with the GCC Runtime Library Exception | No obligations for the binary. |

**LGPL note.** The 32-bit PolyBrik binary links alsa-lib and the GNU C library statically. The LGPL lets you replace those libraries in the program you received. PolyBrik meets this by publishing its complete source code, build scripts and the recipe for its toolchain in this repository: you can rebuild the binary with your own versions of these libraries (see [Building from source](#building-from-source); `platforms/sdl/t32/deps.sh` downloads the exact upstream alsa-lib and SDL sources, and `platforms/sdl/t32/toolchain/` builds the GNU C library as part of the toolchain).

**No warranty.** PolyBrik is provided as is, without warranty of any kind. Use it at your own risk. Back up your SD card.

## Installing

PolyBrik is a NextUI pak.

1. Install [NextUI](https://github.com/LoveRetro/NextUI) on your Brick or Brick Pro if it is not there yet.
2. Copy the `PolyBrik.pak` folder into `Tools/tg5040/` on the SD card, so you have `/Tools/tg5040/PolyBrik.pak/launch.sh` and `tracker-sdl` next to it.
3. On the device, open **Tools** in NextUI and start **PolyBrik**.

### Your files

On first launch PolyBrik creates a `PolyBrik` folder at the root of the SD card and keeps everything there: projects, the workspace, samples, synth patches, themes and settings. Put your own samples into `PolyBrik/Samples` (any folder layout you like) and your synth patches into `PolyBrik/Patches/<ENGINE>/` (for example `Patches/WTFM/`), next to the included `OT ...` presets.

### Updating

Replace the `PolyBrik.pak` folder with the new one. Your `PolyBrik` folder is never touched by an update, with one exception: the **`Samples/PolyBrik Factory`** folder belongs to the app. When an update ships a new version of the factory pack, that folder is replaced as a whole, so fixed or removed factory samples reach everyone. **Keep your own samples outside `PolyBrik Factory`.**

The included synth presets are only ever added: an update copies a preset into `Patches/<ENGINE>/` when no file of that name is there, and never overwrites or deletes a patch, so your own patches and your edited copies of the presets are safe.

### Quitting

Hold **Select + Start** for about two seconds. The firmware saves and PolyBrik returns to NextUI.

## Building from source

You only need this to change PolyBrik; to use it, take the release zip. Everything below is for macOS with [Homebrew](https://brew.sh) (the platform it was built on). A Linux host should work with the equivalent packages; it has not been tried.

### 1. Desktop build (for development and tests)

```sh
brew install git cmake gcc sdl2-compat
git clone <this repository> polybrik && cd polybrik
cmake -S platforms/sdl -B build-host -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc-16 -DCMAKE_CXX_COMPILER=g++-16
cmake --build build-host -j
./build-host/tests/actionlist_test          # and the other tests in build-host/tests
mkdir -p sdcard && ./build-host/tracker-sdl --sd sdcard
```

Apple clang cannot compile the Tracker firmware, so the build uses Homebrew GCC (`gcc-16`; use the version Homebrew installs). The desktop app opens a window with the firmware and the legend; the keyboard stands in for the Brick's buttons.

**The desktop build is not the release build.** It is a native 64-bit program and cannot link Polyend's 32-bit synth libraries, so it is built without the Tracker Mini's `PLATFORM_IMX` and `FEATURE_SYNTH` features. Compared with PolyBrik on the device it has **no synths** (no synth instrument slots), loads **stereo samples as their left channel only**, and leaves out the rest of the Mini's stereo audio paths. The sample memory is the same 60 MB as the Mini. Use it for the user interface, the key map, the action lists and the tests; check synth and stereo work on the device build.

### 2. The 32-bit device build (the release)

1. **Build the cross toolchain once** (GCC 10.3, glibc 2.28, about 25 minutes): follow `platforms/sdl/t32/toolchain/README.md`. It lists the Homebrew packages (`crosstool-ng` and its helpers), the crosstool-NG configuration and the two small macOS patches.
2. **Build the dependencies** (static SDL 2.30.8 and alsa-lib 1.1.9, downloaded from their upstream sites):
   ```sh
   export T32_PREFIX=$HOME/x-tools/arm-gcc10-linux-gnueabihf/bin/arm-gcc10-linux-gnueabihf- T32_GLIBC=sysroot
   sh platforms/sdl/t32/deps.sh build-t32
   ```
3. **Build PolyBrik and stage the pak:**
   ```sh
   cmake -S platforms/sdl -B build-t32/app -DCMAKE_TOOLCHAIN_FILE=$PWD/platforms/sdl/cmake/t32-armhf.cmake -DT32_DEPS=$PWD/build-t32/deps
   cmake --build build-t32/app -j
   sh platforms/sdl/pak/deploy-t32.sh --stage-only build-t32/app
   ```
   The pak is staged in `build-t32app/pak/PolyBrik.pak`. Without `--stage-only` the script also copies it to a Brick over SSH: set `BRICK_HOST` to its address or SSH alias (default `brick`).

This is also how the LGPL libraries can be replaced: change the versions in `platforms/sdl/t32/deps.sh` (or the glibc in the toolchain) and rebuild.

## Manual

This manual covers what is different on the Brick. For the firmware itself (the pages, the pattern editor, the synths, the sample tools) see the Polyend Tracker Mini manual and the Tracker CE documentation: every page works as described there, only the buttons are mapped differently.

### The screen

- **Top:** the firmware's 800 x 480 screen, stretched to the width of the Brick's display.
- **Bottom:** the **legend strip**. It names what every button does right now. Hold a modifier (Y, A, L4 or Menu) and the buttons that change meaning turn amber with their new names. The last line shows the page name and a hint.
- **Screen keys row:** the row just above the legend, where the Mini shows the labels of its eight screen keys. On PolyBrik this row is the **action list** (see [R4: the action list](#r4-the-action-list)). It is always visible, tinted dark, with live values; R4 lights it up.
- **L4 + Menu** switches the firmware screen between stretched and 1:1 (800 x 480, centred, sitting on the legend strip). Stretched is the default.

### Buttons at a glance

**L4 and R4** in this manual are the **two buttons under the screen**. The Brick Pro labels them L4 and R4; on the plain Brick the same two buttons are labelled **L3 and R3**. The legend strip uses each device's own labels.


| Button | Alone | With L4 held |
|---|---|---|
| D-pad | move the cursor | Up Fn1, Right Fn2, Down Fn3, Left Fn4 |
| B (south) | Enter | Redo (Pattern page) |
| A (east) | column key | |
| Y (west) | Shift | Shift |
| X (north) | Delete | Undo (Pattern page) |
| L1 / L2 / R2 / R1 | value -1 / -10 / +10 / +1 | Copy / Paste / Insert / Preview (Pattern page) |
| Start | Play | Play |
| Select | Rec | Rec |
| Menu | Menu (hold for the page grid) | screen scale: stretched / 1:1 |
| L4 (left button under the screen) | edit layer (hold) | |
| R4 (right button under the screen) | action list | |

On the Brick Pro the sticks add more; see [Brick and Brick Pro](#brick-and-brick-pro).

### Moving and editing in the pattern

- **D-pad Left / Right** walks through the columns of a track (Note, Instrument, FX1, FX2) before it moves to the next track, so every column is reachable with the D-pad alone.
- **Up / Down** moves through the steps.
- **B** is Enter: it enters a note or confirms, exactly as Enter on the Mini, including its hold behaviour.
- **X** is Delete. **Y + X** (Shift + Delete) is backspace.
- **The shoulders are the value keys**, in their physical order from left to right: **L1 = -1, L2 = -10, R2 = +10, R1 = +1**. They change the value under the cursor, as the Mini's value keys and the Tracker+'s jog wheel do. On lists, **+1 moves down the list**.
- **Y + a shoulder** (Shift + a value key) changes the master volume, as on the Mini.

### A: the column key

The Mini has four keys for the pattern columns: Note, Instr, FX1 and FX2. On the Brick, **A is the key of the column the cursor is in**: in the Note column it is the Note key, in the FX1 column the FX1 key, and so on. Everything the Mini does with these keys works with A:

- Hold **A** on an instrument or FX cell to open its list.
- **Y + A** (Shift + the column key) opens the note keyboard popup in the Note column.
- Hold **A** and press **L4 + L2** (Paste) to paste into that column only, or **X** (Delete) to delete that column only.

On the Brick Pro the left stick also gives you all four column keys directly.

### Y: Shift

**Y is the Shift key.** Wherever the Mini manual says Shift, press Y. Hold it and the legend shows its shortcuts, among them: **Y + D-pad** selects, **Y + a shoulder** changes the master volume, **Y + X** is backspace, **Y + Start** plays the song, **Y + Select** arms recording.

### L4: the edit layer

Hold **L4** and some buttons change meaning. The legend shows them in amber while L4 is held.

| L4 + | Does |
|---|---|
| L1 | Copy |
| L2 | Paste |
| R2 | Insert (Y + L4 + R2: Home) |
| R1 | Preview (Pattern page) |
| X | Undo (Pattern page) |
| B | Redo (Pattern page) |
| D-pad Up | Fn1 (Pattern) |
| D-pad Right | Fn2 (Instrument parameters) |
| D-pad Down | Fn3 (Sample playback) |
| D-pad Left | Fn4 (Master) |
| Menu | screen scale: stretched / 1:1 |

Preview, Undo and Redo are the Pattern page's one-press keys; on other pages use the page's own undo where it has one (for example in the R4 list of the sample editor).

**Fn5 to Fn8:** hold **Y** (Shift) **first**, then L4 + D-pad. That is Shift + Fn, as on the Mini: Up Song, Right Sample loader, Down File, Left Config.

The Fn keys open pages and can be rebound in **Config, Fn buttons binding**, exactly as on the Mini. They also work as held keys: hold **L4 + Left** for a quick look at Master. To use a Pattern-held shortcut, press **L4 + Up**, then let go of L4 while you keep Up held: the Pattern key stays held and the other buttons are back to normal (for example, the value keys then change the pattern number).

A button keeps the meaning it had when you pressed it, so press **Y before L4** when you want Shift together with an L4 shortcut.

### R4: the action list

The Mini has eight screen keys under its display; their functions change from page to page. The Brick has no such keys, so PolyBrik puts every screen key function of every page into the **action list**, drawn in the screen keys row.

**Opening it**

- **Tap R4:** the list opens and stays open (**sticky**, blue frame). Pick an entry with Left / Right and press **B** to run it. Tap **R4** again (or Menu) to close it.
- **Hold R4:** the list is open while you hold (**momentary**, amber frame). Move with Left / Right; when you **let go of R4**, the highlighted action runs and the list closes.

**Using it**

- **Left / Right** move the highlight through the whole list, skipping greyed-out entries and wrapping around.
- **L4** (tap) flips to the next page of the list; the page number shows in the last cell, for example `2/3`.
- **Value entries** (a number under the name, such as Length or Volume): the shoulders change them by -1 / -10 / +10 / +1, and Up / Down by +1 / -1, without leaving the list.
- **Action entries** run with B (sticky) or on release (momentary).
- **Greyed-out entries** are not available in the page's current state (for example Fill outside edit mode). The highlight skips them.
- While the list is open it takes every button except **Start** (Play), **Select** (Rec) and **Menu**, so you cannot change the pattern by accident.
- The list remembers where you left it on each page.

**When the list is closed** its cells stay visible in the screen keys row, tinted dark, with live values: you can always see the page's screen key functions and their values.

**Every page has its list**, built from that page's screen keys, including pages with more than eight (the Mini's **More** key pages are flattened into list pages) and the screens inside a page: the Instrument page in sample and synth mode, **Load Patch** and **Edit Patch** for the synths, the patch browser, the synth macros, the envelope and effects screens, the Master effects and mixers, the File browsers and export, the sample editor effects, the recorder's stages and the name keyboards.

The **Pattern page** list, for example:

| Page | Entries |
|---|---|
| 1 | Pattern, Length, Step jump (values), Fill, Render selection, Invert, Duplicate, Expand |
| 2 | Shrink, Copy pattern, Paste pattern, Name pattern, Name track, 8-track view |
| 3 | Mute 1 to 8 |
| 4 | Rec arm 1 to 8 |

### Firmware windows and keyboards

- **Yes / No windows** (overwrite, stop pattern, changes lost, clock warning and the like): **B** answers Yes, **X** answers No or Cancel.
- **Name keyboards:** B types the selected character, X is backspace, the D-pad moves. Save, Cancel and Auto name are in the R4 list.
- **Other windows** (Fill, render, save changes) put their buttons into the R4 list.

### Pages and the Menu button

**Hold Menu** to show the firmware's page grid, move with the D-pad, and **let go** to open the page. **B** while the grid is shown cycles the page's mode where it has more than one. The Fn keys (L4 + D-pad) open pages directly.

### Settings you can switch with files

PolyBrik reads two optional empty files in the `PolyBrik` folder at start:

| File | Effect |
|---|---|
| `no-scale` | start with the screen at 1:1 instead of stretched (L4 + Menu still switches) |
| `generic-r4` | R4 shows a frame that moves over the firmware's own screen key labels instead of the curated lists (for troubleshooting) |

## Brick and Brick Pro

PolyBrik detects the device from NextUI and adapts.

| | TrimUI Brick | TrimUI Brick Pro |
|---|---|---|
| Column keys | A (the cursor column's key) | A, plus the **left stick**: Left Note, Right Instr, Up FX1, Down FX2 |
| Jog wheel | the value keys | the value keys, plus the **right stick**: turn it around like the Tracker+'s jog wheel |
| Buttons under the screen | labelled L3 / R3; they are this manual's L4 / R4 | labelled L4 / R4 |
| Right stick click | not present | Preview (Pattern page) |
| Left stick click | not present | not used |
| Legend strip | no stick click cells | stick click cells |

Everything else is the same on both.

## Known limitations

- **MIDI** (USB and DIN), **USB audio**, **USB storage** and **audio input** are not supported. The sample recorder runs but records silence.
- **One highlight at a time:** the Mini lets you hold two screen keys together (two parameters, several mixer tracks, two sample points). The action list selects one entry at a time.
- Pattern-held shortcuts that use the D-pad (queue and step patterns while holding the Pattern key) are out of reach, because your thumb holds the Fn key on the D-pad. The Pattern entry in the Pattern page's list changes and queues patterns instead.
- The plain TrimUI Brick has not been tested on hardware yet.
- Long names in the screen keys row are shortened to fit (for example "Perf Presets"); the full name is shown in the page above.

## Maintenance

PolyBrik is shared as is. There is no plan to maintain it or to keep it up to date with Tracker CE upstream. Issues and pull requests on GitHub are welcome, but there is no guarantee that any of them will be answered, fixed or merged. If you want to take it further, please fork it and extend it: every change to the firmware is listed in `platforms/sdl/PATCHES.md` and wrapped in `#ifdef PLATFORM_SDL`, so pulling a newer Tracker CE into a fork stays manageable. Before you start, read `AGENTS.md`: it collects the rules, build details, device notes and traps I ran into, for people and for coding agents alike.

## How this was made

I built PolyBrik with [Claude Code](https://claude.com/claude-code), used liberally and generously: it wrote about 99% of the code, the patches, the tests and this documentation, under my direction, with my testing on the hardware.
