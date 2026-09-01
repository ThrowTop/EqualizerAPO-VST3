# Security policy

## Supported versions

Security fixes are provided for the most recent tagged release and the current
`main` branch. Older development builds are unsupported.

## Reporting a vulnerability

Use GitHub's private **Report a vulnerability** flow in the repository Security
tab. Include affected versions, reproduction steps, impact, and the smallest
necessary diagnostic material. Remove personal paths, device names, configs,
plugin binaries, and unrelated memory from reports.

If private vulnerability reporting is not enabled yet, open a public issue that
only requests a private contact channel; do not include exploit details in that
issue.

Audio plugins run in the Windows Audio service process. A crash caused solely
by a third-party plugin may need to be reported to that plugin's vendor, but a
host boundary or validation failure in this project is in scope.
