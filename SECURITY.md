# Security policy

## Supported versions

FacetWire is pre-1.0. Security fixes are made on the latest `main` revision;
there are no supported release branches yet.

## Reporting a vulnerability

Do not open a public issue for a suspected vulnerability. Use GitHub private
vulnerability reporting when enabled for this repository. If it is not
available, contact the repository owner privately through their GitHub profile
and request a secure reporting channel.

Include the affected revision, platform, reproduction steps, impact, and any
suggested mitigation. Please avoid accessing data that is not your own and do
not publish exploit details before a fix and coordinated disclosure are ready.

Security-sensitive areas include plugin loading, manifest parsing, untrusted
document parsing, process isolation, path handling, resource limits, and host
service capability enforcement.
