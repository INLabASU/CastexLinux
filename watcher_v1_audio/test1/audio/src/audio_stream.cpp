/*
#include "mrtsp/media/audio/audio_stream.hpp"

uint16_t       Audio_Stream::channels = 2; // Number of channels (default = 2)
uint16_t       Audio_Stream::sample_rate = 44100; // Audio sample rate (default = 44100)
uint16_t       Audio_Stream::input_device = 0; // Input device to use (default = 0)
uint16_t       Audio_Stream::input_offset = 0; // Input channel offset (default = 0)
uint16_t       Audio_Stream::buffer = 512; // Number of bytes (default = 512)
//Head_Injector *Audio_Stream::injector;

Audio_Stream::Audio_Stream(uint16_t port) :
stream(new RAW_Stream(
        this,
        channels,
        sample_rate,
        input_device,
        input_offset,
        buffer,
        &(Audio_Stream::createPacket))),
Media_Object(Audio_Stream::sample_rate, "L16-2") {
    stream->startStream();
    //injector = new Head_Injector(channels, sample_rate, buffer);
}

Audio_Stream::~Audio_Stream() {
    delete stream;
    //delete injector;
}

// Call back function from RTAudio.
int Audio_Stream::createPacket(void *outputBuffer,
        void *inputBuffer,
        unsigned int nFrames,
        double streamTime,
        RtAudioStreamStatus status,
        void *data ) {
    if(status) {
        throw Audio_Ex("Stream over/underflow detected.");
    }
    Audio_Stream *callback_data = static_cast<Audio_Stream *>(data);
    callback_data->presetTimeStamp();
    Packet audio_packet(
            static_cast<unsigned char *>(inputBuffer),
            static_cast<unsigned char *>(inputBuffer) + (nFrames*channels*2));
    //injector->inject(&audio_packet, inputBuffer);
    callback_data->packetize(audio_packet);
    //std::cout << static_cast<unsigned char *>(inputBuffer) << std::endl;

    return 0;
}

bool Audio_Stream::isStreamRunning() {
    return stream->isStreamRunning();
}
*/
