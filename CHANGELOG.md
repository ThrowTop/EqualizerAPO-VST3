# Changelog

This changelog records changes in EqualizerAPO-VST3, an independently named and
substantially reworked project derived from Equalizer APO.

## Unreleased

### Added

- Target-based CMake and Visual Studio 2026 build, test, deployment, portable
  ZIP, and NSIS installer workflows.
- Native x64 VST3 effect hosting with module/class selection, processor and
  controller state, native Win32 views, and declared-latency reporting.
- Stable playback/capture endpoint bindings with conservative recovery after
  Windows endpoint GUID changes.
- Configuration Editor device management through an elevated helper.
- Device-scoped, collapsible effect groups with independent enable controls.
- Automated stable-binding and VST3 host tests.

### Changed

- The active configuration is an explicit absolute file path and can be changed
  from Configuration Editor.
- The public build defaults to the baseline x64 instruction set; optimized SIMD
  variants remain opt-in developer builds.
- Dependencies are pinned and prepared below the ignored `.deps` tree.
- The upstream update checker is disabled because it targets upstream release
  infrastructure.

### Removed

- The retired Visual Studio-only build and obsolete Configurator workflow.
- Upstream web shortcuts that no longer describe this fork's behavior.

### Known limitations

- VST3 hosting has no plugin latency compensation, process isolation,
  sidechains, instruments/MIDI, transport, or 32-bit bridge.
- Long-duration live microphone and gaming qualification remains open.
- The 0.x series remains pre-release while installer and live-audio
  qualification are completed.
