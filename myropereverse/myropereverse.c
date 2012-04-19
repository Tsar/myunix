#include <fcntl.h>

// == ROPE REALISATION START ==

typedef struct RopeNodeStruct {
    int weight;
    struct RopeNodeStruct* left;
    struct RopeNodeStruct* right;
    char* s;
} RopeNode;

typedef struct {
    RopeNode* node1;
    RopeNode* node2;
} RopeNodePair;

char ropeIndex(RopeNode* node, int i) {
    if (node->weight <= i)
        return ropeIndex(node->right, i - node->weight);
    if (node->left)
        return ropeIndex(node->left, i);
    return node->s[i];
}

void ropeSplit(int i, RopeNode* node, RopeNodePair* result) {
    //...
}

// == ROPE REALISATION END ==

#define K 10

void* memmove(void* destination, const void* source, size_t num);

void reverseBuffer(char* buffer, int len) {
    int i, mid = len / 2;
    for (i = 0; i < mid; ++i) {
        char tmp = buffer[i];
        buffer[i] = buffer[len - i - 1];
        buffer[len - i - 1] = tmp;
    }
}

int main() {
    char buf[K];
    int len = 0;
    
    int s = 0;
    while (1) {
        int n = read(0, buf + len, K - len);
        if (n < 0)
            return -1;
        len += n;
        if (len == 0)
            break;
        
        int x;
        for (x = 0; x < len && buf[x] != '\n'; ++x);

        if (x == len) {
            if (x == K) {
                len = 0;
                s = 1;
            } else if (n == 0) {
                break;
            }
        } else {
            if (s) {
                s = 0;
                memmove(buf, buf + (x + 1), len - (x - 1));
                len -= (x + 1);
            } else {
                reverseBuffer(buf, x);
                int n2 = 0;
                while (n2 < x + 1) {
                    int n2Delta = write(1, buf + n2, x + 1 - n2);
                    if (n2Delta == -1)
                        return -1;
                    n2 += n2Delta;
                }
                memmove(buf, buf + (x + 1), len - (x - 1));
                len -= (x + 1);
            }
        }
    }
    
    return 0;
}
