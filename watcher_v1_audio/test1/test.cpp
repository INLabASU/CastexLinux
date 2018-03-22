#include "inc/audio_formats.hpp"
#include "inc/audio_exceptions.hpp"

#include "inc/raw_stream.hpp"

#include <sys/socket.h>
#include <netinet/in.h>

#define SENDER      1
#define RECEIVER    0

#define RUN_CONFIG SENDER
//#define RUN_CONFIG RECEIVER

// Main
int main() {
    if(RUN_CONFIG == SENDER){
        sender();
    }else{ // Receiver
        receiver();
    }
}

// Sender function
void sender() {

    //TODO: Setup sender

    RAW_Stream *stream = new RAW_Stream(SENDER, &send); // Start Stream
    while(true) {
        //TODO: Check on status of stream here
    };
}
// Audio Callback funtion. Called when next segment of data is ready. Data is in 'inputbuffer'
int send(void *outputBuffer,
        void *inputBuffer,
        unsigned int nFrames,
        double streamTime,
        RtAudioStreamStatus status,
        void *data) {
    if(status) {
        throw Audio_Ex("Stream over/underflow detected.");
    }

    /** SEND PACKET HERE **/
    // Use: 'static_cast<unsigned char *>(inputBuffer)'
    //TODO: Send data

    return 0; // return of 1 ends the stream
}

// Receiver function
void receiver() {
    // Spin on waiting for audio from the socket.
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
	struct sockaddr_in cliaddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;
    addr_size = sizeof serverStorage;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    char *hello = "Hello from server";

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd,
        SOL_SOCKET,
        SO_REUSEADDR | SO_REUSEPORT,
        &opt,
        sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("192.168.43.1");
    address.sin_port = htons( PORT );
    memset(address.sin_zero, '\0', sizeof address.sin_zero);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd,
            (struct sockaddr *)&address,
            sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Setup Audio player
    RAW_Stream *stream = new RAW_Stream(RECEIVER, &play); // Start Stream
    while(true) {
        //TODO: Check on status of stream here
    };

    unsigned char buffer[64 * 1024];
    while(1) {
        len = sizeof(cliaddr);
        ret = recvfrom(server_fd,
            buffer,
            sizeof(buffer),
            0,
            (struct sockaddr *)&serverStorage,
            &addr_size);
        // TODO: QUEUE buffer here.
        //TODO: Check on stream status
    }
}
// Audio Callback funtion. Called when ready for next segment of data. Data goes in 'outputbuffer'
int play(void *outputBuffer,
        void * /*inputBuffer*/,
        unsigned int nBufferFrames,
        double /*streamTime*/,
        RtAudioStreamStatus /*status*/,
        void *data) {
    outputBuffer = static_cast<unsigned char *>(data);
    return 0; // return of 1 ends the stream
}
