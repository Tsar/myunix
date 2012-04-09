#include <stdio.h>
#include <stdlib.h>

int main() {
    int n = 500;
    unsigned char* buf = (unsigned char*)malloc(500);

    int i;
    for (i = 0; i < 500; ++i) {
        buf[i] = i % 0xFF;
    }
    for (i = 500 - 1; i >= 0; --i) {
        printf("%d ", buf[i]);
    }

    free(buf);

    return 0;
}
