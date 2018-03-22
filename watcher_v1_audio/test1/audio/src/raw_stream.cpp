#include "mrtsp/media/audio/raw_stream.hpp"

uint16_t       RAW_Stream::channels = 2; // Number of channels (default = 2)
uint16_t       RAW_Stream::sample_rate = 44100; // Audio sample rate (default = 44100)
uint16_t       RAW_Stream::input_device = 0; // Input device to use (default = 0)
uint16_t       RAW_Stream::input_offset = 0; // Input channel offset (default = 0)
uint16_t       RAW_Stream::buffer = 512; // Number of bytes (default = 512)

RAW_Stream::RAW_Stream(int read_or_recv, void *callback) {
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
    output_parameters.deviceId = adac.getDefaultInputDevice(); // input_device
    output_parameters.nChannels = channels;
    output_parameters.firstChannel = input_offset;
    frame_buffer = buffer;
    try {
        if(read_or_recv == SENDER) {
            adac.openStream(
                NULL,
                &input_parameters,
                FORMAT,
                sample_rate,
                &frame_buffer,
                (RtAudioCallback)callback,
                this,
                &options );
        }else {
            adac.openStream(
                &output_parameters,
                NULL,
                FORMAT,
                sample_rate,
                &frame_buffer,
                (RtAudioCallback)callback,
                this,
                &options );
        }
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
