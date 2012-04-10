#include <sys/mman.h>
#include <pthread.h>

#define N 5000

#define MAX_BB_LIST_SIZE 10
#define MAX_GB_LIST_SIZE 20
#define MAX_SB_LIST_SIZE 100

#define HASHMAP_SIZE 10000

typedef struct BucketStruct {
    void* ptr;
    size_t size;
    struct BucketStruct* next;
    struct BucketStruct* prev;
    pthread_t creatorThreadId;
} Bucket;

//global big buckets list
Bucket* gbbList;
Bucket* gbbListTail;
int gbbListSize = 0;
pthread_mutex_t gbbListMutex;

typedef struct ThreadInfoStruct {
    pthread_t threadId;

    //big buckets list
    Bucket* bbList;
    Bucket* bbListTail;
    int bbListSize;
    pthread_mutex_t bbListMutex;

    //small buckets list
    Bucket* sbList;
    Bucket* sbListTail;
    int sbListSize;
    pthread_mutex_t sbListMutex;

    struct ThreadInfoStruct* next;
} ThreadInfo;

ThreadInfo* threadInfo[HASHMAP_SIZE] = {};

pthread_mutex_t threadInfoMutex;

void* memcpy(void* destination, const void* source, size_t num);
void* memset(void* ptr, int value, size_t num);

//#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT

#include <fcntl.h>

#define NUMBER_BUFFER_SIZE 24

int debugOutputLineNumber = 0;

void writeNumber(size_t number, int bufLen) {
    char buf[NUMBER_BUFFER_SIZE];
    memset(buf, ' ', NUMBER_BUFFER_SIZE);
    int i = bufLen - 1;
    while (i >= 0) {
        buf[i--] = '0' + (number % 10);
        number /= 10;
        if (number == 0)
            break;
    }
    write(1, buf, bufLen);
}

void writeThreadId() {
    writeNumber(++debugOutputLineNumber, 7);
    write(1, "| thread[", 9);
    writeNumber(pthread_self(), NUMBER_BUFFER_SIZE);
    write(1, "]: ", 3);
}

#endif

ThreadInfo* getThreadInfo(pthread_t tId) {
    ThreadInfo* tInfo = threadInfo[tId % HASHMAP_SIZE];
    while (tInfo != 0 && tInfo->threadId != tId)
        tInfo = tInfo->next;
    if (tInfo == 0) {
        tInfo = (ThreadInfo*)mmap(0, sizeof(ThreadInfo), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#ifdef DEBUG_OUTPUT
        if (tInfo == MAP_FAILED) {
            writeThreadId();
            write(1, "mmap failed, crash coming...\n", 29);
        }
#endif
        tInfo->threadId = tId;
        tInfo->bbList = 0;
        tInfo->bbListTail = 0;
        tInfo->bbListSize = 0;
        tInfo->sbList = 0;
        tInfo->sbListTail = 0;
        tInfo->sbListSize = 0;
        tInfo->next = 0;
        pthread_mutex_init(&tInfo->bbListMutex, 0);
        pthread_mutex_init(&tInfo->sbListMutex, 0);
        
        pthread_mutex_lock(&threadInfoMutex);
        tInfo->next = threadInfo[tId % HASHMAP_SIZE];
        threadInfo[tId % HASHMAP_SIZE] = tInfo;
        pthread_mutex_unlock(&threadInfoMutex);
    }
    return tInfo;
}

#ifdef DEBUG_OUTPUT

void writeThreadInfo() {
    pthread_t tId = pthread_self();
    ThreadInfo* tInfo = getThreadInfo(tId);
    writeNumber(++debugOutputLineNumber, 7);
    write(1, "| thread[", 9);
    writeNumber(tId, NUMBER_BUFFER_SIZE);
    write(1, ", bb: ", 6);
    writeNumber(tInfo->bbListSize, 4);
    write(1, ", sb: ", 6);
    writeNumber(tInfo->sbListSize, 4);
    write(1, "]: ", 3);
}

#endif

Bucket* searchBucketInList(Bucket* bList, size_t minSize) {
    Bucket* cur = bList;
    while (cur != 0 && cur->size < minSize)
        cur = cur->next;
    return cur;
}

void addBucketToList(Bucket* b, Bucket** bList, Bucket** bListTail) {
    if (*bListTail == 0)
        *bListTail = b;
    b->next = *bList;
    if (b->next != 0)
        b->next->prev = b;
    *bList = b;
}

void deleteBucketFromList(Bucket* b, Bucket** bList, Bucket** bListTail) {
    if (b == *bList && b == *bListTail) {
        *bList = 0;
        *bListTail = 0;
        return;
    }
    if (b == *bList) {
        *bList = b->next;
        b->next->prev = 0;
        b->next = 0;
        return;
    }
    if (b == *bListTail) {
        *bListTail = b->prev;
         b->prev->next = 0;
         b->prev = 0;
         return;
    }
    if (b->next != 0)
        b->next->prev = b->prev;
    if (b->prev != 0)
        b->prev->next = b->next;
    b->next = 0;
    b->prev = 0;
}

Bucket* createNewBucket(size_t size) {
#ifdef DEBUG_OUTPUT
    writeThreadInfo();
    write(1, "createB(", 8);
    writeNumber(size, NUMBER_BUFFER_SIZE);
    write(1, ")\n", 2);
#endif

    Bucket* b = (Bucket*)mmap(0, sizeof(Bucket) + size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#ifdef DEBUG_OUTPUT
    if (b == MAP_FAILED) {
        writeThreadInfo();
        write(1, "mmap failed, crash coming...\n", 29);
    }
#endif
    b->ptr = ((void*)b) + sizeof(Bucket);
    b->size = size;
    b->next = 0;
    b->prev = 0;
    b->creatorThreadId = pthread_self();
    return b;
}

void destroyBucket(Bucket* b) {
    munmap((void*)b, sizeof(Bucket) + b->size);
}

void destroyBucketList(Bucket* bList) {
    if (bList == 0)
        return;
    if (bList->next != 0)
        destroyBucketList(bList->next);

    destroyBucket(bList);
}

void destroyThreadInfoList(ThreadInfo* tInfo) {
    if (tInfo == 0)
        return;
    destroyThreadInfoList(tInfo->next);
    pthread_mutex_destroy(&tInfo->bbListMutex);
    pthread_mutex_destroy(&tInfo->sbListMutex);
    destroyBucketList(tInfo->bbList);
    destroyBucketList(tInfo->sbList);

    munmap((void*)tInfo, sizeof(ThreadInfo));
}

void _init() {
    pthread_mutex_init(&threadInfoMutex, 0);
    pthread_mutex_init(&gbbListMutex, 0);
}

void _fini() {
    pthread_mutex_destroy(&threadInfoMutex);
    pthread_mutex_destroy(&gbbListMutex);

    int i;
    for (i = 0; i < HASHMAP_SIZE; ++i)
        destroyThreadInfoList(threadInfo[i]);

    destroyBucketList(gbbList);
}

void* malloc(size_t size) {
    ThreadInfo* tInfo = getThreadInfo(pthread_self());
    void* res;
    if (size >= N) {
        Bucket* bb = searchBucketInList(tInfo->bbList, size);
        if (bb != 0) {
            pthread_mutex_lock(&tInfo->bbListMutex);
            deleteBucketFromList(bb, &tInfo->bbList, &tInfo->bbListTail);
            --tInfo->bbListSize;
            pthread_mutex_unlock(&tInfo->bbListMutex);
            res = bb->ptr;
        } else {
            bb = searchBucketInList(gbbList, size);
            if (bb != 0) {
                pthread_mutex_lock(&gbbListMutex);
                deleteBucketFromList(bb, &gbbList, &gbbListTail);
                --gbbListSize;
                pthread_mutex_unlock(&gbbListMutex);
                res = bb->ptr;
            } else {
                bb = createNewBucket(size);
                res = bb->ptr;
            }
        }
    } else {
        Bucket* sb = searchBucketInList(tInfo->sbList, size);
        if (sb != 0) {
            pthread_mutex_lock(&tInfo->sbListMutex);
            deleteBucketFromList(sb, &tInfo->sbList, &tInfo->sbListTail);
            --tInfo->sbListSize;
            pthread_mutex_unlock(&tInfo->sbListMutex);
            res = sb->ptr;
        } else {
            sb = createNewBucket(size);
            res = sb->ptr;
        }
    }

#ifdef DEBUG_OUTPUT
    writeThreadInfo();
    write(1, "malloc (", 8);
    writeNumber(size, NUMBER_BUFFER_SIZE);
    write(1, ") = ", 4);
    writeNumber((size_t)res, NUMBER_BUFFER_SIZE);
    write(1, "\n", 1);
#endif

    return res;
}

void free(void* ptr) {
    if (ptr == 0)
        return;

#ifdef DEBUG_OUTPUT
    writeThreadInfo();
    write(1, "free   (", 8);
    writeNumber((size_t)ptr, NUMBER_BUFFER_SIZE);
    write(1, ")\n", 2);
#endif

    Bucket* curBucket = (Bucket*)(ptr - sizeof(Bucket));
    if (curBucket->size >= N) {
        ThreadInfo* tInfo = getThreadInfo(pthread_self());

        pthread_mutex_lock(&tInfo->bbListMutex);
        addBucketToList(curBucket, &tInfo->bbList, &tInfo->bbListTail);
        ++tInfo->bbListSize;
        pthread_mutex_unlock(&tInfo->bbListMutex);
        if (tInfo->bbListSize > MAX_BB_LIST_SIZE) {
            //I think, it's better to move out (to gbbList) NOT curBucket,
            //because there can be smaller buckets holding place in list, so we will alloc and free bigger bucket every time.
            //Moving out smallest bucket is also a bad idea: this means, that sum size of buckets in list will only increase.
            //So I'll take the last bucket in list (adding to list adds to front, so that's a kind of a queue).
            pthread_mutex_lock(&tInfo->bbListMutex);
            Bucket* toMoveToGlobalBBList = tInfo->bbListTail;
            deleteBucketFromList(toMoveToGlobalBBList, &tInfo->bbList, &tInfo->bbListTail);
            --tInfo->bbListSize;
            pthread_mutex_unlock(&tInfo->bbListMutex);

            pthread_mutex_lock(&gbbListMutex);
            addBucketToList(toMoveToGlobalBBList, &gbbList, &gbbListTail);
            ++gbbListSize;
            pthread_mutex_unlock(&gbbListMutex);
            if (gbbListSize > MAX_GB_LIST_SIZE) {
                pthread_mutex_lock(&gbbListMutex);
                Bucket* toDestroy = gbbListTail;
                deleteBucketFromList(toDestroy, &gbbList, &gbbListTail);
                --gbbListSize;
                pthread_mutex_unlock(&gbbListMutex);

                destroyBucket(toDestroy);
            }
        }
    } else {
        ThreadInfo* tInfo = getThreadInfo(curBucket->creatorThreadId);

        pthread_mutex_lock(&tInfo->sbListMutex);
        addBucketToList(curBucket, &tInfo->sbList, &tInfo->sbListTail);
        ++tInfo->sbListSize;
        pthread_mutex_unlock(&tInfo->sbListMutex);
        if (tInfo->sbListSize > MAX_SB_LIST_SIZE) {
            pthread_mutex_lock(&tInfo->sbListMutex);
            Bucket* toDestroy = tInfo->sbListTail;
            deleteBucketFromList(toDestroy, &tInfo->sbList, &tInfo->sbListTail);
            --tInfo->sbListSize;
            pthread_mutex_unlock(&tInfo->sbListMutex);

            destroyBucket(toDestroy);
        }
    }
}

void* realloc(void* ptr, size_t size) {
#ifdef DEBUG_OUTPUT
    writeThreadInfo();
    write(1, "realloc(", 8);
    writeNumber((size_t)ptr, NUMBER_BUFFER_SIZE);
    write(1, ", ", 2);
    writeNumber(size, NUMBER_BUFFER_SIZE);
    write(1, ")\n", 2);
#endif

    if (ptr == 0)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return 0;
    }
    size_t oldSize = ((Bucket*)(ptr - sizeof(Bucket)))->size;
    if (oldSize >= size && ((oldSize >= N && size >= N) || (oldSize < N && size < N)))
        return ptr;

    void* res = malloc(size);
    memcpy(res, ptr, oldSize < size ? oldSize : size);
    free(ptr);
    return res;
}

void* calloc(size_t num, size_t size) {
    void* res = malloc(num * size);
    return memset(res, 0, num * size);
}
