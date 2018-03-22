#include "mrtsp/media/audio/head_injector.hpp"

Head_Injector::Head_Injector(unsigned short channels,
        unsigned short sample,
        unsigned short frame_buffer) {
    /** Data Chunk **/
    header.data.chunkSize = frame_buffer * channels * sizeof(RAW_Data);
    /** FMT Chunk **/
    header.fmt.chunkSize = sizeof(header.fmt) -
            sizeof(header.fmt.chunkID) -
            sizeof(header.fmt.chunkSize); // PCM is expected to be 16
    header.fmt.audioFormat = 1; // 1 for PCM
    header.fmt.numChannels = channels;
    header.fmt.sampleRate = sample;
    header.fmt.bitsPerSample = 16; // Default is 16-bit audio
    header.fmt.byteRate = sample * channels * header.fmt.bitsPerSample/8; // Number of bytes per second.
    header.fmt.blockAlign = channels * header.fmt.bitsPerSample/8; // Size of data block size in bytes
    /** RIFF Chunk **/
    header.riff.chunkSize = sizeof(header.riff.format) +
            sizeof(header.fmt) +
            sizeof(header.data.chunkID) +
            sizeof(header.data.chunkSize) +
            header.data.chunkSize;
}

Head_Injector::~Head_Injector() {}

void Head_Injector::inject_8(Packet *packet, uint8_t data) {
    packet->push_back( data );
}

void Head_Injector::inject_16(Packet *packet, uint16_t data) {
    inject_8( packet, static_cast<uint8_t>( data & 0x00FF ) );
    inject_8( packet, static_cast<uint8_t>(( data & 0xFF00 ) >> 8) );
}

void Head_Injector::inject_32(Packet *packet, uint32_t data) {
    inject_16( packet, static_cast<uint16_t>( data & 0x0000FFFF ) );
    inject_16( packet, static_cast<uint16_t>(( data & 0xFFFF0000 ) >> 16) );
}

void Head_Injector::inject_16_ascii(Packet *packet, uint16_t data) {
    inject_8( packet, static_cast<uint8_t>(( data & 0xFF00 ) >> 8) );
    inject_8( packet, static_cast<uint8_t>( data & 0x00FF ) );
}

void Head_Injector::inject_32_ascii(Packet *packet, uint32_t data) {
    inject_16_ascii( packet, static_cast<uint16_t>(( data & 0xFFFF0000 ) >> 16) );
    inject_16_ascii( packet, static_cast<uint16_t>( data & 0x0000FFFF ) );
}

void Head_Injector::inject_data(Packet *packet, void *data, uint32_t size) {
    Packet buffer(static_cast<unsigned char *>(data), static_cast<unsigned char *>(data) + size);
    packet->insert(packet->end(), buffer.begin(), buffer.end());
}

void Head_Injector::inject(Packet *packet, void *data) {
    /** RIFF Chunk **/
    // inject_32_ascii( packet, header.riff.chunkID );
    // inject_32( packet, header.riff.chunkSize );
    // inject_32_ascii( packet, header.riff.format );
    /** fmt Chunk **/
    // inject_32_ascii( packet, header.fmt.chunkID );
    // inject_32( packet, header.fmt.chunkSize );
    // inject_16( packet, header.fmt.audioFormat );
    // inject_16( packet, header.fmt.numChannels );
    // inject_32( packet, header.fmt.sampleRate );
    // inject_32( packet, header.fmt.byteRate );
    // inject_16( packet, header.fmt.blockAlign );
    // inject_16( packet, header.fmt.bitsPerSample );
    /** data Chunk **/
    // inject_32_ascii( packet, header.data.chunkID );
    // inject_32( packet, header.data.chunkSize );
    inject_data(packet, data, (header.data.chunkSize));
}
