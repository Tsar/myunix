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

#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT

#include <fcntl.h>

#define NUMBER_BUFFER_SIZE 12

void writeNumber(size_t number) {
    char buf[NUMBER_BUFFER_SIZE] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    int i = NUMBER_BUFFER_SIZE - 1;
    if (number == 0) {
        buf[i--] = 'O';
        buf[i--] = 'R';
        buf[i--] = 'E';
        buf[i--] = 'Z';
    } else {
        while (i >= 0 && number % 10 > 0) {
            buf[i--] = '0' + (number % 10);
            number /= 10;
        }
    }
    write(1, buf, NUMBER_BUFFER_SIZE);
}

#endif

ThreadInfo* getThreadInfo(pthread_t tId) {
    ThreadInfo* tInfo = threadInfo[tId % HASHMAP_SIZE];
    while (tInfo != 0 && tInfo->threadId != tId)
        tInfo = tInfo->next;
    if (tInfo == 0) {
        tInfo = (ThreadInfo*)mmap(0, sizeof(ThreadInfo), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
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
    Bucket* b = (Bucket*)mmap(0, sizeof(Bucket) + size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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
            return bb->ptr;
        }
        bb = searchBucketInList(gbbList, size);
        if (bb != 0) {
            pthread_mutex_lock(&gbbListMutex);
            deleteBucketFromList(bb, &gbbList, &gbbListTail);
            --gbbListSize;
            pthread_mutex_unlock(&gbbListMutex);
            return bb->ptr;
        }
        bb = createNewBucket(size);
        res = bb->ptr;
    } else {
        Bucket* sb = searchBucketInList(tInfo->sbList, size);
        if (sb != 0) {
            pthread_mutex_lock(&tInfo->sbListMutex);
            deleteBucketFromList(sb, &tInfo->sbList, &tInfo->sbListTail);
            --tInfo->sbListSize;
            pthread_mutex_unlock(&tInfo->sbListMutex);
            return sb->ptr;
        }
        sb = createNewBucket(size);
        res = sb->ptr;
    }

#ifdef DEBUG_OUTPUT
    write(1, " malloc(", 8);
    writeNumber(size);
    write(1, ") = ", 4);
    writeNumber((size_t)res);
    write(1, "\n", 1);
#endif

    return res;
}

void free(void* ptr) {
    if (ptr == 0)
        return;

#ifdef DEBUG_OUTPUT
    write(1, "   free(", 8);
    writeNumber((size_t)ptr);
    write(1, ")\n", 2);
#endif

    Bucket* curBucket = (Bucket*)(ptr - sizeof(Bucket));
    if (curBucket->size >= N) {
        ThreadInfo* tInfo = getThreadInfo(pthread_self());

        pthread_mutex_lock(&tInfo->bbListMutex);
        addBucketToList(curBucket, &tInfo->bbList, &tInfo->bbListTail);
        ++tInfo->bbListSize;
        pthread_mutex_unlock(&tInfo->bbListMutex);
        if (tInfo->bbListSize >= MAX_BB_LIST_SIZE) {
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
            if (gbbListSize >= MAX_GB_LIST_SIZE) {
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
        if (tInfo->sbListSize >= MAX_SB_LIST_SIZE) {
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
    write(1, "realloc(", 8);
    writeNumber((size_t)ptr);
    write(1, ", ", 2);
    writeNumber(size);
    write(1, ")\n", 2);
#endif

    /*
    //Note: I could just write [free(ptr); return malloc(size);] and if current bucket is enough size, than malloc will give it again,
    //because it will be in front of the buckets list and searchBucketInList works from list's front. But to optimize and get rid of few
    //instructions, which put bucket to front of the list and than take it from there again (everything O(1)), I do a custom check.
    if (ptr == 0)
        return malloc(size);
    if (((Bucket*)(ptr - sizeof(Bucket)))->size >= size)
        return ptr;
    */

    free(ptr);
    return malloc(size);
}
