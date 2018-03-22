/*

#include <vector>

#include "mrtsp/media_object.hpp"
#include "mrtsp/media/audio/raw_stream.hpp"
#include "mrtsp/media/audio/audio_formats.hpp"

#ifndef AUDIO_STREAM_HPP
#define AUDIO_STREAM_HPP

class Audio_Stream : public Media_Object {
public:
    Audio_Stream(uint16_t port);
    ~Audio_Stream();

    static int createPacket(void *outputBuffer,
        void *inputBuffer,
        unsigned int nFrames,
        double streamTime,
        RtAudioStreamStatus status,
        void *data);
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
    //Media_Object *m;
    RAW_Stream *stream;
    //static Head_Injector *injector;
};

#endif
*/
