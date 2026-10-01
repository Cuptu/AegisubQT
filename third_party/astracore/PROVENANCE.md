# AstraCore source

Upstream: https://github.com/Cuptu/AstraCore

Source and header revision: `71dcc0724bfcf0dfb7fe75968392b367c27f2eac`.
The persistent video-session implementation and prior integration fixes are now
committed upstream. The original import used a local uncommitted session change
on base `b8aed2520502633f7128657760b02e039cf93599`; this copy has been refreshed
from the public upstream revision above. Its CMake project is replaced here by
the Aegisub build integration; the C tests and fixture generator come from upstream.

License: GPL version 3; the full upstream license is in `LICENSE`. Distributing
the combined application with this backend requires preserving its GPL license
and supplying the corresponding source, including local changes and build files.

Included upstream changes: FFmpeg SDK minimum 6.1; detect the supported-config
function instead of assuming all libavcodec 61 versions provide it; validate
the Content Light Level side-data size before reading it; seek again when a
persistent session requests its previously consumed frame, preserving the
requested timestamp instead of advancing one frame. Real decode/seek tests and
a generated media fixture have been added alongside the upstream tests.
The optional `color_range` tail field in `ac_hdr_metadata` reports the stream's
FFmpeg color range while accepting callers with the original struct size.
Persistent sessions drain delayed B frames at EOF and reject invalid time,
format, dimensions and buffer arguments. Real media tests include B frames and
recovery after invalid requests, with fixtures generated from PNG/WAV inputs.
