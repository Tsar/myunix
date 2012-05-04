#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

void* malloc(size_t size);
void* memset(void* ptr, int value, size_t num);

typedef struct {
    int portNumber;
    int socketDescriptor_ipv4;
    struct sockaddr_in servAddr_ipv4;
    int socketDescriptor_ipv6;
    struct sockaddr_in servAddr_ipv6;
} PortInfo;

void error(const char* msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char* argv[]) {
    PortInfo* ports = malloc(sizeof(PortInfo) * (argc - 1));
    int i;
    for (i = 0; i < argc - 1; ++i) {
        ports[i].portNumber = atoi(argv[i + 1]);

        ports[i].socketDescriptor_ipv4 = socket(AF_INET, SOCK_STREAM, 0);
        if (ports[i].socketDescriptor_ipv4 < 0)
            error("ERROR: Could not create socket [IPv4]");
        memset(&ports[i].servAddr_ipv4, 0, sizeof(ports[i].servAddr_ipv4));
        ports[i].servAddr_ipv4.sin_family = AF_INET;
        ports[i].servAddr_ipv4.sin_addr.s_addr = INADDR_ANY;
        ports[i].servAddr_ipv4.sin_port = htons(ports[i].portNumber);
        if (bind(ports[i].socketDescriptor_ipv4, (struct sockaddr*)&ports[i].servAddr_ipv4, sizeof(ports[i].servAddr_ipv4)) < 0)
            error("ERROR: Could not bind socket [IPv4]");
        if (listen(ports[i].socketDescriptor_ipv4, 5) < 0)
            error("ERROR: Could not start listening on socket [IPv4]");
        //accept
    }
    
    free(ports);

    return 0;
}
