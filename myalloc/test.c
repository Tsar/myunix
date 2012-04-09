#include <stdio.h>
#include <stdlib.h>

int main() {
    unsigned char* buf = (unsigned char*)malloc(500);

    int i;
    for (i = 0; i < 500; ++i)
        buf[i] = i % 0xFF;
    for (i = 500 - 1; i >= 0; --i)
        printf("%d ", buf[i]);
    
    buf = (unsigned char*)realloc(buf, 30000);
    
    for (i = 0; i < 30000; ++i)
        buf[i] = (50 + i) % 0xFF;
    for (i = 30000 - 1; i >= 0; --i)
        printf((buf[i] == (50 + i) % 0xFF) ? "+" : "FUCK");

    free(buf);

    return 0;
}
