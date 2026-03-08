#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define FRANCS 0
#define SARACENS 1
typedef struct
{
    int count;
    int* write_fds;
    int* read_fds;
} Side;

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}
void soldier_work(int index, int side, char* name, int hp, int strength, Side* ally, Side* enemy)
{
    if (side == FRANCS)
    {
        printf("I am French knight %s. I have come to support my king with %d hp and %d strength\n", name, hp,
               strength);
    }
    else
    {
        printf("I am Spanish knight %s. I have come to support my king with %d hp and %d strength\n", name, hp,
               strength);
    }
    srand(getpid());

    fcntl(ally->read_fds[index], F_SETFL, O_NONBLOCK);
    int alive[enemy->count];
    int alive_count = enemy->count;
    for (int i = 0; i < enemy->count; i++)
        alive[i] = 1;
    while (hp > 0 && alive_count > 0)
    {
        int attacked;
        ssize_t r = read(ally->read_fds[index], &attacked, sizeof(int));
        if (r > 0)
        {
            hp -= attacked;
            if (hp <= 0)
                break;
        }
        else if (r == 0)
            break;
        int S = rand() % strength;
        int enemy_index = rand() % enemy->count;
        if (!alive[enemy_index])
            continue;
        if (S == 0)
        {
            printf("%s attacked enemy, however he deflected\n", name);
            continue;
        }
        else if (S > 0 && S <= 5)
        {
            printf("%s goes to strike, he hit right and well\n", name);
        }
        else
        {
            printf("%s strikes a powerful blow, he broke the enemy's shield and inflicted a big wound\n", name);
            // po angielsku zle napisali w warsztacie zmienilem na dobrze
        }
        int w = write(enemy->write_fds[enemy_index], &S, sizeof(int));
        if (w == -1 && errno == EPIPE)
        {
            alive[enemy_index] = 0;
            alive_count--;
        }
        int random_sleep = rand() % 10 + 1;
        msleep(random_sleep);
    }
    if (hp <= 0)
    {
        printf("%s dies.\n", name);
    }
    else
    {
        printf("%s survives with %d hp\n", name, hp);
    }
    close(ally->read_fds[index]);
    for (int i = 0; i < enemy->count; i++)
    {
        close(enemy->write_fds[i]);
    }
}

void create_soldier(int index, int side, char* name, int hp, int strength, Side* ally, Side* enemy)
{
    pid_t pid;
    if ((pid = fork()) == -1)
    {
        ERR("fork");
    }
    else if (pid == 0)
    {
        for (int i = 0; i < ally->count; i++)
        {
            close(ally->write_fds[i]);
            if (i != index)
                close(ally->read_fds[i]);
        }
        for (int i = 0; i < enemy->count; i++)
        {
            close(enemy->read_fds[i]);
        }
        soldier_work(index, side, name, hp, strength, ally, enemy);

        exit(EXIT_SUCCESS);
    }
}

void read_files()
{
    set_handler(SIG_IGN, SIGPIPE);
    FILE* francs;
    FILE* saracens;
    if ((francs = fopen("franci.txt", "r")) == NULL)
    {
        ERR("Franks didn't come to the battle");
    }
    if ((saracens = fopen("saraceni.txt", "r")) == NULL)
    {
        ERR("Saracens didn't come to the battle");
    }
    int francs_count, saracens_count;
    fscanf(francs, "%d", &francs_count);
    fscanf(saracens, "%d", &saracens_count);

    int francs_fds[francs_count][2];
    int saracens_fds[saracens_count][2];
    int francs_write[francs_count];
    int saracens_write[saracens_count];
    int saracens_read[saracens_count];
    int francs_read[francs_count];
    int hp, strength;
    char buf[256];
    for (int i = 0; i < francs_count; i++)
    {
        if (pipe(francs_fds[i]) == -1)
        {
            ERR("pipe");
        }
        francs_write[i] = francs_fds[i][1];
        francs_read[i] = francs_fds[i][0];
    }
    for (int i = 0; i < saracens_count; i++)
    {
        if (pipe(saracens_fds[i]) == -1)
        {
            ERR("pipe");
        }
        saracens_write[i] = saracens_fds[i][1];
        saracens_read[i] = saracens_fds[i][0];
    }
    Side french = {francs_count, francs_write, francs_read};
    Side spanish = {saracens_count, saracens_write, saracens_read};

    for (int i = 0; i < francs_count; i++)
    {
        fscanf(francs, "%s %d %d", buf, &hp, &strength);
        create_soldier(i, FRANCS, buf, hp, strength, &french, &spanish);
    }

    for (int i = 0; i < saracens_count; i++)
    {
        fscanf(saracens, "%s %d %d", buf, &hp, &strength);
        create_soldier(i, SARACENS, buf, hp, strength, &spanish, &french);
    }

    for (int i = 0; i < francs_count; i++)
    {
        close(francs_fds[i][0]);
        close(francs_fds[i][1]);
    }
    for (int i = 0; i < saracens_count; i++)
    {
        close(saracens_fds[i][0]);
        close(saracens_fds[i][1]);
    }
    fclose(francs);
    fclose(saracens);
}

int main(int argc, char* argv[])
{
    read_files();
    srand(time(NULL));
    while (wait(NULL) > 0)
        ;
    printf("open file descriptors: %d\n", count_descriptors());
}
