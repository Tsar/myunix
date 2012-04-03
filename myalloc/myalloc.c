#include <stdio.h>

void* malloc(size_t size) {
    printf("malloc(%zu)\n", size);
    return (void*)69;
}

void free(void* ptr) {
    printf("free(%zu)\n", (size_t)ptr);
}
