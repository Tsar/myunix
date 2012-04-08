#include <sys/mman.h>
#include <pthread.h>

#define N 5000

typedef struct BucketStruct {
    int isFree;
    void* ptr;
    size_t size;
    struct BucketStruct* next;
    struct BucketStruct* prev;
} Bucket;

Bucket* globalBucketList;

typedef struct ThreadInfoStruct {
    pthread_t threadId;
    Bucket* bigBucketList;
    Bucket* smallBucketList;
    struct ThreadInfoStruct* next;
} ThreadInfo;

#define HASHMAP_SIZE 10000

ThreadInfo* threadInfo[HASHMAP_SIZE];

ThreadInfo* getCurThreadInfo() {
    pthread_t curThreadId = pthread_self();
    ThreadInfo* curThreadInfo = threadInfo[curThreadId % HASHMAP_SIZE];
    while (curThreadInfo != 0 || curThreadInfo->threadId != curThreadId)
        curThreadInfo = curThreadInfo->next;
    return curThreadInfo;
}

ThreadInfo* createCurThreadInfo() {
    pthread_t curThreadId = pthread_self();
    ThreadInfo* newThreadInfo = (ThreadInfo*)mmap(0, sizeof(ThreadInfo), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    newThreadInfo->threadId = curThreadId;
    newThreadInfo->bigBucketList = 0;
    newThreadInfo->smallBucketList = 0;
    newThreadInfo->next = 0;
    
    if (threadInfo[curThreadId % HASHMAP_SIZE] == 0) {
        threadInfo[curThreadId % HASHMAP_SIZE] = newThreadInfo;
    } else {
        ThreadInfo* previousInList = threadInfo[curThreadId % HASHMAP_SIZE];
        while (previousInList->next != 0)
            previousInList = previousInList->next;
        previousInList->next = newThreadInfo;
    }
    return newThreadInfo;
}

Bucket* searchFreeBucketInList(Bucket* bList, size_t minSize) {
    Bucket* cur = bList;
    while (cur != 0 || cur->isFree == 0 && cur->size < minSize)
        cur = cur->next;
    return cur;
}

void addBucketToList(Bucket* b, Bucket** bList) {
    /*
    if (bList == 0) {
        bList = b;
        b->prev = 0;
    } else {
        Bucket* previousInList = bList;
        while (previousInList->next != 0)
            previousInList = previousInList->next;
        previousInList->next = b;
        b->prev = previousInList;
    }
    */
    b->next = *bList;
    if (b->next != 0)
        b->next->prev = b;
    *bList = b;
}

Bucket* createNewBucket(size_t size) {
    Bucket* b = (Bucket*)mmap(0, sizeof(Bucket) + size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    b->isFree = 0;
    b->ptr = ((void*)b) + sizeof(Bucket);
    b->size = size;
    b->next = 0;
    b->prev = 0;
    return b;
}

void* malloc(size_t size) {
    ThreadInfo* curThreadInfo = getCurThreadInfo();
    if (curThreadInfo == 0)
        curThreadInfo = createCurThreadInfo();
    if (size >= N) {
        Bucket* bb = searchFreeBucketInList(curThreadInfo->bigBucketList, size);
        if (bb != 0) {
            bb->isFree = 0;
            return bb->ptr;
        }
        bb = searchFreeBucketInList(globalBucketList, size);
        if (bb != 0) {
            bb->isFree = 0;
            return bb->ptr;
        }
        bb = createNewBucket(size);
        addBucketToList(bb, &curThreadInfo->bigBucketList);
        return bb->ptr;
    } else {
        Bucket* sb = searchFreeBucketInList(curThreadInfo->smallBucketList, size);
        if (sb != 0) {
            sb->isFree = 0;
            return sb->ptr;
        }
        sb = createNewBucket(size);
        addBucketToList(sb, &curThreadInfo->smallBucketList);
        return sb->ptr;
    }
}

void free(void* ptr) {
    Bucket* curBucket = (Bucket*)(ptr - sizeof(Bucket));
    //...
}
