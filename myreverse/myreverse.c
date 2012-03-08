#include <fcntl.h>

#define K 10

void reverseBuffer(char* buffer, unsigned int len) {
    unsigned int i, mid = len / 2;
    for (i = 0; i < mid; ++i) {
        char tmp = buffer[i];
        buffer[i] = buffer[len - i - 1];
        buffer[len - i - 1] = tmp;
    }
}

int main() {
    char buf[K + 1];
    
    while (1) {
        int n = read(0, buf, K + 1);
        
        if (n < 1)
            continue;
        int cont = 0;
        while (buf[n - 1] != '\n') {
            n = read(0, buf, K + 1);
            if (n < 1 || buf[n - 1] == '\n') {
                cont = 1;
                continue;
            }
        }
        if (cont == 1)
            continue;
        
        reverseBuffer(buf, --n);
        write(0, buf, n);
        write(0, "\n", 1);
    }
    
    return 0;
}
