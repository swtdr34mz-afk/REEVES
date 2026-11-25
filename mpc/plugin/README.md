# REEVES for the MPC

The REEVES synth as a plugin for Akai MPC standalone hardware: the same engine as the REEVES VST3, built with Akai's
MPC Plugin SDK. A bi-timbral polysynth in the manner of the Sequential Prophet Rev2: two oscillators per voice
(sawtooth, saw+tri, triangle, pulse, each with SHAPE MOD), sub octave, noise, sync, a Curtis-style 4-pole / 2-pole
low-pass, three DADSR envelopes, four LFOs, eight modulation slots, an effect per layer, the arp, the poly and gated
sequencers of the 32 factory programs, unison with chord memory, STACK and SPLIT, 8 or 16 voices.

Unofficial: not made, endorsed or supported by Sequential.

## Installieren (kurz auf Deutsch)

1. MPC OS **3.10.1 oder neuer**. Auf der MPC: **Preferences → Enhancements (Beta) → Enable Enhancements** und
   **External Plugin Support** einschalten.
2. Den Ordner **`swtdr34mz - VST - REEVES`** (aus dem Download `REEVES-MPC.zip`) auf einen USB-Stick, eine SD-Karte
   oder das interne Laufwerk der MPC kopieren, in einen Ordner namens **`VST2 Plugins`** ganz oben auf dem Laufwerk:
   `VST2 Plugins/swtdr34mz - VST - REEVES/`.
3. Die MPC neu starten. REEVES steht dann im Plugin-Browser unter **External Plugins** (als Plugin-Programm laden).

Die MPC XL, Live III (64 Bit) und die 32-Bit-Geräte (X, X SE, Live, Live II, One, One+, Key 61, Key 37) nehmen
denselben Ordner; jedes Gerät lädt seine eigene Datei darin.

## Die Seiten

Sechs Seiten, auf den Funktionstasten. **Einmal drücken: Layer A, nochmal drücken: Layer B** (unten links steht,
welcher Layer gerade zu sehen ist).

| Taste | Was drauf ist (aufgebaut wie die Bereiche am Rev2) |
| --- | --- |
| **OSC** | OSCILLATORS: oben Oszillator 1, unten Oszillator 2 (FREQ, FINE TUNE, SHAPE mit LEDs, SHAPE MOD), dazu Sub, Noise, Mix, Slop; SYNC dazwischen |
| **FILTER** | LOW-PASS FILTER: oben Cutoff bis Audio Mod, unten 4 POLE und die Hüllkurve |
| **ENV** | AMPLIFIER und AUXILIARY ENVELOPE nebeneinander |
| **LFO** | die vier LFOs, jeder wie die LFO-Sektion am Rev2 |
| **MOD** | Display mit allen 8 Modulations-Slots; eine Slot-Nummer antippen, dann bearbeiten SOURCE, DESTINATION und AMOUNT rechts diesen Slot |
| **PLAY** | Effekt, Arp, Glide/Unison/Hold; Program (Stack/Split, Splitpunkt, 8/16 Stimmen, Qualität), Misc Params |

Unter jedem Regler steht sein Wert. Ein Knopf wird mit dem Finger hoch und runter gezogen oder mit seinem Q-Link gedreht; zweimal tippen öffnet den
großen Regler der MPC (bei einer Auswahl die Liste). Ein Taster schaltet um, eine LED-Liste wählt aus. Die Q-Links
liegen auf jeder Seite auf den wichtigsten Reglern (obere Reihe der Q-Links = erste Gruppe; auf MOD: die Mengen und Ziele
aller 8 Slots).

Im **Program-Modus** (ohne Plugin-Seite) machen die Q-Links: 13-16 Cutoff, Resonance, Filter-Env, Key Amount;
9-12 Filter-Hüllkurve; 5-8 Amp-Hüllkurve; 1-4 FX Mix, LFO 1 Menge, Unison Detune, Lautstärke (alles Layer A).

**QUALITY**: ECO (2x) ist voreingestellt und reicht auf der MPC; HIGH (4x) klingt in den Höhen noch etwas sauberer,
braucht aber fast doppelt so viel Rechenzeit. Auf den 32-Bit-MPCs ist die Stimmenzahl auf 8 voreingestellt.

Die Programme: die 32 Werks-Sounds der VST3 (Programm-Wechsel auf der MPC), mit ihren Sequenzen. Das Tempo kommt
von der MPC.

## Licence

MPL-2.0. Fonts: Fira Sans and Anonymous Pro, SIL Open Font License.
