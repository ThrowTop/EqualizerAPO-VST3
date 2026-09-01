# Project instructions

- Use the single configured build tree at `build/vs2026`; do not create alternate build, test-runtime, QA-output, or numbered fallback directories.
- If an Equalizer APO executable from this repository blocks a build, deployment, replacement, or cleanup, terminate that process and retry. Do not work around the lock by creating another output directory.
- Keep generated Visual Studio `*.dir` directories at the root of `build/vs2026` hidden.
