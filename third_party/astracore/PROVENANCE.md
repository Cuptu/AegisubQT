# AstraCore source

Upstream: https://github.com/Cuptu/AstraCore

Base revision: `b8aed2520502633f7128657760b02e039cf93599`.
The source and header were imported from the existing local AstraCore checkout,
including its uncommitted persistent video-session implementation. That checkout
has not been modified. Its original CMake project is replaced here by the
Aegisub build integration; the two upstream C tests are included unchanged.

License: GPL version 3; the full upstream license is in `LICENSE`. Distributing
the combined application with this backend requires preserving its GPL license
and supplying the corresponding source, including local changes and build files.

Local integration changes: FFmpeg SDK minimum 6.1; detect the supported-config
function instead of assuming all libavcodec 61 versions provide it; validate
the Content Light Level side-data size before reading it; seek again when a
persistent session requests its previously consumed frame, preserving the
requested timestamp instead of advancing one frame. Real decode/seek tests and
a generated media fixture have been added alongside the upstream tests.
The optional `color_range` tail field in `ac_hdr_metadata` reports the stream's
FFmpeg color range while accepting callers with the original struct size.
