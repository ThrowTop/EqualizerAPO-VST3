# Third-party notices

This project is derived from **Equalizer APO** by Jonas Thedering and other
contributors. Equalizer APO source is licensed under the GNU General Public
License, version 2 or later. The complete project license is in `License.txt`.

The build and binary packages also use the following components. This summary
is informational; the corresponding license texts and copyright notices govern.
Release packages place the available notices under `licenses/`.

| Component | Use | License |
| --- | --- | --- |
| Equalizer APO | Original application and DSP code | GPL-2.0-or-later |
| libHybridConv | Convolution implementation | LGPL-2.0-or-later |
| muparserx | Embedded expression parser | BSD-style license |
| Steinberg VST3 SDK | VST3 host interfaces and support code | MIT |
| Qt 6.7.2 | Configuration Editor runtime | LGPL-3.0-only or GPL/commercial options offered by Qt |
| FFTW | FFT processing | GPL-2.0-or-later |
| libsndfile | Audio-file I/O | LGPL-2.1-or-later |
| FLAC, Ogg, Vorbis, Opus | libsndfile codec dependencies | BSD-style licenses |
| LAME | libsndfile MP3 codec dependency | LGPL-2.0 |
| mpg123 | libsndfile MP3 codec dependency | LGPL-2.1 |
| Microsoft Visual C++ runtime | Compiler runtime redistributed with the package | Microsoft Visual C++ Redistributable terms |

Source and license references:

- Equalizer APO: <https://sourceforge.net/projects/equalizerapo/>
- Qt: <https://www.qt.io/licensing/open-source-lgpl-obligations>
- FFTW: <https://www.fftw.org/>
- libsndfile: <https://libsndfile.github.io/libsndfile/>
- muparserx: <https://beltoforion.de/en/muparserx/>
- Steinberg VST3 SDK: <https://github.com/steinbergmedia/vst3sdk>

The binary package includes the GNU Lesser General Public License 2.1 terms
applicable to libHybridConv's "version 2 or later" license grant.

Qt is deployed as replaceable dynamic libraries; this project does not use a
static Qt build. Dependency sources can be reconstructed from the pinned
versions in `tools/bootstrap-dependencies.ps1`, `vcpkg.json`, and the vcpkg
baseline recorded by that bootstrap script.
