# ImuxHack

Windows-only Geode mod for Geometry Dash 2.2081.

ImuxHack is being built as an original music-driven level generation system. The core pipeline is:

`audio -> PCM -> onset/beat analysis -> beat timeline -> LevelGraph -> validation -> Geometry Dash objects`

Current implementation includes:

- Own IMUX C++ API.
- Deterministic LevelGraph generator.
- Configurable difficulty, density, synchronization, movement and decoration.
- Spectral/energy frame analysis and onset detection.
- BPM estimation from detected events.
- WAV PCM loader for 16/24/32-bit PCM and 32-bit IEEE float.
- Playability-oriented graph validation.
- Geode mod settings.
- IMUX button and interface entry in the Geometry Dash editor.
- Windows GitHub Actions build producing a .geode artifact.

The editor integration and generator are intentionally separated from the audio core so future decoders, pattern generators and object serializers can be added without rewriting the UI.

The next implementation layer is direct Geometry Dash level-object insertion, section regeneration, cached analysis and MP3/OGG decoding on Windows.
