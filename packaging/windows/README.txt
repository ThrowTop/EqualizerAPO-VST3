EqualizerAPO-VST3
=================

EqualizerAPO-VST3 is a substantially reworked Windows system audio processor
derived from Equalizer APO. It is independently maintained and is not an
official Equalizer APO release.

Installation
------------

Use the setup executable for normal installation. EqualizerAPO-VST3 shares the
Windows APO registrations used by Equalizer APO, so uninstall the original
product before running setup. Side-by-side installation is not supported.

For the portable package, extract the complete archive to its permanent
location and run install.bat. Optionally pass an existing absolute
configuration path:

    install.bat -ConfigFile C:\path\to\main.txt

Without that argument, installation keeps the registered configuration or
creates a minimal config.txt under Documents\EqualizerAPO-VST3. Administrator
approval is required. Open Configuration Editor after installation and use
Manage devices to select playback or capture endpoints.

Safety
------

VST effects run inside the Windows Audio service process. A faulty plugin can
crash or hang that process. Probe plugins before using them on a live endpoint.
The host currently provides no plugin latency compensation or process
isolation.

License and source
------------------

License and dependency notices are included in License.txt,
THIRD_PARTY_NOTICES.md, and the licenses directory. Complete corresponding
source is provided by the repository and tag that published this artifact.
