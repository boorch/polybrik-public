# Brick to Tracker Mini key map v4

v4 replaces the v3 screen-key layers. The table is data: `InputMap::kBrickBindings` in `platforms/sdl/InputMap.cpp` (stable string ids, "" = unbound), raw joystick indices per device in `kBrickDevices`.

## Rules
1. The four shoulders are the Mini's own value keys, in the Brick's physical left-to-right order: L1 = -1, L2 = -10, R2 = +10, R1 = +1. Shift (Y) + any value key stays the firmware's master volume.
2. B (south) is a plain Enter. The firmware's own hold and repeat apply to it again.
3. A (east) is the column key (core patch 20): Note / Instr / FX1 / FX2 of the cursor column, press, hold and release forwarded. Held with Shift + Copy or Delete it limits paste or delete to that column; held on Instr / FX it opens the cell's list.
4. Y (west) is the only Shift. X (north) is Delete; Shift + X is backspace.
5. L4 (f1) is the edit layer modifier. Nothing is sent on L4 press or release alone. While the action list is up, an L4 tap flips to the next list page and its shortcuts are off.
6. R4 (f2) is the action list key. Press shows the list in the firmware's label bar. Release after a hold event (333 ms, `ButtonsImpl` hold time) runs the highlighted action and closes it (momentary, amber frame). Release before a hold event leaves it up (sticky, blue frame); Enter runs the highlight, R4 or Menu closes it.
7. While the list is up it owns every key except Start (Play), Select (Rec) and Menu (which closes it first). Left / Right move the highlight across the whole list, skipping disabled entries and wrapping. On a value entry L1 / L2 / R2 / R1 nudge -1 / -10 / +10 / +1 and Up / Down +1 / -1; Enter does nothing there.
8. Two-button firmware windows (overwrite, clock warning, stop pattern, changes lost) answer to B (confirm) and X (cancel), core patch 27. Keyboards keep the firmware meanings: B types, X is backspace, Save / Render / Cancel are list entries on R4.
9. Left / Right walk the Note, Instr, FX1, FX2 columns before changing track (core patch 19).
10. Menu (b8) is an ordinary key: hold shows the module grid, release picks. Power, Vol+ and Vol- stay with the OS.

## Table
Plain Brick names; Brick Pro differences in the notes column. SDL names are positional (SDL a is the key printed B).

| Brick input | SDL | Alone | L4 held (edit layer) | Notes |
|---|---|---|---|---|
| D-pad Up / Down | hat 0 | Up / Down | Fn1 (Pattern) / Fn3 (Sample playback) | Y held before L4: Shift + Fn, i.e. Fn5 / Fn7 (Song / File) |
| D-pad Left / Right | hat 0 | Left / Right | Fn4 (Master) / Fn2 (Instrument) | Y held before L4: Fn8 / Fn6 (Config / Sample loader); let go of L4 and the Fn key stays held |
| L1 | b4 | minus1 | Copy | |
| L2 | a2 | minus10 | Paste | synthesized Shift down, Copy press and release, Shift up |
| R2 | a5 | plus10 | Insert | Shift + Insert is Home (Y + L4 + R2) |
| R1 | b5 | plus1 | Preview | patch 28 key on the Pattern page |
| B (south) | b0 | Enter | Redo | patch 28 key on the Pattern page |
| A (east) | b1 | column key | | |
| Y (west) | b2 | Shift | Shift | |
| X (north) | b3 | Delete | Undo | patch 28 key on the Pattern page |
| Start | b7 | Play | Play | |
| Select | b6 | Rec | Rec | |
| Menu | b8 | Menu | Screen scale: stretched / 1:1 (platform, the firmware sees nothing) | 1:1 sits centred on top of the legend strip |
| L4 (f1) | b9 plain, b11 Pro | mod | mod | |
| R4 (f2) | b10 plain, b12 Pro | list | list | |
| L3 | b9 Pro | unbound | | |
| R3 | b10 Pro | Preview | | Pro only |
| Left stick (Pro) | a0 / a1 | Left Note, Right Instr, Up FX1, Down FX2 | | press at 20000, release at 12000 (`kStickOn` / `kStickOff`) |
| Right stick (Pro) | a3 / a4 | jog wheel | | angle outside `kWheelRadius` 16000, 12 detents per turn, each sends `Interface::potChange(0, +1 / -1)` |

L4 + A is spare. Fn bindings follow the firmware's defaults (Config, Fn buttons binding, in the 32-bit app). Desktop keyboard map: `kDesktopKeys` in `InputMap.cpp` (Tab list, Z / Y undo / redo on the Pattern page; `--keys` takes `pad.<input>` ids and the Mini ids `list`, `mod`, `preview`, `undo`, `redo`).

## Legend strip (`io/Legend` v3)
Row 0: four shoulder cells (-1, -10, +10, +1; copy, paste, insert, preview while L4 is held). Row 1: one cell per other key, the held modifier's cell amber and the others showing that shortcut. Row 2: the page name (core patch 21) plus a hint; with the list up, the list hint. The screen keys row above the strip shows the page's list cells tinted dark while the list is closed.

## What the Brick cannot do like the Mini
- Holding two screen keys at once (two parameters, two sample points, several mixer tracks): the list has one highlight.
- Pattern-held shortcuts on the D-pad: the thumb holds the Fn key (L4 + Up, then L4 released); the Pattern list entry changes and queues patterns instead.
- Plain Brick: Instr and FX1 as keys of their own; A is the cursor column's key, the Pro adds all four on the left stick.

## Power combination
Select + Start held 500 ms calls the firmware power callback; the firmware then needs the buttons held another 1.5 s (`TURN_OFF_TIME`, `mtSleep.h:8`) before it saves and exits, about 2 s in all. A Select or Start press reaches the firmware 150 ms late, and not at all when the other joins within that time. Escape has the same hold on the desktop.

## Device detection
Both devices have two buttons under the screen: printed L4 / R4 on the Brick Pro, L3 / R3 on the plain Brick (same function, ids `f1` / `f2`). The legend uses each device's print: on the plain Brick its cells and hints say L3 / R3. On the Pro, L3 / R3 are the stick clicks.

`--device` or the `DEVICE` environment variable picks `brick` or `brickpro` (default `brick`). On the plain Brick raw b9 / b10 are L4 / R4; on the Pro they are the stick clicks and L4 / R4 are b11 / b12. The stick axes exist only in the Pro table. The stick thresholds are not device-verified yet.

## Ideas (post-MVP)
- Haptic ticks on the jog wheel, Pro only. Not checked: whether the Brick's haptics setting drives a motor reachable from a pak; needs its own device probe.
