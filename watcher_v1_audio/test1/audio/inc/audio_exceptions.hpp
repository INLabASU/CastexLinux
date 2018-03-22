#include <iostream>
#include <string>

#include <arpa/inet.h>

#ifndef AUDIO_EX_HPP
#define AUDIO_EX_HPP

class Audio_Ex : std::runtime_error {
public:
    Audio_Ex(std::string const& message) :
    std::runtime_error(message) {}
};

class Audio_Disconnected_Ex : Audio_Ex {
public:
    Audio_Disconnected_Ex() :
    Audio_Ex("Audio: Disconnected") {}
};

class Audio_Invalid_Settings_Ex : Audio_Ex {
public:
    Audio_Invalid_Settings_Ex() :
    Audio_Ex("Invalid Setting Configurations") {}
};

#endif
