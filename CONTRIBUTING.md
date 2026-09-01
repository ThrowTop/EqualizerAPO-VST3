# Contributing

Thank you for helping improve EqualizerAPO-VST3.

## Before opening a change

- Keep changes focused and explain the user-visible reason for them.
- Search existing issues before starting overlapping work.
- Do not commit configs, plugins, logs, dumps, build output, dependency caches,
  or generated Visual Studio files.
- Preserve compatibility with existing Equalizer APO text configurations unless
  the change explicitly documents a migration.

## Build and test

Use the single configured build tree:

```powershell
tools\bootstrap-dependencies.bat
cmake --preset default
cmake --build --preset release
ctest --test-dir build/vs2026 -C Release --output-on-failure
```

Do not create alternate build directories. If a repository executable locks an
output, terminate it and retry. Generated root `build/vs2026/*.dir` directories
must remain hidden.

For VST3 changes, run `VST3HostTests.exe` against representative real plugins
and record plugin version, class ID, channel layouts, declared latency, state
restore, and any crash. A short probe is not proof of long-duration real-time
safety.

## Real-time requirements

Code reached from the APO processing callback must not allocate memory, take
contended locks, perform file or registry I/O, scan plugins, or write logs.
Measure DSP and host changes in Release mode at realistic small callback sizes.
Keep declared plugin latency, processing time, and end-to-end latency as
separate measurements.

## Style and release notes

- Match surrounding C++ and CMake style; avoid unrelated formatting churn.
- Compile with the existing `/W4` settings and address new warnings.
- Update the root README, changelog, or code comments when behavior,
  configuration, packaging, or verification steps change.
- Never report an estimate or offline Editor measurement as a live benchmark.

## Pull requests

Include a concise summary, the commands and manual scenarios tested, and any
known limitation. Attach no proprietary plugin binaries, user configs, logs
containing private paths, or crash dumps containing private data.
