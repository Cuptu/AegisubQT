# Windows AstraCore runtime

The archive is the complete Windows x64 runtime built from the AstraCore
revision in `archive.json`, downloaded from the linked successful three-platform
CI run. The archive SHA256 is verified before extraction. Source and header
revision: `../astracore/upstream-revision.txt`; corresponding upstream source:
https://github.com/Cuptu/AstraCore/tree/62e883d002dd060bcd74282154cfbdd8a1e834e8

The archive preserves the upstream manifest, native DLL, FFmpeg/ffprobe, libmpv,
all shared dependencies, and LICENSES. Runtime validation tools in
`../astracore_runtime_tools` are copied unchanged from this source revision and
licensed under its GPL-3.0 license. Refresh the archive and these tools together
when updating the upstream revision. Explicit native SDK builds remain supported.
