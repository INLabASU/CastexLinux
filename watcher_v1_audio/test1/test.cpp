#include <sys/socket.h>
#include <netinet/in.h>


#define SENDER 1
#define RECEIVER 0

#define RUN_CONFIG SENDER

// Main
int main(){
    if(RUN_CONFIG == SENDER){
        
    }else{ // Receiver

    }
}

// Sender function
void sender(){
    while(1){
        // Spin on sending raw audio from microphone.
    
    }
}

// Receiver function
void receiver(){
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
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
        
    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                    &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("192.168.43.1");
    address.sin_port = htons( PORT );
    memset(address.sin_zero, '\0', sizeof address.sin_zero);
        
    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address,
                                sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    unsigned char buffer[64 * 1024];
    while(1){
        len = sizeof(cliaddr);
        ret = recvfrom(server_fd, buffer, sizeof(buffer), 0,
                (struct sockaddr *) &serverStorage, &addr_size);
                
        // Play buffer here.
    }
}
