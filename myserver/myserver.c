#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <poll.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void* malloc(size_t size);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* destination, const void* source, size_t num);

#define MAX_CHAT_MSG_LEN  21
#define MSG_QUEUE_SIZE    10000

typedef struct {
    char msg[MAX_CHAT_MSG_LEN];
    int msgLen;
} MsgQueueElement;

typedef struct {
    int head;
    pthread_mutex_t mutex;
} MsgQueueHead;

MsgQueueElement msgQueueData[MSG_QUEUE_SIZE];
pthread_mutex_t msgQueueMutex;
MsgQueueHead* msgQueueHeads;
int msgQueueHeadsCount;
int msgQueueHeadsCapacity;
int msgQueueTail;

void initMsgQueue() {
    pthread_mutex_init(&msgQueueMutex, 0);
    msgQueueTail = 0;
    msgQueueHeadsCount = 0;
    msgQueueHeadsCapacity = 2 * sizeof(MsgQueueHead);
    msgQueueHeads = malloc(msgQueueHeadsCapacity);
}

void destroyMsgQueue() {
    int i;
    for (i = 0; i < msgQueueHeadsCount; ++i)
        pthread_mutex_destroy(&msgQueueHeads[i].mutex);
    free(msgQueueHeads);
    pthread_mutex_destroy(&msgQueueMutex);
}

void addToMsgQueue(const char* msg, int msgLen) {
    pthread_mutex_lock(&msgQueueMutex);
    int i;
    for (i = 0; i < msgQueueHeadsCount; ++i) {
        if ((msgQueueHeads[i].head + 1) % MSG_QUEUE_SIZE == msgQueueTail) {
            pthread_mutex_lock(&msgQueueHeads[i].mutex);
            msgQueueHeads[i].head = (msgQueueHeads[i].head + 1) % MSG_QUEUE_SIZE;  //drop message from head
            pthread_mutex_unlock(&msgQueueHeads[i].mutex);
        }
    }
    memcpy(msgQueueData[msgQueueTail].msg, msg, msgLen);
    msgQueueData[msgQueueTail].msgLen = msgLen;
    msgQueueTail = (msgQueueTail + 1) % MSG_QUEUE_SIZE;
    pthread_mutex_unlock(&msgQueueMutex);
}

MsgQueueHead* createNewMsgQueueHead() {
    pthread_mutex_lock(&msgQueueMutex);
    ++msgQueueHeadsCount;
    if (msgQueueHeadsCount > msgQueueHeadsCapacity) {
        msgQueueHeadsCapacity *= 2;
        msgQueueHeads = realloc(msgQueueHeads, msgQueueHeadsCapacity);
    }
    msgQueueHeads[msgQueueHeadsCount - 1].head = msgQueueTail;
    pthread_mutex_init(&msgQueueHeads[msgQueueHeadsCount - 1].mutex, 0);
    pthread_mutex_unlock(&msgQueueMutex);
    return &msgQueueHeads[msgQueueHeadsCount - 1];
}

void error(const char* msg) {
    perror(msg);
    exit(1);
}

typedef struct {
    int socketDescriptor;
    pthread_t threadSendId;
    pthread_t threadRecvId;
} TalkerThreadInfo;

typedef struct {
    int portNumber;
    int ipv6;
    pthread_t threadId;
} AcceptorThreadInfo;

static void* threadSend(void* talkerThreadInfo) {
    TalkerThreadInfo* tti = (TalkerThreadInfo*)talkerThreadInfo;
    struct pollfd pfd;
    pfd.fd = tti->socketDescriptor;
    pfd.events = POLLOUT;
    MsgQueueHead* msgQueueHead = createNewMsgQueueHead();
    while (1) {
        while (msgQueueHead->head == msgQueueTail);
        if (poll(&pfd, 1, -1) < 0)
            error("ERROR: poll crashed [sender thread]");
        if (pfd.revents & POLLOUT) {
            int n2 = 0;
            MsgQueueElement* mqe = &msgQueueData[msgQueueHead->head];
            while (n2 < mqe->msgLen) {
                int n2Delta = send(tti->socketDescriptor, mqe->msg + n2, mqe->msgLen - n2, 0);
                if (n2Delta == -1) {
                    //thinking what to do here
                }
                n2 += n2Delta;
            }
            pthread_mutex_lock(&msgQueueHead->mutex);
            msgQueueHead->head = (msgQueueHead->head + 1) % MSG_QUEUE_SIZE;
            pthread_mutex_unlock(&msgQueueHead->mutex);
        }
    }
}

static void* threadRecv(void* talkerThreadInfo) {
    TalkerThreadInfo* tti = (TalkerThreadInfo*)talkerThreadInfo;
    struct pollfd pfd;
    pfd.fd = tti->socketDescriptor;
    pfd.events = POLLIN;
    while (1) {
        if (poll(&pfd, 1, -1) < 0)
            error("ERROR: poll crashed [receiver thread]");
        if (pfd.revents & POLLIN) {
            char msg[MAX_CHAT_MSG_LEN];
            int msgLen = recv(tti->socketDescriptor, msg, MAX_CHAT_MSG_LEN, 0);
            if (msgLen > 0) {
                if (msg[msgLen - 1] != '\n')
                    while (recv(tti->socketDescriptor, msg, MAX_CHAT_MSG_LEN, 0) > 0);
                else
                    addToMsgQueue(msg, msgLen);
            }
        }
    }
}

static void* threadAcceptor(void* acceptorThreadInfo) {
    AcceptorThreadInfo* ati = (AcceptorThreadInfo*)acceptorThreadInfo;

    //temporary disabling IPv6
    if (ati->ipv6)
        return;

    int socketDescriptor = socket(ati->ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
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
        if (fcntl(acSocketDescriptor, F_SETFL, O_NONBLOCK) < 0)
            error("ERROR: fcntl failed");

        char cliAddrAsStr[INET6_ADDRSTRLEN];
        if (inet_ntop(ati->ipv6 ? AF_INET6 : AF_INET, ati->ipv6 ? (void*)&cliAddr6.sin6_addr : (void*)&cliAddr.sin_addr, cliAddrAsStr, INET6_ADDRSTRLEN) == NULL)
            error("ERROR: inet_ntop failed");
        printf("Connection from <%s>\n", cliAddrAsStr);

        TalkerThreadInfo* tti = malloc(sizeof(TalkerThreadInfo));
        tti->socketDescriptor = acSocketDescriptor;
        if (pthread_create(&tti->threadSendId, NULL, threadSend, (void*)tti) != 0)
            error("ERROR: Could not create sender thread");
        if (pthread_create(&tti->threadRecvId, NULL, threadRecv, (void*)tti) != 0)
            error("ERROR: Could not create receiver thread");
    }
}

int main(int argc, char* argv[]) {
    initMsgQueue();
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
    //destroyMsgQueue();

    return 0;
}
