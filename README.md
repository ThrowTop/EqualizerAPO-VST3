# EqualizerAPO-VST3

EqualizerAPO-VST3 is a substantially reworked Windows system-wide audio
processor with native VST3 hosting. It replaces the old solution-only build
with a target-based CMake architecture and adds new device identity, endpoint
management, configuration, testing, deployment, and packaging systems.

The project descends from Equalizer APO and retains compatibility with its text
configuration language and x64 VST2 hosting, but it is independently named and
maintained. It is not a minor patch set or an official Equalizer APO release.

> [!IMPORTANT]
> EqualizerAPO-VST3 currently uses the same Windows APO class registrations and
> application registry contract as Equalizer APO. The two products cannot be
> installed side by side. Uninstall Equalizer APO before installing
> EqualizerAPO-VST3.

The 0.x series is pre-release while setup, upgrade, uninstall, and long-running
live-audio qualification are completed.

## Highlights

- One CMake/Visual Studio build for the APO, Editor, helpers, tests, portable
  package, and NSIS installer.
- Native x64 VST3 effects, including flat modules and `.vst3` package
  directories, processor/controller state, declared latency, and native Win32
  plugin views.
- Existing x64 VST2 effect hosting.
- Playback and capture endpoint management from the unelevated Editor through
  a small elevated helper.
- Stable physical-device bindings that can recover from Windows endpoint GUID
  changes and fail closed on ambiguous matches.
- Device-scoped effect groups with independent enable and collapse controls.
- An explicit active configuration path rather than an assumed file inside the
  application directory.
- Automated stable-device and VST3 host tests.

## Platform and safety

- Windows 10 or newer, x64 only.
- Release packages use the baseline x64 instruction set by default. AVX,
  AVX2, and AVX-512 remain opt-in developer builds.
- Installation changes Windows audio-engine settings and requires
  administrator rights. Applications requiring a protected audio path may
  behave differently.
- VST effects run inside the Windows Audio service process. A faulty plugin can
  crash, hang, or corrupt that process; probe plugins before enabling them on a
  live endpoint.
- VST3 hosting currently has no plugin latency compensation, sidechains,
  instruments/MIDI, 32-bit bridge, transport, or process isolation.

## Configuration compatibility

Existing Equalizer APO text configurations are intended to remain valid. In
particular, legacy `Device:` selectors using names, words, endpoint GUIDs,
semicolon-separated alternatives, or `Device: all` continue to use their
original matching behavior.

Stable bindings are an extension:

```text
Device: stable:v1|binding=...|endpoint=...|flow=...|connection=...|device=...|fallback=name
```

- Opening and saving an old config does not automatically rewrite an existing
  legacy `Device:` selector.
- Opening a device group's selector, keeping or changing its checked devices,
  and confirming the dialog writes stable selectors for the currently matched
  endpoints.
- A config with effects before any `Device:` line is represented by the new
  Editor as an explicit `Device: all` group and will save that explicit scope.
- Stable selectors are forward-compatible within this project through their
  versioned `stable:v1` syntax, but original Equalizer APO builds do not
  understand this extension.
- Missing or ambiguous stable bindings fail closed rather than silently
  selecting a similarly named device.

The **Manage devices** installation list and a `Device:` line serve different
purposes. Manage devices controls which Windows endpoints load the APO; a
`Device:` line scopes effects among endpoints where the APO is already loaded.

VST3 configurations use a separate `VST3Plugin:` command containing the module
path, class ID, processor state, and controller state. Existing `VSTPlugin:`
lines continue to select x64 VST2 plugins.

## Installing a release

Once a tested release is published, prefer its setup executable. The installer:

1. installs the x64 runtime under `C:\Program Files\EqualizerAPO-VST3`;
2. keeps an existing active configuration, or proposes a new one under the
   current user's Documents folder;
3. registers the APO and restarts Windows Audio; and
4. opens Configuration Editor so playback or capture devices can be selected.

The portable ZIP contains `install.bat` for manual installation. Run it with an
optional absolute configuration path:

```powershell
install.bat -ConfigFile C:\path\to\main.txt
```

Without `-ConfigFile`, it keeps the registered file or creates a minimal new
configuration. Uninstall removes application files and registrations but does
not recursively delete user content under `VSTPlugins` or `VST3`.

## Build prerequisites

- Visual Studio 2026 with **Desktop development with C++**
- Windows SDK 10.0.26100
- CMake 4.4.2 or newer
- Git and Python 3
- NSIS 3.12 for the setup executable

Prepare the pinned private dependency tree:

```powershell
tools\bootstrap-dependencies.bat
```

This creates ignored content under `.deps/`: vcpkg, Qt 6.7.2, and the Steinberg
VST3 SDK. vcpkg manifest mode supplies FFTW and libsndfile.

## Configure, build, and test

The project has one supported generated tree: `build/vs2026`.

```powershell
cmake --preset default
cmake --build --preset release
ctest --test-dir build/vs2026 -C Release --output-on-failure
```

Do not create another build directory to work around a locked output. Stop the
repository executable holding the file and retry in `build/vs2026`.

Useful local overrides include:

```powershell
cmake --preset default -DEAPO_SIMD=AVX2 -DEAPO_BUILD_BENCHMARK=ON
```

`EAPO_SIMD` accepts `BASELINE`, `SSE2`, `AVX`, `AVX2`, `AVX512`, or
`AVX512_256`. Instruction-set flags apply only to DSP/core code.

## Build release artifacts

Portable ZIP:

```powershell
cmake --build --preset package
```

Portable ZIP and NSIS setup executable:

```powershell
cmake --build --preset release-artifacts
```

Artifacts are written to the ignored `dist/` directory. Packaging uses a
temporary hidden staging area under `build/vs2026/.staging` and excludes local
configs, plugins, logs, dumps, dependency caches, and build output.

## Contributing and security

See [`CONTRIBUTING.md`](CONTRIBUTING.md) before proposing changes and
[`SECURITY.md`](SECURITY.md) for private vulnerability reporting. Changes on
the real-time path require extra care: the audio callback must not allocate,
lock, scan, log, or perform file I/O.

Project history is summarized in [`CHANGELOG.md`](CHANGELOG.md). Dependency
attribution and binary-distribution terms are in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Origin and license

EqualizerAPO-VST3 is a substantially rearchitected project derived from
[Equalizer APO by Jonas Thedering](https://sourceforge.net/projects/equalizerapo/).
The original and modified code remain distributed under the GNU General Public
License, version 2 or (at your option) any later version; see
[`License.txt`](License.txt).
