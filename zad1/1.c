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

typedef struct
{
    int fd[3][2];
} Pipes;

void work(Pipes* pipes, int vertix)
{
    int close_whole = (vertix + 1) % 3;  // zamykasz caly pipe na przeciwko
    int close_read = vertix;             // zamykasz reada na pipe ze swoim indeksem, nie potrzebujesz go;
    int close_write = (vertix + 2) % 3;  // zamykasz write tam skad czytasz;

    if (close(pipes->fd[close_read][0]) == -1)
        ERR("close");

    if (close(pipes->fd[close_write][1]) == -1)
        ERR("close");

    for (int i = 0; i < 2; i++)
        if (close(pipes->fd[close_whole][i]) == -1)
            ERR("close");

    int no_write = vertix;
    int no_read = (vertix + 2) % 3;
    pid_t pid = getpid();
    srand(pid);
    // poczatek petli
    if (vertix == 0)
    {
        ssize_t nwrite;
        int number = 1;
        if ((nwrite = write(pipes->fd[no_write][1], &number, sizeof(int))) == -1)
            ERR("write");
    }
    ssize_t nread;
    ssize_t nwrite;
    int buffer_number;

    while (1)
    {
        if ((nread = read(pipes->fd[no_read][0], &buffer_number, sizeof(int))) == -1)
            ERR("read");
        else if (nread == 0 ||
                 buffer_number == 0)  // case ze EOF dostajesz albo ze wylosowal gosc 0, wtedy konczysz petle
            break;
        else
        {
            printf("[%d]  Dostalem %d, lece dalej\n", pid, buffer_number);
            buffer_number += -10 + (rand() % 21);
            if ((nwrite = write(pipes->fd[no_write][1], &buffer_number, sizeof(int))) == -1)
            {
                if (errno == EPIPE)
                {
                    break;
                }
                ERR("write");
            }
        }
    }
    if (close(pipes->fd[no_read][0]) == -1)
        ERR("close");
    if (close(pipes->fd[no_write][1]) == -1)
        ERR("close");
    return;
}

void create_children(Pipes* pipes)
{
    pid_t pid;
    for (int i = 1; i < 3; i++)  // dzieci dostaja indeks jeden albo dwa
    {
        if ((pid = fork()) == -1)
            ERR("fork");
        if (pid == 0)
        {
            work(pipes, i);
            exit(EXIT_SUCCESS);
        }
    }
    work(pipes, 0);
    while (wait(NULL) > 0)
        ;
}

int main(int argc, char** argv)
{
    int result;

    Pipes pipes;

    for (int i = 0; i < 3; i++)
    {
        if ((result = pipe(pipes.fd[i])) == -1)
        {
            ERR("pipe fail");
        }
    }

    create_children(&pipes);

    return EXIT_SUCCESS;
}
