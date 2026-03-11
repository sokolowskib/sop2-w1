#include <math.h>
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define MESSAGE_LEN 16
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct
{
    int pipe0[2];
    int pipe1[2];
} PipeContainer;

typedef struct
{
    PipeContainer* container;
} Pipes;

void usage()
{
    printf("Program bierze dwa argumenty:\n");
    printf("\t 2<= N <= 5");
    printf("\t 5<= M <= 10");
}

void player_work(int N, int M, Pipes* pipes, int index)
{
    char read_buf[MESSAGE_LEN];
    char write_buf[MESSAGE_LEN];
    int round_counter = 0;
    bool used_cards[M];
    ssize_t nread, nwrite;
    pid_t pid = getpid();
    srand(pid);
    for (int i = 0; i < M; i++)
    {
        used_cards[i] = false;
    }

    // zamykanie nie potrzebnych pipe
    for (int i = 0; i < N; i++)
    {
        if (i != index)
        {
            for (int j = 0; j < 2; j++)
            {
                if (close(pipes->container[i].pipe0[j]) == -1)
                    ERR("close");
                if (close(pipes->container[i].pipe1[j]) == -1)
                    ERR("close");
            }
        }
        if (i == index)
        {
            if (close(pipes->container[i].pipe0[1]) == -1)
                ERR("close");
            if (close(pipes->container[i].pipe1[0]) == -1)
                ERR("close");
        }
    }
    while (round_counter < M)
    {
        if ((nread = read(pipes->container[index].pipe0[0], &read_buf, sizeof(read_buf))) == -1)
            ERR("read");
        else if (nread == 0)
        {
            close(pipes->container[index].pipe0[0]);
            close(pipes->container[index].pipe1[1]);
            exit(EXIT_SUCCESS);
        }
        if (strcmp(read_buf, "new_round") == 0)
            printf("[%d] Biore udzial w rundzie %d\n", index, round_counter);
        int selected_card = 1 + rand() % M;
        while (used_cards[selected_card])
            selected_card = 1 + rand() % M;
        used_cards[selected_card] = true;

        memset(write_buf, 0, MESSAGE_LEN);
        snprintf(write_buf, sizeof(write_buf), "%d", selected_card);

        if ((nwrite = write(pipes->container[index].pipe1[1], write_buf, sizeof(write_buf))) == -1)
            ERR("write");

        if ((nread = read(pipes->container[index].pipe0[0], &read_buf, sizeof(read_buf))) == -1)
            ERR("read");
        int winning = atoi(read_buf);
        if (winning > 0)
        {
            printf("[%d] wygralem runde %d\n", index, round_counter);
        }
        round_counter++;
    }
    close(pipes->container[index].pipe0[0]);
    close(pipes->container[index].pipe1[1]);
    return;
}

void server_work(int N, int M, Pipes* pipes)
{
    char read_buf[16];
    char write_buf[16];
    ssize_t printed;
    ssize_t nread, nwrite;
    int round_counter = 0;
    int cards_in_round[N];
    bool active_players[N];
    int round_winners[N];
    int result[N];
    memset(result, 0, sizeof(result));
    pid_t pid = getpid();
    srand(pid);
    for (int i = 0; i < N; i++)
    {
        if (close(pipes->container[i].pipe0[0]) == -1)
            ERR("close");
        if (close(pipes->container[i].pipe1[1]) == -1)
            ERR("close");
        active_players[i] = true;
    }
    while (round_counter < M)
    {
        memset(write_buf, 0, MESSAGE_LEN);
        if ((printed = snprintf(write_buf, sizeof(write_buf), "new_round")) == -1)
            ERR("snprintf");
        for (int i = 0; i < N; i++)
        {  // wysylasz kazdemu poczatek rundy
            if ((nwrite = write(pipes->container[i].pipe0[1], write_buf, sizeof(write_buf))) == -1)
            {
                active_players[i] = false;
                close(pipes->container[i].pipe0[1]);
                close(pipes->container[i].pipe1[0]);
            }
        }
        int biggest_card = -1;
        for (int i = 0; i < N; i++)
        {  // czekasz na karty od gracza
            if (!active_players[i])
                continue;
            if ((nread = read(pipes->container[i].pipe1[0], &read_buf, MESSAGE_LEN)) == -1)
            {
                active_players[i] = false;
            }
            int card = atoi(read_buf);
            printf("Gracz %d wyslal karte %d\n", i, card);
            if (card > biggest_card)
            {
                biggest_card = card;
            }
            cards_in_round[i] = card;
        }
        int winners = 0;
        for (int i = 0; i < N; i++)  // decydujesz kto wygral
        {
            if (cards_in_round[i] == biggest_card)
            {
                round_winners[i] = 1;
                winners++;
            }
            else
            {
                round_winners[i] = 0;
            }
        }
        int prize = floor(N / winners);
        for (int i = 0; i < N; i++)  // wysylasz nagrode graczom
        {
            if (round_winners[i])
            {
                memset(write_buf, 0, MESSAGE_LEN);
                snprintf(write_buf, sizeof(write_buf), "%d", prize);
                if ((nwrite = write(pipes->container[i].pipe0[1], write_buf, sizeof(write_buf))) == -1)
                    ERR("write");
                result[i] += prize;
            }
            else
            {
                memset(write_buf, 0, MESSAGE_LEN);
                snprintf(write_buf, sizeof(write_buf), "%d", 0);
                if ((nwrite = write(pipes->container[i].pipe0[1], write_buf, sizeof(write_buf))) == -1)
                    ERR("write");
            }
        }
        round_counter++;
    }
    printf("Wyniki:\n");
    for (int i = 0; i < N; i++)
    {
        printf("Gracz %d: %d\n", i, result[i]);
    }
    return;
}
void create_players(int N, int M, Pipes* pipes)
{
    pid_t pid;
    for (int i = 0; i < N; i++)
    {
        if ((pid = fork()) == -1)
            ERR("fork");
        if (pid == 0)
        {
            player_work(N, M, pipes, i);
            exit(EXIT_SUCCESS);
        }
    }
    server_work(N, M, pipes);
    while (wait(NULL) > 0)
        ;
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 3)
    {
        usage();
        exit(EXIT_FAILURE);
    }
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    if (N > 5 || N < 2 || M > 10 || M < 5)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    PipeContainer* container = (PipeContainer*)malloc(N * sizeof(PipeContainer));
    for (int i = 0; i < N; i++)
    {
        if (pipe(container[i].pipe0) == -1)
            ERR("pipe");
        if (pipe(container[i].pipe1) == -1)
            ERR("pipe");
    }
    Pipes pipes = {container};
    create_players(N, M, &pipes);
    free(container);
    return EXIT_SUCCESS;
}
