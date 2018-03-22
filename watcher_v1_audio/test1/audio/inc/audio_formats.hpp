/**
   Structure definitions for Audio Formats. (RAW, WAV)
**/

#include <cstdint>

#ifndef AUDIO_FORMATS_HPP
#define AUDIO_FORMATS_HPP

/** Packet typedef **/
typedef std::vector<unsigned char> Packet;

/** RTAudio typedefs **/
typedef unsigned int RtAudioStreamStatus;
typedef int (*RtAudioCallback)(void *outputBuffer,
        void *inputBuffer,
        unsigned int nFrames,
        double streamTime,
        RtAudioStreamStatus status,
        void *userData );

/** RAW Specifications **/
#define FORMAT RTAUDIO_SINT16
typedef signed short RAW_Data;

struct RAW_AUDIO {
    // Information needed to create audio format headers
    unsigned long bufferBytes;
    unsigned long totalFrames;
    unsigned long frameCounter;
    unsigned int channels;
    // RAW Data
    RAW_Data *buffer;
};

/** WAV Specifications **/
// https://web.archive.org/web/20140327141505/https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
struct RIFFChunk {
    uint32_t chunkID = 0x52494646; //'RIFF'
    uint32_t chunkSize; // Size of all chunks (RIFF + FMT + DATA)
    uint32_t format = 0x57415645; //'WAVE'
};
struct fmtChunk { // Sub-chunk of RIFF
    uint32_t chunkID = 0x666d7420; //'fmt '
    uint32_t chunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
struct dataChunk { // Sub-chunk of RIFF
    uint32_t chunkID = 0x64617461; //'data'
    uint32_t chunkSize;
    RAW_Data data;
};
struct WaveHeader {
    RIFFChunk riff;
    fmtChunk fmt;
    dataChunk data;
};

#endif
