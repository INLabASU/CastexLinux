/**
  This class is for RTAudio integration for enqueuing microphone audio in frames.
**/

#include <unistd.h>
#include <vector>

#include "audio/inc/rtaudio/RtAudio.h"
//#include "mrtsp/media/audio/head_injector.hpp"
#include "audio/inc/audio_formats.hpp"
#include "audio/inc/audio_exceptions.hpp"

#ifndef RAW_STREAM_HPP
#define RAW_STREAM_HPP

#define SENDER      1
#define RECEIVER    0
#define SLEEP(milliseconds) usleep( (unsigned long) (milliseconds * 1000.0) )

class RAW_Stream {
public:
    RAW_Stream();
    ~RAW_Stream();

    void startStream();
    bool isStreamRunning();

    static uint16_t channels;
    static uint16_t sample_rate;
    static uint16_t input_device;
    static uint16_t input_offset;
    static uint16_t buffer;

private:
    RtAudio adac;
    RtAudio::StreamParameters input_parameters;
    RtAudio::StreamParameters output_parameters;
    RtAudio::StreamOptions options;
    unsigned int frame_buffer;
};

#endif
