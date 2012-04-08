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

ThreadInfo* getCurThreadInfo() {
    pthread_t curThreadId = pthread_self();
    ThreadInfo* curThreadInfo = threadInfo[curThreadId % HASHMAP_SIZE];
    while (curThreadInfo != 0 || curThreadInfo->threadId != curThreadId)
        curThreadInfo = curThreadInfo->next;
    if (curThreadInfo == 0) {
        curThreadInfo = (ThreadInfo*)mmap(0, sizeof(ThreadInfo), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
        curThreadInfo->threadId = curThreadId;
        curThreadInfo->bigBucketList = 0;
        curThreadInfo->bbListSize = 0;
        curThreadInfo->smallBucketList = 0;
        curThreadInfo->sbListSize = 0;
        curThreadInfo->next = 0;
        
        curThreadInfo->next = threadInfo[curThreadId % HASHMAP_SIZE];
        threadInfo[curThreadId % HASHMAP_SIZE] = curThreadInfo;
    }
    return curThreadInfo;
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
    return b;
}

void destroyBucket(Bucket* b) {
    munmap((void*)b, sizeof(Bucket) + b->size);
}

void* malloc(size_t size) {
    ThreadInfo* curThreadInfo = getCurThreadInfo();
    if (size >= N) {
        Bucket* bb = searchBucketInList(curThreadInfo->bigBucketList, size);
        if (bb != 0) {
            deleteBucketFromList(bb, &curThreadInfo->bigBucketList);
            --curThreadInfo->bbListSize;
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
        Bucket* sb = searchBucketInList(curThreadInfo->smallBucketList, size);
        if (sb != 0) {
            deleteBucketFromList(sb, &curThreadInfo->smallBucketList);
            --curThreadInfo->sbListSize;
            return sb->ptr;
        }
        sb = createNewBucket(size);
        return sb->ptr;
    }
}

void free(void* ptr) {
    ThreadInfo* curThreadInfo = getCurThreadInfo();
    Bucket* curBucket = (Bucket*)(ptr - sizeof(Bucket));
    if (curBucket->size >= N) {
        addBucketToList(curBucket, &curThreadInfo->bigBucketList);
        ++curThreadInfo->bbListSize;
        if (curThreadInfo->bbListSize >= MAX_BB_LIST_SIZE) {
            //I think, it's better to move out (to globalBigBucketList) NOT curBucket,
            //because there can be smaller buckets holding place in list, so we will alloc and free bigger bucket every time.
            //Moving out smallest bucket is also a bad idea: this means, that sum size of buckets in list will only increase.
            //So I'll take the last bucket in list (adding to list adds to front, so that's a kind of a queue).
            Bucket* toMoveToGlobalBBList = getLastBucketInList(curThreadInfo->bigBucketList);
            deleteBucketFromList(toMoveToGlobalBBList, &curThreadInfo->bigBucketList);
            --curThreadInfo->bbListSize;

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
        //..
    }
}
