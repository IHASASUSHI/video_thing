#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/videodev2.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

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

static struct video *videos;
static unsigned int size_videos;
static int idx;

static int hash_function(char *s) {
    int hash;
    while (1) {
        if ((int)s[0] >= (int)'0' && (int)s[0] <= (int)'9' || s[0] == '\0')
            break;
        s++;
    }
    hash = atoi(s);
    return hash;
}

static int expand_and_zero_array(struct video *videos, int size) {
    struct video *newarray = realloc(videos, size * sizeof(struct video));
    if (!newarray) {
        errno = 1;
        return -1;
    }
    videos = newarray;
    return 0;
}

static void errno_exit(const char *s) {
    fprintf(stderr, "%s error %d, %s\\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg) {
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

struct v4l2_buffer get_frame_user_ptr(char *dev_name) {
    fd_set fds;
    struct timeval tv;
    int r;
    struct v4l2_buffer buf;
    unsigned int i;
    printf("video size: %d\n", size_videos);

    idx = hash_function(dev_name);
    printf("hash done\n");

    if (idx >= size_videos) {
        exit(1);
    }

    FD_ZERO(&fds);
    FD_SET(videos[idx].fd, &fds);
    printf("fd setup done\n");

    /* Timeout. */
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    r = select(videos[idx].fd + 1, &fds, NULL, NULL, &tv);
    printf("select done\n");

    if (-1 == r) {
        if (EINTR != errno)
            errno_exit("select");
    }

    if (0 == r) {
        fprintf(stderr, "select timeout\\n");
        exit(EXIT_FAILURE);
    }

    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_USERPTR;

    if (-1 == xioctl(videos[idx].fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
            case EAGAIN:
                errno_exit("EAGAIN");
                break;
            case EIO:
                /* Could ignore EIO, see spec. */

                /* fall through */

            default:
                errno_exit("VIDIOC_DQBUF");
        }
    } else {
        for (i = 0; i < videos[idx].n_buffers; ++i) {
            printf("%d\n", i);
            if (buf.m.userptr == (unsigned long)videos[idx].buffers[i].start && buf.length == videos[idx].buffers[i].length)
                break;
        }
        printf("ith buffer is %d\n", i);

        assert(i < videos[idx].n_buffers);
        //if (-1 == xioctl(videos[idx].fd, VIDIOC_QBUF, &buf))
        //   errno_exit("VIDIOC_QBUF");

        printf("assertion done\n");

        return buf;
    }
}

static void stop_capturing(int idx) {
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(videos[idx].fd, VIDIOC_STREAMOFF, &type))
        errno_exit("VIDIOC_STREAMOFF");
}

static void start_capturing(int idx) {
    unsigned int i;
    enum v4l2_buf_type type;

    for (i = 0; i < videos[idx].n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;
        buf.index = i;
        buf.m.userptr = (unsigned long)videos[idx].buffers[i].start;
        buf.length = videos[idx].buffers[i].length;

        if (-1 == xioctl(videos[idx].fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF");
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(videos[idx].fd, VIDIOC_STREAMON, &type))
        errno_exit("VIDIOC_STREAMON");
}

static void uninit_device(int idx) {
    unsigned int i;

    for (i = 0; i < videos[idx].n_buffers; ++i)
        free(videos[idx].buffers[i].start);

    free(videos[idx].buffers);
}

static void init_userp(unsigned int buffer_size, int idx) {
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    if (-1 == xioctl(videos[idx].fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support "
                            "user pointer i/on",
                    videos[idx].dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    videos[idx].buffers = calloc(4, sizeof(*videos[idx].buffers));

    if (!videos[idx].buffers) {
        fprintf(stderr, "Out of memory\\n");
        exit(EXIT_FAILURE);
    }

    for (videos[idx].n_buffers = 0; videos[idx].n_buffers < 4; ++videos[idx].n_buffers) {
        videos[idx].buffers[videos[idx].n_buffers].length = buffer_size;
        videos[idx].buffers[videos[idx].n_buffers].start = malloc(buffer_size);

        if (!videos[idx].buffers[videos[idx].n_buffers].start) {
            fprintf(stderr, "Out of memory\\n");
            exit(EXIT_FAILURE);
        }
    }
}

static void init_device(int idx) {
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    if (-1 == xioctl(videos[idx].fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s is no V4L2 device\\n",
                    videos[idx].dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is no video capture device\\n",
                videos[idx].dev_name);
        exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming i/o\\n",
                videos[idx].dev_name);
        exit(EXIT_FAILURE);
    }

    /* Select video input, video standard and tune here. */

    CLEAR(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(videos[idx].fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl(videos[idx].fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
            }
        }
    } else {
        /* Errors ignored. */
    }

    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 3280;
    fmt.fmt.pix.height = 2464;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (-1 == xioctl(videos[idx].fd, VIDIOC_S_FMT, &fmt)) {
        switch (errno) {
            case EAGAIN:
                printf("EAGAIN\n");
                break;

            case EINVAL:
                printf("EINVAL, fmt.type field is invalid\n");
                break;

            case EBADR:
                printf("EBADR\n");
                break;

            case EBUSY:
                printf("EBUSY\n");
                break;

                /* fall through */
        }
        errno_exit("VIDIOC_S_FMT");
    }
    /*
    Note VIDIOC_S_FMT may change width and height.
    */
    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    init_userp(fmt.fmt.pix.sizeimage, idx);
}

void close_device(char *dev_name) {
    idx = hash_function(dev_name);
    if (idx >= size_videos) {
        exit(1);
    }
    stop_capturing(idx);
    printf("closed capture\n");
    uninit_device(idx);
    if (-1 == close(videos[idx].fd))
        errno_exit("close");

    videos[idx].fd = -1;
}

void open_device(char *dev_name) {
    struct stat st;
    idx = hash_function(dev_name);
    if (videos == NULL) {
        printf("callocing video0s\n");
        videos = calloc(4, sizeof(struct video) + 1);
        size_videos = 4;
    } else if (idx >= size_videos)
        expand_and_zero_array(videos, idx + 1);

    videos[idx].io = IO_METHOD_USERPTR;

    if (-1 == stat(dev_name, &st)) {
        fprintf(stderr, "Cannot identify '%s': %d, %s\\n",
                dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "%s is no device\n", dev_name);
        exit(EXIT_FAILURE);
    }

    videos[idx].fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == videos[idx].fd) {
        fprintf(stderr, "Cannot open '%s': %d, %s\\n",
                dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    init_device(idx);
    start_capturing(idx);
}

int main(int argc, char **argv) {
    FILE *fptr = fopen("test.jpg", "w");
    char *dev_name = "/dev/video10";
    struct v4l2_buffer frame;

    open_device(dev_name);
    frame = get_frame_user_ptr(dev_name);
    fwrite((void *)frame.m.userptr, frame.bytesused, 1, fptr);
    close_device(dev_name);
    fclose(fptr);
    fprintf(stderr, "\\n");
    return 0;
}
