# How Aftershock turns a seismogram into a reverb

This document separates three things: what comes straight from the data, what is a deliberate
design choice, and what has not been shown yet. The plugin is only meaningful if those stay
distinct.

## 1. The data

Each recording is three-component broadband ground velocity (vertical, north, east) from a
permanent station of a global or national network. The pipeline (`pipeline/build_pack.py`):

1. resolves the origin (time, location, depth, magnitude) from USGS ComCat
2. picks one station per distance band: near (< 3°, roughly < 330 km), regional (3–15°),
   teleseismic (20–60°)
3. removes the instrument response (pre-filter 0.02–9.5 Hz), rotates to Z/N/E,
   low-passes at 8 Hz and resamples to 20 Hz
4. rejects recordings that would be misleading:
   - **near clipping**: raw counts above 90 % of a 24-bit digitiser's range
   - **sensor step**: the instrument's resting level shifts during the shaking (mass drift or tilt
     close to large earthquakes). Response removal turns the step into a slow fake signal
     that leaks backwards in time, so these are rejected rather than repaired
   - **energy before P**: more than 4x the early pre-event level just before P, in 0.5–5 Hz
   - **gaps** and **signal-to-noise below 8**
5. for near stations whose broadband sensors clip, falls back to the co-located
   accelerometer (HN channels), which records strong shaking without clipping
6. keeps the trace until the envelope drops to twice the pre-event noise

P and S times come from the iasp91 model. The surface-wave marker is distance / 3.6 km/s and is
only an approximation, shown for shallow events only.

## 2. Measurements stored with each recording

**Band level**: energy in ten log-spaced bands from 0.1 to 8 Hz, relative to the strongest band.
This is the recording's spectrum. It changes strongly with distance (high frequencies are lost)
and with magnitude (bigger events radiate relatively more low frequency).

**Seismic RT60**: per band, the decay slope of the envelope after its post-S peak, fitted between
-5 and -25 dB (as T20 in room acoustics) and extrapolated to 60 dB. If noise prevents a 20 dB
fit, a 10 or 5 dB fit is used and stored (`fit_db`); the interface draws those points lighter.
Missing points mean the band never rose far enough above the noise.

An apparent Q is stored alongside: Q = 0.4548 · f · RT60, from amplitude decaying as
exp(−π f t / Q). It ignores geometric spreading and changing wave types, so it is a label, not a
seismological estimate of attenuation. At teleseismic distances the "decay" also contains
surface-wave dispersion, not only scattering.

**Early-arrival taps**: the eight strongest energy bursts between P and S + 30 % of the S−P time,
with their relative timing, strength and N/E balance.

## 3. The mapping (design choices)

| Seismic | Audio | Why |
|---|---|---|
| time, a window of the recording | time, compressed by k = window / reverb length (typically 100–700x) | makes minutes of ground motion reverb-length |
| ten bands 0.1–8 Hz, log spaced | ten bands 120 Hz–12 kHz, log spaced | keeps "low decays slowest, high decays fastest" in the same direction |
| band envelope (energy, smoothed) | amplitude of band-passed noise in the matching audio band | the reverb inherits each band's onset, peaks and decay |
| north / east ground motion | left / right | two horizontal directions of real motion |
| P-to-S arrival bursts | early reflections | real arrival rhythm of that event at that station |

Envelope smoothing follows the compression: it is limited so the envelope never moves faster
than about 40 Hz in audio time. Otherwise the reverb grains.

**Spectrum** (0–1) decides how much of the band levels survives. At 1 the reverb has the
recording's spectrum, which at teleseismic distances means a very dark sound (high bands
60–80 dB down). At 0 every band is equally loud and only the shape over time remains. The
default (0.35) keeps the ordering between recordings while staying usable.

**Decay contrast** scales how differently the bands decay around their average: 1 is the data,
0 makes them decay alike, 2–3 exaggerates the difference. This is the control that makes the
attenuation of different regions most audible.

**Grit** blends in the raw trace played k times faster. This is plain audification: pitch and
length are tied, so a shorter reverb is also higher.

## 4. What should be audible, and how to test it

The claim worth testing is narrow: *recordings from different tectonic settings and distances
give reverbs that listeners can tell apart, and the differences follow the measured band level
and seismic RT60.*

A plan:

1. **Stimuli.** Use `aftershock_render` to render all recordings at a fixed length (3 s),
   spectrum 1 and decay contrast 1, loudness-normalised. Convolve a short set of dry sources
   (speech, snare, piano chord) with each.
2. **Discrimination (ABX).** Pairs from the same setting and distance band versus pairs from
   different settings. If listeners cannot beat chance on different-setting pairs, the setting
   label is not audible and should not be advertised as a feature.
3. **Attribute rating (MUSHRA-style, no reference).** Rate brightness and length. Correlate
   ratings with the stored band level slope and mean seismic RT60. A significant correlation
   means the physics reaches the ear through the mapping.
4. **Control.** Include an exponential-decay noise reverb matched in RT60 and spectrum. If
   listeners rate it the same as the seismic reverbs, the remaining "seismic character" is in the
   fine structure (arrival bursts, irregular decay), which is the part a plain reverb cannot copy.

## 5. First measurements on the rendered IRs

All 80 recordings rendered with `aftershock_render --length 3 --spectrum 1 --contrast 1 --grit 0
--damping 0`, then measured on the IR itself (spectral centroid, and T20 of the 3 s IR
extrapolated to 60 dB). Medians:

| Group | n | Centroid | T20 |
|---|---|---|---|
| near (< 330 km) | 19 | 2887 Hz | 3.6 s |
| regional | 26 | 1601 Hz | 5.7 s |
| teleseismic | 35 | 1058 Hz | 3.9 s |
| volcanic (near + regional) | 3 | 1916 Hz | 1.4 s |
| deep slab (near + regional) | 2 | 1535 Hz | 7.3 s |
| stable craton (near + regional) | 5 | 2968 Hz | 6.1 s |
| other settings (near + regional) | 35 | 1900–3800 Hz | 3.7–5.8 s |

Distance darkens the reverb as attenuation predicts; the volcanic recordings give much shorter
reverbs, deep-slab and craton paths the longest. The groups are small and the setting is
confounded with magnitude and station, so these are reasons to run the listening test, not results.

## 6. Known limits

- 35 curated earthquakes and 80 recordings; 19 events have a near station. Great earthquakes
  (M8+) clip regional broadband sensors, so they often only have teleseismic recordings.
- The band mapping (log onto log) is a choice, not a law. Another defensible choice is a single
  frequency transposition by k (what "Grit" does), which keeps the seismic spectrum exactly but
  ties pitch to length.
- The ten audio carrier bands are noise, so fine spectral detail inside a band is not from the data.
