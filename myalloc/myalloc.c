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

typedef struct ThreadInfoStruct {
    pthread_t threadId;
    Bucket* bigBucketList;
    int bbListSize;
    Bucket* smallBucketList;
    int sbListSize;
    struct ThreadInfoStruct* next;
} ThreadInfo;

ThreadInfo* threadInfo[HASHMAP_SIZE];

ThreadInfo* getThreadInfo(pthread_t tId) {
    ThreadInfo* tInfo = threadInfo[tId % HASHMAP_SIZE];
    while (tInfo != 0 || tInfo->threadId != tId)
        tInfo = tInfo->next;
    if (tInfo == 0) {
        tInfo = (ThreadInfo*)mmap(0, sizeof(ThreadInfo), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
        tInfo->threadId = tId;
        tInfo->bigBucketList = 0;
        tInfo->bbListSize = 0;
        tInfo->smallBucketList = 0;
        tInfo->sbListSize = 0;
        tInfo->next = 0;
        
        tInfo->next = threadInfo[tId % HASHMAP_SIZE];
        threadInfo[tId % HASHMAP_SIZE] = tInfo;
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

void* malloc(size_t size) {
    ThreadInfo* tInfo = getThreadInfo(pthread_self());
    if (size >= N) {
        Bucket* bb = searchBucketInList(tInfo->bigBucketList, size);
        if (bb != 0) {
            deleteBucketFromList(bb, &tInfo->bigBucketList);
            --tInfo->bbListSize;
            return bb->ptr;
        }
        bb = searchBucketInList(globalBigBucketList, size);
        if (bb != 0) {
            deleteBucketFromList(bb, &globalBigBucketList);
            --gbListSize;
            return bb->ptr;
        }
        bb = createNewBucket(size);
        return bb->ptr;
    } else {
        Bucket* sb = searchBucketInList(tInfo->smallBucketList, size);
        if (sb != 0) {
            deleteBucketFromList(sb, &tInfo->smallBucketList);
            --tInfo->sbListSize;
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

        addBucketToList(curBucket, &tInfo->bigBucketList);
        ++tInfo->bbListSize;
        if (tInfo->bbListSize >= MAX_BB_LIST_SIZE) {
            //I think, it's better to move out (to globalBigBucketList) NOT curBucket,
            //because there can be smaller buckets holding place in list, so we will alloc and free bigger bucket every time.
            //Moving out smallest bucket is also a bad idea: this means, that sum size of buckets in list will only increase.
            //So I'll take the last bucket in list (adding to list adds to front, so that's a kind of a queue).
            Bucket* toMoveToGlobalBBList = getLastBucketInList(tInfo->bigBucketList);
            deleteBucketFromList(toMoveToGlobalBBList, &tInfo->bigBucketList);
            --tInfo->bbListSize;

            addBucketToList(toMoveToGlobalBBList, &globalBigBucketList);
            ++gbListSize;
            if (gbListSize >= MAX_GB_LIST_SIZE) {
                Bucket* toDestroy = getLastBucketInList(globalBigBucketList);
                deleteBucketFromList(toDestroy, &globalBigBucketList);
                --gbListSize;

                destroyBucket(toDestroy);
            }
        }
    } else {
        ThreadInfo* tInfo = getThreadInfo(curBucket->creatorThreadId);

        addBucketToList(curBucket, &tInfo->smallBucketList);
        ++tInfo->sbListSize;
        if (tInfo->sbListSize >= MAX_SB_LIST_SIZE) {
            Bucket* toDestroy = getLastBucketInList(tInfo->smallBucketList);
            deleteBucketFromList(toDestroy, &tInfo->smallBucketList);
            --tInfo->sbListSize;

            destroyBucket(toDestroy);
        }
    }
}
