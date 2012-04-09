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

Bucket* globalBigBucketList;
int gbListSize = 0;
pthread_mutex_t gbListMutex;

typedef struct ThreadInfoStruct {
    pthread_t threadId;
    Bucket* bigBucketList;
    int bbListSize;
    pthread_mutex_t bbListMutex;
    Bucket* smallBucketList;
    int sbListSize;
    pthread_mutex_t sbListMutex;
    struct ThreadInfoStruct* next;
} ThreadInfo;

ThreadInfo* threadInfo[HASHMAP_SIZE] = {};

pthread_mutex_t threadInfoMutex;

ThreadInfo* getThreadInfo(pthread_t tId) {
    ThreadInfo* tInfo = threadInfo[tId % HASHMAP_SIZE];
    while (tInfo != 0 && tInfo->threadId != tId)
        tInfo = tInfo->next;
    if (tInfo == 0) {
        tInfo = (ThreadInfo*)mmap(0, sizeof(ThreadInfo), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
        tInfo->threadId = tId;
        tInfo->bigBucketList = 0;
        tInfo->bbListSize = 0;
        tInfo->smallBucketList = 0;
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
    while (cur != 0 || cur->size < minSize)
        cur = cur->next;
    return cur;
}

void addBucketToList(Bucket* b, Bucket** bList) {
    b->next = *bList;
    if (b->next != 0)
        b->next->prev = b;
    *bList = b;
}

void deleteBucketFromList(Bucket* b, Bucket** bList) {
    if (b == *bList) {
        *bList = b->next;
        b->next->prev = 0;
        b->next = 0;
        return;
    }
    if (b->next != 0)
        b->next->prev = b->prev;
    if (b->prev != 0)
        b->prev->next = b->next;
    b->next = 0;
    b->prev = 0;
}

Bucket* getLastBucketInList(Bucket* bList) {
    if (bList == 0)
        return 0;
    Bucket* cur = bList;
    while (cur->next != 0)
        cur = cur->next;
    return cur;
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
    destroyBucketList(tInfo->bigBucketList);
    destroyBucketList(tInfo->smallBucketList);

    munmap((void*)tInfo, sizeof(ThreadInfo));
}

void _init() {
    pthread_mutex_init(&threadInfoMutex, 0);
    pthread_mutex_init(&gbListMutex, 0);
}

void _fini() {
    pthread_mutex_destroy(&threadInfoMutex);
    pthread_mutex_destroy(&gbListMutex);

    int i;
    for (i = 0; i < HASHMAP_SIZE; ++i)
        destroyThreadInfoList(threadInfo[i]);

    destroyBucketList(globalBigBucketList);
}

void* malloc(size_t size) {
    ThreadInfo* tInfo = getThreadInfo(pthread_self());
    if (size >= N) {
        Bucket* bb = searchBucketInList(tInfo->bigBucketList, size);
        if (bb != 0) {
            pthread_mutex_lock(&tInfo->bbListMutex);
            deleteBucketFromList(bb, &tInfo->bigBucketList);
            --tInfo->bbListSize;
            pthread_mutex_unlock(&tInfo->bbListMutex);
            return bb->ptr;
        }
        bb = searchBucketInList(globalBigBucketList, size);
        if (bb != 0) {
            pthread_mutex_lock(&gbListMutex);
            deleteBucketFromList(bb, &globalBigBucketList);
            --gbListSize;
            pthread_mutex_unlock(&gbListMutex);
            return bb->ptr;
        }
        bb = createNewBucket(size);
        return bb->ptr;
    } else {
        Bucket* sb = searchBucketInList(tInfo->smallBucketList, size);
        if (sb != 0) {
            pthread_mutex_lock(&tInfo->sbListMutex);
            deleteBucketFromList(sb, &tInfo->smallBucketList);
            --tInfo->sbListSize;
            pthread_mutex_unlock(&tInfo->sbListMutex);
            return sb->ptr;
        }
        sb = createNewBucket(size);
        return sb->ptr;
    }
}

void free(void* ptr) {
    Bucket* curBucket = (Bucket*)(ptr - sizeof(Bucket));
    if (curBucket->size >= N) {
        ThreadInfo* tInfo = getThreadInfo(pthread_self());

        pthread_mutex_lock(&tInfo->bbListMutex);
        addBucketToList(curBucket, &tInfo->bigBucketList);
        ++tInfo->bbListSize;
        pthread_mutex_unlock(&tInfo->bbListMutex);
        if (tInfo->bbListSize >= MAX_BB_LIST_SIZE) {
            //I think, it's better to move out (to globalBigBucketList) NOT curBucket,
            //because there can be smaller buckets holding place in list, so we will alloc and free bigger bucket every time.
            //Moving out smallest bucket is also a bad idea: this means, that sum size of buckets in list will only increase.
            //So I'll take the last bucket in list (adding to list adds to front, so that's a kind of a queue).
            pthread_mutex_lock(&tInfo->bbListMutex);
            Bucket* toMoveToGlobalBBList = getLastBucketInList(tInfo->bigBucketList);
            deleteBucketFromList(toMoveToGlobalBBList, &tInfo->bigBucketList);
            --tInfo->bbListSize;
            pthread_mutex_unlock(&tInfo->bbListMutex);

            pthread_mutex_lock(&gbListMutex);
            addBucketToList(toMoveToGlobalBBList, &globalBigBucketList);
            ++gbListSize;
            pthread_mutex_unlock(&gbListMutex);
            if (gbListSize >= MAX_GB_LIST_SIZE) {
                pthread_mutex_lock(&gbListMutex);
                Bucket* toDestroy = getLastBucketInList(globalBigBucketList);
                deleteBucketFromList(toDestroy, &globalBigBucketList);
                --gbListSize;
                pthread_mutex_unlock(&gbListMutex);

                destroyBucket(toDestroy);
            }
        }
    } else {
        ThreadInfo* tInfo = getThreadInfo(curBucket->creatorThreadId);

        pthread_mutex_lock(&tInfo->sbListMutex);
        addBucketToList(curBucket, &tInfo->smallBucketList);
        ++tInfo->sbListSize;
        pthread_mutex_unlock(&tInfo->sbListMutex);
        if (tInfo->sbListSize >= MAX_SB_LIST_SIZE) {
            pthread_mutex_lock(&tInfo->sbListMutex);
            Bucket* toDestroy = getLastBucketInList(tInfo->smallBucketList);
            deleteBucketFromList(toDestroy, &tInfo->smallBucketList);
            --tInfo->sbListSize;
            pthread_mutex_unlock(&tInfo->sbListMutex);

            destroyBucket(toDestroy);
        }
    }
}
