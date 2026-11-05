# Audio test assets

Everything here was derived from the firmware source at `tracker-ce/` (line numbers are
from upstream commit 7fd6c2c) and proven with screenshots and recordings from `tracker-sdl`
with audio enabled. Run everything from the repo root. Every run must use a FRESH copy of
`sdcard/` (the firmware writes instruments and the project into the SD it is given):

    cp -R sdcard /tmp/sd-x && ./build-host/tracker-sdl $(sed 's#--sd sdcard#--sd /tmp/sd-x#' platforms/sdl/tests/audio/notes.keys) --audio-file out.wav

## Files

- `sine440.wav`: 2.000 s, mono, 16-bit, 44100 Hz, 440 Hz sine at -6 dBFS (peak 16383),
  5 ms linear fades. The same file sits at `sdcard/Samples/sine440.wav`, where the key
  scripts load it from (it is the 4th entry of the Samples/ listing after `/..`).
- `analyze.py`: numpy-only checks, see below.
- `*.keys`: one line of `tracker-sdl` flags each, sd path written as `sdcard`.

## Key scripts

Key ids are from `platforms/sdl/InputMap.cpp`. Fn bindings (`mtConfig.cpp:565-572`,
`interfaceMenuPopup.cpp:28-41`): `shift+fn2` = Sample Loader, `shift+fn3` = File,
`fn1` = Pattern editor.

- `load.keys`: boot, Sample Loader, enter `Samples/` (`down`, `insert`), move to
  `sine440.wav` (4 x `down`), `screen1` (labelled "Enter"/"Add") loads it into
  instrument 1, `fn1` back to the pattern editor. Screenshot at 9.5 s shows
  `1. sine440` top right.
- `notes.keys`: continues with `rec` (edit mode), then at steps 1, 5, 9, 13 of track 1
  a `plus1` press. In edit mode `plus1` feeds the encoder with +1
  (`interfaceInputs.cpp:231-238`), `functEncoder` -> `changeSelectedStepValue`
  (`fPatternEdit.cpp:3187-3195`) -> `Sequencer::changeSelectionNote`, and an empty step
  with a positive delta becomes `STEP_NOTE_DEFAULT` = 60 with the last used instrument
  (`mtSequencer_patternEdit.cpp:658-668`, `mtSequencer.h:44`). `down` x4 moves to the
  next note step; `up` x6 parks the cursor on step 7 so steps 1..13 are all visible;
  `rec` leaves edit mode; screenshot at 17.0 s shows `C5 01` at 1, 5, 9, 13; `play` at
  17.5 s; quit at 22 s (a 32-step pattern lasts 3.69 s, see below).
- `notes-c4.keys`: same, with `minus10` after each `plus1`. `minus10` is -10 on the
  encoder, which the note column turns into -12 (`fPatternEdit.cpp:3192`), so the steps
  read `C4 01`.
- `export.keys`: the notes as above, then `shift+fn3` (File page), `screen6` = Export
  (`fProjEdit.cpp:270`), `screen3` = Export Pattern (`fProjEdit.cpp:293` ->
  `functExportPattern` -> `startExport(exportPattern)`). Proven on the audio build:
  screenshot at 19.1 s shows "File - Export" with only
  "Cancel" lit (the during-export page, `startExport` blocks all inputs except screen8),
  header `0m:00s ||`, and the file below exists after quit, with `--audio-file` and with
  the SDL device.

The audio thread is required. Every note edit previews the step with `playSelection()`
when the sequencer is stopped (`mtSequencer_patternEdit.cpp:700-704` and `1293-1298`),
and the preview only ends when the sequencer ticks past the step (`inc_uStepGlobal`,
`mtSequencer.cpp:2280-2290`), which happens from `updateAudioBuffers`. Without the
audio thread (`--no-audio`) the transport stays in play and `functExport` stops at
"This action will stop the pattern" (`fProjEdit.cpp:878-884`); no `Export/` folder is
created.

## Expected numbers

### Tempo and step length

`Sequencer::getStepPeriodMs` (`mtSequencer.cpp:2252-2260`):
`timerPeriodUs = (312500 / tempo) / 12`, `stepPeriodMs = 12 * 48 * timerPeriodUs / 1000`,
so one step is `15000 / tempo` ms (a 16th note: `kStepsPerBeat = 4`, `kuStepsInStep = 48`,
`mtSequencer.h:945-947`).

Default tempo is 130 BPM, not 120: `DEFAULT_TEMPO = 130.0f` (`mtStructs.h:49`),
`globalTempo = DEFAULT_TEMPO` (`mtStructs.h:915`, `fileManagerProject.cpp:299`).
Default pattern length is 32 steps (`mtStructs.h:916`, `fileManagerProject.cpp:300`,
and the "Length 32" box in the screenshots). The scripts do not change either.

| tempo | 1 step | 4 steps (note interval) | 16 steps | 32 steps (whole pattern) |
|-------|--------|-------------------------|----------|--------------------------|
| 130 (default) | 115.385 ms | 461.538 ms | 1.84615 s | 3.69231 s |
| 120 | 125.000 ms | 500.000 ms | 2.00000 s | 4.00000 s |

"Pattern divider 4" on the config screen is `patternDiv = 3` (`mtStructs.h:961`) and only
sets the row highlight (`fPatternEdit.cpp:322` -> `stepDevider`, used in
`libs/NewDisplay/tracker.cpp:439`; menu text `configMenuDefs.h:246`: "Highlights each
chosen step"). It does not change timing.

### Unpitched note

The sample plays at its original rate at note index 60: `kMiddleNoteIdx = 60` and
`notes[idx] = 2^((idx - 60) / 12) * noteCorrection` (`play_memory.cpp:23, 47-48`).
Index 60 is displayed as `C5` (`scales.h:62`, `allNotes[60]`), and it is also
`STEP_NOTE_DEFAULT` (`mtSequencer.h:44`), which is why `plus1` on an empty step gives it.
`C4` is index 48 and plays one octave down.

`noteCorrection` is `NOTE_CORRECTION_COEFFICIENT = 44100 / 44117.64706`
(`mtStructs.h:358`): live playback calls `initNotesLookup()` with the default `true`
(`mtAudioEngine.cpp:185`); the exporter calls `initNotesLookup(false)` before rendering
and `initNotesLookup(true)` after (`mtAudioExport.cpp:77, 118`).

| note entered | expected live pitch | expected export pitch |
|--------------|---------------------|-----------------------|
| C5 (`notes.keys`) | 439.824 Hz | 440.000 Hz |
| C4 (`notes-c4.keys`) | 219.912 Hz | 220.000 Hz |

"Root note C3" on the config screen is `padBoardRootNote` (`mtStructs.h:876`,
`configMenu.cpp:71`): the pad board / note keyboard root, unrelated to sample pitch.

### Export file

`Export/New Project/New Project_P1_0001/master.wav` inside the sd copy
(`fileManagerExport.cpp:219-224` builds `Export/<project>/<project>_P<pattern>_<nnnn>`,
counter bumps while the folder exists; `mtAudioExport.cpp:194` appends `master.wav`;
project name `cNewProjectName = "New Project"`, `fileManagerDefs.h:54`). Stereo 16-bit
44100 Hz.

Expected content: 176128 frames = 3.9938 s = 32 steps (1272.2 blocks of 128) plus a
104-block silence tail (`kReverbSilenceMaxMs` = 300 ms, `mtAudioExport.cpp:632-647`);
peak 0.2454 (`sox stat`), `pitch` 440.000 Hz, `onsets` four notes 461.5 ms apart.

Two platform details the export depends on (both handled in the tree):

- The exporter renders on the main thread (`Tracker.cpp:171` -> `mtAudioExport::update`
  -> `updateRender` -> `artificial_isr`, `mtAudioExport.cpp:697-713`) without the audio
  lock, and in render mode the audio thread's `updateAudioBuffers` returns at once
  (`Tracker.cpp:181`). `AudioOutputStream::update()` still copies 256 bytes into each
  pointer last given to `setOutBuffers` (`core/libs/Audio/Output.cpp:8-29`), so
  `AudioHost::renderBlock` must hand it buffers that never die (static), like the DMA
  buffers on hardware; stack buffers crashed the export with SIGSEGV.
- `saveMasterBufferToSD` (`mtAudioExport.cpp:326-384`) reopens `master.wav` with
  `kWriteCreateNew` for every 70-block packet, seeks to the old end and writes on. On
  FatFs (`FA_CREATE_ALWAYS`) the freed clusters are re-allocated by the seek and the old
  bytes survive (`ff.c` "Reuse the cluster hole"); a POSIX truncate-at-open left a zero
  hole and an all-zero file of the right length. `FileSystemImpl` now truncates such a
  file at close to the furthest byte written.

## Notes on the expected numbers

- The firmware's unpitched note is C5 (index 60), not C4: a 440 Hz sample at C4 plays at 220 Hz. Live playback is also 0.04 % flat (439.824 Hz) because
  of the clock correction coefficient; only the export is exactly 440 Hz.
- The default project is 130 BPM with a 32-step pattern, and a step is a 16th note.
  16 steps at 120 BPM are 2.000 s; at the default 130 BPM they are 1.846 s and the full 32-step pattern (what Export
  Pattern renders) is 3.692 s.

## Running analyze.py

    python3 platforms/sdl/tests/audio/analyze.py pitch out.wav --start 0.05 --dur 0.4
    python3 platforms/sdl/tests/audio/analyze.py onsets out.wav --threshold -20
    python3 platforms/sdl/tests/audio/analyze.py compare out.wav "sd-x/Export/New Project/New Project_P1_0001/master.wav" --max-offset 8820

- pitch test: `pitch` on the live recording or the export, windowed to one note
  (`--start`/`--dur`); `peak1` should match the table above (0.4 s of a sine resolves
  to better than 0.1 Hz with the parabolic interpolation; the self-check gave 440.000 Hz
  on a 0.1 s burst, and a 0.1 s window of the live stream gave 439.818 Hz). A segment
  whose strongest peak is below -60 dBFS prints `silent segment ..., no pitch`.
- tempo test: `onsets` prints onset times and the gaps between them; with the four notes
  the gaps should be 461.5 ms (130 BPM), the live loop returns every 3.692 s (1272 or
  1273 blocks, the sequencer ticks per 128-sample block) and the export is 3.994 s long.
  The envelope uses 0.5 ms blocks with a 15 dB hysteresis: a retrigger of the same
  sample only dips to zero for 1 to 2 ms (stacked play in `envelopeUpdate`), which 5 ms
  blocks averaged away, and the release ramp of a stopped preview would otherwise fire
  every 50 ms. Measured on `notes.keys`: previews at 10.006, 11.507, 13.007, 14.505 s,
  pattern at 17.506, +0.464, +0.462, +0.462 s, loops at +3.694 s and +3.695 s.
- export test: `compare` aligns B to A by cross-correlation (lag limited to
  `--max-offset` samples), then prints the max absolute sample difference in 16-bit
  units and the RMS of the difference in dB relative to A. Identical signals print
  `max abs diff 0.0` and `-inf dB`. It warns when the aligned window covers less than
  90 % of the longer file and when the difference is above -20 dB (then the offset is
  not meaningful). The export and the live stream are never sample-identical: live
  plays at 439.82 Hz, the export at 440.00 Hz, a slip of 0.07 periods per 0.4 s. Compare
  windows cut at the onsets (`sox master.wav e.wav trim 0.0045 0.05`, `sox live.wav l.wav
  trim 17.5062 0.05`, `--max-offset 100`): 50 ms gives about -29 dB (max abs diff 475),
  0.4 s about -18 dB, the whole pattern -3 dB.

Self-check (synthetic file, two 100 ms bursts of 440 Hz at 0.200 s and 0.700 s, and a
copy delayed by 100 samples): `pitch` 440.000 Hz, `onsets` 0.2000 s and 0.6999 s
(+0.4999 s), `compare` offset 100 samples, max abs diff 0.0, -inf dB.
