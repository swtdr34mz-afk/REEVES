# REEVES – changes

The newest first. To release a new version: raise the number in VERSION and put its notes here under
"## <the new number>". After the merge into main the release is made by itself (auto-release.yml), the
downloads are built into it, and the website and the Discord post follow.

## 1.2.1
The factory sounds re-voiced for the new oscillators:
- Basses and leads with SUB OCTAVE get their low end back (the sub is quieter since 1.2.0, so it's turned up).
- All 32 sounds evened out in level, so none jumps out when you step through them.
- Split points shown as Cubase names notes: LOW SPLIT's POINT starts on C3 (note 60), as does Split A|B (both used to say C4 for the same note on some screens).
- More Rev2 character: Rev Pad and Warm Strings now have the Rev2's slowly moving saw (an LFO on SHAPE MOD), Brass Section a bit of reedy bite from oscillator 2's SHAPE MOD.

## 1.2.0
The oscillators re-voiced from recordings of a real Prophet Rev2 (thanks to the user who recorded 600 notes of his Rev2):
- SHAPE MOD now works as on the Rev2: at 0 every shape is the plain wave; turned up, a pulse of that width is added - the saw gets a second drop that moves through the wave, triangle and saw+tri a moving step, and PULSE goes from a thin needle through about square to a needle the other way.
- SAW + TRI has the Rev2's own lopsided shape.
- The SUB OCTAVE is about 10 dB quieter, as on the Rev2.
- Imported Rev2 programs (.syx) now sound much closer to the originals. The factory programs are re-voiced to sound as before; your own saved sounds that use SHAPE MOD may sound different.

## 1.1.6
New: REEVES for Linux - VST3, LV2 and a standalone app (64-bit, built on Ubuntu 22.04). Checked with pluginval like the Windows version.

## 1.1.5
The window opens as large as you left it - also after closing and reopening the DAW (before, it came back at the smallest size). A new instance starts at the size you used last.
Saving your own sounds fixed: the save window now opens inside the plugin (before, it could open hidden behind the DAW's plugin window, so nothing got saved). If Windows keeps apps out of the Documents folder, your sounds are saved in AppData instead. A failed save now says so.

## 1.1.4
(see the release page on GitHub)
