# REEVES

A 16-voice (or 8-voice) bi-timbral polysynth in the manner of the Sequential **Prophet Rev2**, as a **VST3** for
Windows and the Mac, an **AU** for the Mac and a **standalone app**. By **swtdr34mz**. Its sound comes from the same
oscillators, ladder filter and envelopes as [PROPHET 67](https://github.com/swtdr34mz-afk/PROPHET-67-VST).

Unofficial: not made, endorsed or supported by Sequential.

## Download

[Actions](https://github.com/swtdr34mz-afk/REEVES/actions) → the newest green run → **Artifacts** at the bottom:
`REEVES-Windows` or `REEVES-macOS` (a GitHub sign-in is needed for artifacts).

## Installieren (kurz auf Deutsch)

**Windows (Cubase):**
1. Zip entpacken.
2. Den Ordner `REEVES.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
3. Cubase starten (oder *Studio → VST-PlugIn-Manager → Aktualisieren*). REEVES steht unter den Instrumenten,
   Hersteller swtdr34mz.
4. `REEVES.exe` ist die Standalone-App. Audio- und MIDI-Gerät stellt man unter *Options* ein.

Windows SmartScreen kann beim ersten Start der App warnen (sie ist nicht signiert): *Weitere Informationen →
Trotzdem ausführen*.

**Mac:** `REEVES.vst3` nach `~/Library/Audio/Plug-Ins/VST3/`, `REEVES.component` nach
`~/Library/Audio/Plug-Ins/Components/`, dann einmal im Terminal: `xattr -cr` auf beide Dateien.

## What it has

- **8 or 16 voices** (VOICES on the panel: the Rev2's two models), shared by the two layers: SINGLE gives all to A,
  **STACK A+B** and **SPLIT A|B** half to each (the split point in MISC PARAMS).
- Per voice: **two oscillators** (sawtooth, saw+tri, triangle, pulse; SHAPE MOD on each), a **sub octave**, noise,
  hard **SYNC**, **OSC SLOP**, glide (four modes, a rate per oscillator), a **Curtis-style low-pass** (4-pole,
  self-oscillating, or 2-pole) with **AUDIO MOD**, three **DADSR** envelopes (the auxiliary one with REPEAT),
  **four LFOs** (KEY SYNC, CLK SYNC).
- **Modulation**: eight slots of 22 sources and 53 destinations (the Rev2's lists), plus the mod wheel, pressure,
  breath, velocity and foot routes (MISC PARAMS).
- Per layer: an **EFFECT** (Delay Mono, DDL Stereo, BBD Delay, Chorus, Phaser High / Low / Mst, Flanger 1 / 2,
  Reverb, Ring Mod, Distortion, HP Filter), a clock, an **ARP** (up, down, up+down, random, assign; range,
  repeats, relatch), the **poly sequencer** (64 steps of up to six notes, recorded from the keys with rests and
  ties) and the **gated sequencer** (four tracks of 16 steps, each its own length, SLEW).
- **UNISON** of 1 to 16 voices with DETUNE and KEY MODE, and CHORD memory.
- **HOST SYNC**: the clocks, synced LFOs and delays follow the DAW's tempo and position.
- 32 factory programs; WRITE keeps your own (Documents/REEVES/Programs).

## The panel

The panel is the Rev2 desktop module's (no keys: play it from a MIDI keyboard or the DAW). As on the Rev2: the display shows what was touched last. **MISC PARAMS**, the modulation's **SOURCE** /
**DESTINATION**, the ARP's **MODE**, the sequencer's **MODE** / **DESTINATION** / **RECORD** and **Global** open
their pages; **PARAMETER** picks a line, **VALUE** sets it. With the mouse: the wheel over the display is VALUE, a click
on it picks from a list (or a program), the gated sequencer's steps are drawn with the mouse. **SELECT** in
MODULATION picks the slot, the LFO buttons 1-4 the LFO the knobs show. **Edit Layer B** shows layer B.

In REEVES, **PLAY** switches the gated sequencer on as well as the poly one.

## Building

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
JUCE 8 comes from GitHub. The engine's tests: `cmake -S tests -B build-tests && cmake --build build-tests && build-tests/engine_tests`.

## Licence

MPL-2.0 (LICENSE). Fonts: Fira Sans and Anonymous Pro, SIL Open Font License (fonts/).
