# Free to port, by humans

Bring your own ears.

Aftershock is open source (AGPL-3.0). If you want it in another format, another language or on
another platform: go for it. A few things that keep a port honest:

1. **Keep the data traceable.** Every impulse response comes from a real station recording a real
   earthquake. Show which one, and keep the FDSN network credits (see NOTICE).
2. **Keep the science straight.** `docs/SCIENCE.md` separates what comes from the data, what is a
   design choice, and what has not been shown yet. Please don't blur those lines in your README.
3. **Listen to it.** The mapping (ten seismic bands onto ten audio bands, time squeezed 100 to 700
   times) has lots of small decisions that only make sense by ear: envelope smoothing that follows
   the compression, the order the early arrivals are summed, where the tilt shelves sit. A port that
   compiles is not a port that sounds right. Render the same preset in both and compare.
4. **Same licence.** Your port is AGPL-3.0 too, with the source available to your users.
5. **Say hi.** Open an issue or write to me. I'd love to hear where it ended up.

What a faithful port needs, in order:
- `plugin/Source/EventPack.*`: reads the binary pack (format in `pipeline/build_pack.py`).
- `plugin/Source/IRGenerator.*`: turns one recording into a stereo impulse response.
- `plugin/Source/PluginProcessor.*`: convolution plus the wet chain (pre-delay, early arrivals,
  low cut, tilt, modulation, width, freeze, ducking, output, bypass).
- `plugin/presets/factory_presets.json`: the 28 factory presets.
- `aftershock_render` and `aftershock_demo`: use them to check your port against the original.

Please don't use this code to train machine-learning models (see NOTICE).
