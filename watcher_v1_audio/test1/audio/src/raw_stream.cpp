#include "mrtsp/media/audio/raw_stream.hpp"

uint16_t       RAW_Stream::channels = 2; // Number of channels (default = 2)
uint16_t       RAW_Stream::sample_rate = 44100; // Audio sample rate (default = 44100)
uint16_t       RAW_Stream::input_device = 0; // Input device to use (default = 0)
uint16_t       RAW_Stream::input_offset = 0; // Input channel offset (default = 0)
uint16_t       RAW_Stream::buffer = 512; // Number of bytes (default = 512)

/** Local Call back Implimentation? **/
int /*RAW_Stream::*/createPacket(void *outputBuffer,
        void *inputBuffer,
        unsigned int nFrames,
        double streamTime,
        RtAudioStreamStatus status,
        void *data ) {
    if(status) {
        throw Audio_Ex("Stream over/underflow detected.");
    }
    RAW_Stream *stream = static_cast<RAW_Stream *>(data);
    stream->presetTimeStamp();
    Packet audio_packet(
            static_cast<unsigned char *>(inputBuffer),
            static_cast<unsigned char *>(inputBuffer) + (nFrames*2*2));
    stream->packetize(audio_packet);

    return 0;
}

RAW_Stream::RAW_Stream() :
Media_Object(RAW_Stream::sample_rate, "L16-2") {
#ifdef __DEBUG__
    std::cout << "RAW_Stream::RAW_Stream called" << std::endl;
    adac.showWarnings(true);
#endif
    if(adac.getDeviceCount() < 1) {
        throw Audio_Ex("Audio: No audio devices found!");
    }
    input_parameters.deviceId = adac.getDefaultInputDevice(); // input_device
    input_parameters.nChannels = channels;
    input_parameters.firstChannel = input_offset;
    frame_buffer = buffer;
    try {
        adac.openStream(
            NULL,
            &input_parameters,
            FORMAT, // ???
            sample_rate,
            &frame_buffer,
            (RtAudioCallback)createPacket,
            this,
            &options );
    }
    catch(RtAudioError& e) {
        throw Audio_Ex(e.getMessage());
    }
#ifdef __DEBUG__ // Test RtAudio functionality for reporting latency.
    std::cout << "RTAudio Stream latency = " << adac.getStreamLatency() << " frames" << std::endl;
#endif
    startStream();
}

RAW_Stream::~RAW_Stream() {
    if( adac.isStreamOpen() ) {
        adac.closeStream();
    }
}

void RAW_Stream::startStream() {
    try {
        adac.startStream();
    }catch(RtAudioError& e) {
        throw Audio_Ex(e.getMessage());
    }
#ifdef __DEBUG__
    std::cout << "Raw Stream Started." << std::endl;
#endif
}

bool RAW_Stream::isStreamRunning() {
    if(!adac.isStreamRunning()) {
        throw Audio_Disconnected_Ex();
        return false;
    }
    return true;
}


int RAW_Stream::getSampleRate() const {
    return RAW_Stream::sample_rate;
}

std::string RAW_Stream::getMediaName() const {
    return "L16";
}

std::string RAW_Stream::getMediaType() const {
    return "audio";
}

int RAW_Stream::getEncodingParameter() const {
    return RAW_Stream::channels;
}
