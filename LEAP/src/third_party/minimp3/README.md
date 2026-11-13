# minimp3

Upstream: https://github.com/lieff/minimp3
Pinned commit: `ea99364f61c14656440e8d77e9c233ccf3124633`.
The header is unchanged; see LICENSE (CC0/public domain dedication).
`Radio.cpp` builds only Layer III with `MINIMP3_ONLY_MP3` and disables SIMD
for ESP32-S3 with `MINIMP3_NO_SIMD`. No other decoder or audio library is linked.

Radio test fixtures are synthetic sine tones generated with FFmpeg/libmp3lame,
not recordings of broadcasts. They test actual PCM decoding in native CI.
