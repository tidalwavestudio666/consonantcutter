ConsonantCutter Standalone (macOS) - proof-of-concept

- Open a WAV/AIFF
- Detect HF "air/noise" bursts (simple HP envelope)
- For each event: keep start+end, remove a chunk from the CENTER with crossfade, then apply gain
- Export a NEW 24-bit WAV (can be shorter because time is removed)

Build (macOS / Apple Silicon):
  cmake -B build -S . -DJUCE_DIR=/path/to/JUCE -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_BUILD_TYPE=Release
  cmake --build build

App:
  build/ConsonantCutterApp_artefacts/Release/ConsonantCutter.app
