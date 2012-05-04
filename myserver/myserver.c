#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

void* malloc(size_t size);
void* memset(void* ptr, int value, size_t num);

void error(const char* msg) {
    perror(msg);
    exit(1);
}

typedef struct {
    int socketDescriptor;
    pthread_t threadId;
} TalkerThreadInfo;

typedef struct {
    int portNumber;
    int ipv6;
    pthread_t threadId;
} AcceptorThreadInfo;

static void* threadTalker(void* talkerThreadInfo) {
    TalkerThreadInfo* tti = (TalkerThreadInfo*)talkerThreadInfo;
    
}

static void* threadAcceptor(void* acceptorThreadInfo) {
    AcceptorThreadInfo* ati = (AcceptorThreadInfo*)acceptorThreadInfo;
    int socketDescriptor = socket(ati->ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
    if (socketDescriptor < 0)
        error("ERROR: Could not create socket");
    struct sockaddr_in servAddr;
    struct sockaddr_in6 servAddr6;
    if (ati->ipv6) {
        memset(&servAddr6, 0, sizeof(servAddr6));
        servAddr6.sin6_family = AF_INET6;
        servAddr6.sin6_port = htons(ati->portNumber);
        servAddr6.sin6_addr = in6addr_any;
    } else {
        memset(&servAddr, 0, sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        servAddr.sin_addr.s_addr = INADDR_ANY;
        servAddr.sin_port = htons(ati->portNumber);
    }
    if (bind(socketDescriptor, ati->ipv6 ? (struct sockaddr*)&servAddr6 : (struct sockaddr*)&servAddr, ati->ipv6 ? sizeof(servAddr6) : sizeof(servAddr)) < 0)
        error("ERROR: Could not bind socket");
    if (listen(socketDescriptor, 10) < 0)
        error("ERROR: Could not start listening on socket");
    while (1) {
        struct sockaddr_in cliAddr;
        struct sockaddr_in6 cliAddr6;
        socklen_t cliAddrLen;
        int acSocketDescriptor = accept(socketDescriptor, ati->ipv6 ? (struct sockaddr*)&cliAddr6 : (struct sockaddr*)&cliAddr, &cliAddrLen);
        if (acSocketDescriptor < 0)
            error("ERROR: Error on accepting");
        //TODO: here should be IP output
        TalkerThreadInfo* tti = malloc(sizeof(TalkerThreadInfo));
        tti->socketDescriptor = acSocketDescriptor;
        if (pthread_create(&tti->threadId, NULL, threadTalker, (void*)tti) != 0)
            error("ERROR: Could not create talker thread");
    }
}

int main(int argc, char* argv[]) {
    int n = argc - 1;
    AcceptorThreadInfo* atis = malloc(2 * n * sizeof(AcceptorThreadInfo));
    int i;
    for (i = 0; i < n; ++i) {
        atis[2 * i].portNumber = atis[2 * i + 1].portNumber = atoi(argv[i + 1]);
        atis[2 * i].ipv6 = 0;
        atis[2 * i + 1].ipv6 = 1;
    }
    for (i = 0; i < 2 * n; ++i) {
        if (pthread_create(&atis[i].threadId, NULL, threadAcceptor, (void*)&atis[i]) != 0)
            error("ERROR: Could not create acceptor thread");
    }
    //for (i = 0; i < 2 * n; ++i) {
        if (pthread_join(atis[0 /* i */].threadId, NULL) != 0)
            error("ERROR: Could not join to acceptor thread");
    //}
    //free(atis);

    return 0;
}
