#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define FIFO_PATH "/run/quartermaster.fifo"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <target>\n", argv[0]);
        return 2;
    }

    const char *target = argv[1];

    char msg[256];

    int target_name = snprintf(msg, sizeof msg, "%s\n", target);

    if (target_name < 0 || (size_t)target_name >= sizeof msg) {
        fprintf(stderr, "target name invalid size\n");
        return 2;
    }

    int fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        if (errno == ENXIO || errno == ENOENT) return 0;
        fprintf(stderr, "open %s: %s\n", FIFO_PATH, strerror(errno));
        return 1;
    }

    size_t left = (size_t)target_name;

    const char *msg_p = msg;

    while (left > 0) {
        ssize_t writer = write(fd, msg_p, left);

        if (writer < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "write: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        left -= (size_t)writer;
        msg_p += writer;
    }

    close(fd);
    return 0;
}
