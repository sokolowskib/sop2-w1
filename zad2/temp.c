#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

int main(int argc, char** argv)
{
    char write_buf[16];
    ssize_t printed;
    printed = snprintf(write_buf, sizeof(write_buf), "new_round");
    memset(write_buf + printed + 1, '0', 16 - printed - 1);
    printf("%s\n", write_buf);

    return EXIT_SUCCESS;
}
