#include <stdio.h>

enum io_method {
    IO_METHOD_USERPTR
};

struct buffer {
    void *start;
    size_t length;
};

struct video {
    struct buffer *buffers;
    unsigned int n_buffers;
    char *dev_name;
    int fd;            //-1
    enum io_method io; // IO_METHOD_USERPTR
};
struct buffer *buffers;
unsigned int n_buffers;
int fd; //-1
char *dev_name;
enum io_method io; // IO_METHOD_USERPTR

int main(void) {
    printf("%i\n", sizeof(struct video));
    printf("%i\n", sizeof(buffers));
    printf("%i\n", sizeof(dev_name));
    printf("%i\n", sizeof(n_buffers));
    printf("%i\n", sizeof(fd));
    printf("%i\n", sizeof(io));
}