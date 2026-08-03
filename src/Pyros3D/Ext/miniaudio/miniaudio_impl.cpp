//============================================================================
// Name        : miniaudio_impl.cpp
// Description : The single translation unit that compiles miniaudio itself.
//============================================================================

// miniaudio is a single-header library: every other file includes it for the
// declarations only, and exactly one .cpp defines MINIAUDIO_IMPLEMENTATION to
// emit the code. Same arrangement the project already uses for glad.
//
// The features below are disabled because nothing here uses them, and each one
// is a meaningful chunk of compile time and binary size:
//   - the low-level device/context API is still needed (ma_engine sits on it),
//     so only the decoders and unused backends are trimmed.
//   - MP3/FLAC decoding pulls in dr_mp3/dr_flac; the engine ships WAV assets
//     and OGG/Vorbis is not built in by default anyway. Re-enable by deleting
//     the matching define if a project needs those formats.
#define MA_NO_MP3
#define MA_NO_FLAC

#define MINIAUDIO_IMPLEMENTATION
#include <Pyros3D/Ext/miniaudio/miniaudio.h>
