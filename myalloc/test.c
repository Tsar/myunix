#include <stdio.h>
#include <stdlib.h>

int main() {
    unsigned char* buf = (unsigned char*)malloc(500);

    int i;
    for (i = 0; i < 500; ++i)
        buf[i] = i % 0xFF;
    for (i = 500 - 1; i >= 0; --i)
        printf("%d ", buf[i]);
    printf("\n");
    
    buf = (unsigned char*)realloc(buf, 38400);
    
    for (i = 0; i < 38400; ++i)
        buf[i] = (50 + i) % 0xFF;
    for (i = 38400 - 1; i >= 0; --i) {
        if (buf[i] != (50 + i) % 0xFF)
            printf("BIG FUCK\n");
    }

    free(buf);

    return 0;
}
