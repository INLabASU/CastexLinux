/**
  This class is for RTAudio integration for enqueuing microphone audio in frames.
**/

#include <unistd.h>
#include <vector>

#include "mrtsp/media_object.hpp"
#include "mrtsp/../rtaudio/RtAudio.h"
#include "mrtsp/media/audio/head_injector.hpp"
#include "mrtsp/media/audio/audio_formats.hpp"
#include "mrtsp/media/audio/audio_exceptions.hpp"

#ifndef RAW_STREAM_HPP
#define RAW_STREAM_HPP

#define  SLEEP( milliseconds ) usleep( (unsigned long) (milliseconds * 1000.0) )

class RAW_Stream : public Media_Object {
public:
    RAW_Stream();
    ~RAW_Stream();
/*
    int createPacket(void *outputBuffer,
            void *inputBuffer,
            unsigned int nFrames,
            double streamTime,
            RtAudioStreamStatus status,
            void *data );*/
    void startStream();
    bool isStreamRunning();
    int getSampleRate() const;
    std::string getMediaType() const;
    std::string getMediaName() const;
    int getEncodingParameter() const;


    static uint16_t channels;
    static uint16_t sample_rate;
    static uint16_t input_device;
    static uint16_t input_offset;
    static uint16_t buffer;

private:
    RtAudio adac;
    RtAudio::StreamParameters input_parameters;
    RtAudio::StreamOptions options;
    unsigned int frame_buffer;
};

#endif
