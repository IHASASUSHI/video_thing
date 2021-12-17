#include <stdio.h>

struct buffer {
    size_t length;
};

int main(void) {
    struct buffer i;
    struct buffer j;
    i.length = 4;
    j = i;
    i.length = 5;
    printf("%i %i", j.length, i.length);
}