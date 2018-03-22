/**
   Takes RAW audio and configuration information,
   Creates a WAV spec'ed header and Injects into audio frame,
   Outputs WAV audio.
**/

#include <iostream>
#include <vector>

#include "mrtsp/media/audio/audio_formats.hpp"

#ifndef HEAD_INJECTOR_HPP
#define HEAD_INJECTOR_HPP

class Head_Injector {
public:
    Head_Injector(unsigned short channels,
            unsigned short sample,
            unsigned short frame_buffer);
    ~Head_Injector();

    void inject(Packet *packet, void *data);

private:
    WaveHeader header;
    void inject_8(Packet *packet, uint8_t data);
    void inject_16(Packet *packet, uint16_t data);
    void inject_32(Packet *packet, uint32_t data);
    void inject_16_ascii(Packet *packet, uint16_t data);
    void inject_32_ascii(Packet *packet, uint32_t data);
    void inject_data(Packet *packet, void *data, uint32_t size);
};

#endif
