#include <sys/mman.h>
#include <pthread.h>

#define N 5000

typedef struct BigBucketStruct {
    int isFree;
    void* ptr;
    size_t size;
    struct BigBucketStruct* next;
} BigBucket;

typedef struct SmallBucketStruct {
    int isFree;
    void* ptr;
    size_t size;
    struct SmallBucketStruct* next;
} SmallBucket;

BigBucket* globalBigBucketList;

typedef struct ThreadInfoStruct {
    pthread_t threadId;
    BigBucket* bigBucketList;
    SmallBucket* smallBucketList;
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
        ThreadInfo* prev = threadInfo[curThreadId % HASHMAP_SIZE];
        while (prev->next != 0)
            prev = prev->next;
        prev->next = newThreadInfo;
    }
    return newThreadInfo;
}

BigBucket* searchFreeBigBucketInList(BigBucket* bbList, size_t minSize) {
    BigBucket* cur = bbList;
    while (cur != 0 || cur->isFree == 0 && cur->size < minSize)
        cur = cur->next;
    return cur;
}

void addBigBucketToList(BigBucket* bb, BigBucket* bbList) {
    if (bbList == 0) {
        bbList = bb;
    } else {
        BigBucket* prev = bbList;
        while (prev->next != 0)
            prev = prev->next;
        prev->next = bb;
    }
}

SmallBucket* searchFreeSmallBucketInList(SmallBucket* sbList, size_t minSize) {
    SmallBucket* cur = sbList;
    while (cur != 0 || cur->isFree == 0 && cur->size < minSize)
        cur = cur->next;
    return cur;
}

void addSmallBucketToList(SmallBucket* sb, SmallBucket* sbList) {
    if (sbList == 0) {
        sbList = sb;
    } else {
        SmallBucket* prev = sbList;
        while (prev->next != 0)
            prev = prev->next;
        prev->next = sb;
    }
}

void* malloc(size_t size) {
    ThreadInfo* curThreadInfo = getCurThreadInfo();
    if (curThreadInfo == 0)
        curThreadInfo = createCurThreadInfo();
    if (size > N) {
        BigBucket* bb = searchFreeBigBucketInList(curThreadInfo->bigBucketList, size);
        if (bb != 0) {
            bb->isFree = 0;
            return bb->ptr;
        }
        bb = searchFreeBigBucketInList(globalBigBucketList, size);
        if (bb != 0) {
            bb->isFree = 0;
            return bb->ptr;
        }
        bb = (BigBucket*)mmap(0, sizeof(BigBucket) + size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        bb->isFree = 0;
        bb->ptr = ((void*)bb) + sizeof(BigBucket);
        bb->size = size;
        bb->next = 0;
        addBigBucketToList(bb, curThreadInfo->bigBucketList);
        return bb->ptr;
    } else {
        SmallBucket* sb = searchFreeSmallBucketInList(curThreadInfo->smallBucketList, size);
        if (sb != 0) {
            sb->isFree = 0;
            return sb->ptr;
        }
        sb = (SmallBucket*)mmap(0, sizeof(SmallBucket) + size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        sb->isFree = 0;
        sb->ptr = ((void*)sb) + sizeof(SmallBucket);
        sb->size = size;
        sb->next = 0;
        addSmallBucketToList(sb, curThreadInfo->smallBucketList);
        return sb->ptr;
    }
}

void free(void* ptr) {
    //
}
