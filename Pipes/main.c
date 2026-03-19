#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

typedef struct teacher
{
    int *pids;
    int n;
    int *pipes;
    int mypipe;
} targ_t;
typedef struct student
{
    int myreadpipe;
    int teacherwritepipe;

} sarg_t;
sig_atomic_t last_signal = 0;
void msleep(unsigned int milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    struct timespec req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}
void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;

    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}
void sig_handler(int sig)
{
    printf("[%d] received signal %d\n", getpid(), sig);
    last_signal = sig;
}
void AttendanceCheck(int mypipe, int teacherpipe)
{
    char buff[PIPE_BUF];
    read(mypipe, buff, sizeof(buff) - 1);

    int n = sprintf(buff, "%s%d%s", "Student ", getpid(), ": HERE");
    int written = 0;
    while (written < n)
    {
        written += write(teacherpipe, buff + written, n - written);
    }
    puts(buff);
    return;
}
void TeacherAttendanceCheck(int *studentPipes, int mypipe, int n, int *pids)
{
    char buff[PIPE_BUF];
    for (int i = 1; i < 2 * n; i += 2)
    {
        int n = sprintf(buff, "%s%d%s", "Teacher: Is ", pids[i / 2], " here?");
        puts(buff);
        int written = 0;
        while (written < n)
        {
            written += write(studentPipes[i], buff + written, n - written);
        }
    }
    int attendies = 0;
    while (attendies < n)
    {
        int r = read(mypipe, buff, sizeof(buff) - 1);
        if (r > 0)
        {
            buff[r] = '\0';
        }

        for (int i = 0; i < r; i++)
        {
            if (buff[i] == ':')
            {
                attendies++;
            }
        }
        // char str[10];
        // intToStr(attendies, str);
        // puts(str);
    }
}

void TeacherWork(void *arg)
{
    sethandler(sig_handler, SIGALRM);
    srand(time(NULL) ^ getpid());
    int *pipes = ((targ_t *)arg)->pipes;
    int n = ((targ_t *)arg)->n;

    int *pids = ((targ_t *)arg)->pids;
    int mypipe = ((targ_t *)arg)->mypipe;
    TeacherAttendanceCheck(pipes, mypipe, n, pids);
    alarm(2);
    int student_stages[n];
    memset(&student_stages, 0, sizeof(int) * n);

    int stg_dif[4];
    stg_dif[0] = 3;
    stg_dif[1] = 6;
    stg_dif[2] = 7;
    stg_dif[3] = 5;
    for (int i = 0; i < 4; i++)
    {
        stg_dif[i] = stg_dif[0] + rand() % 21;
        if (stg_dif[i] > 20)
        {
            stg_dif[i] = 20;
        }
    }

    int amnt_fin = 0;
    int points[n];
    memset(&points, 0, sizeof(int) * n);
    while (amnt_fin < n && last_signal == 0)
    {
        int msg[2];
        if (read(mypipe, &msg, 2 * sizeof(int)) < 0)
        {
            break;
        }
        int pid = 0;

        for (int i = 0; i < n; i++)
        {
            if (pids[i] == msg[0])
            {
                pid = i;
                break;
            }
        }
        int msgb;
        if (msg[1] < stg_dif[student_stages[pid]])
        {
            printf("Teacher: Student %d, needs to fix stage %d\n", pids[pid], student_stages[pid] + 1);
            msgb = 0;
        }
        else
        {
            msgb = 1;
            printf("Teacher: Student %d, finished stage %d\n", pids[pid], student_stages[pid] + 1);
            student_stages[pid]++;
            points[pid] += msg[1];
            if (student_stages[pid] == 4)
            {

                amnt_fin++;
            }
        }
        write(pipes[2 * pid + 1], &msgb, sizeof(int));
    }
    close(mypipe);
    for (int i = 1; i < 2 * n; i += 2)
    {
        close(pipes[i]);
    }
    if (last_signal)
    {
        puts("END OF TIME!");

        for (int i = 0; i < n; i++)
        {
            printf("Teacher: %d - %d\n", pids[i], points[i]);
        }
    }

    puts("Teacher: IT'S FINALLY OVER!");
}

void studentWork(void *arg)
{
    int myreadpipe = ((sarg_t *)arg)->myreadpipe;
    int teacherwritepipe = ((sarg_t *)arg)->teacherwritepipe;
    AttendanceCheck(myreadpipe, teacherwritepipe);
    sethandler(sig_handler, SIGPIPE);
    srand(time(NULL) ^ getpid());
    int k = rand() % 6 + 2;
    int flag = 0;
    for (int i = 0; i < 4;)
    {
        if (last_signal)
        {
            flag = i;
            break;
        }
        unsigned int t = rand() % 400 + 100;
        msleep(t);
        int q = rand() % 21 + 1;
        int tot_score = k + q;
        int msg[2];
        msg[0] = getpid();
        msg[1] = tot_score;
        if (write(teacherwritepipe, &msg, sizeof(int) * 2) == -1 || last_signal)
        {
            flag = i;
            break;
        }
        int msg2;
        if (read(myreadpipe, &msg2, sizeof(int)) == 0 || last_signal)
        {
            flag = i;
            break;
        }
        if (msg2 == 1)
        {
            i++;
        }
    }
    if (flag)
    {
        printf("Student %d: Oh no, I haven't finished stage %d. I need more time\n", getpid(), flag + 1);
        return;
    }
    printf("Student %d: I NAILED IT!\n", getpid());
}

void createChildrenAndPipes(int n, int *pipes)
{
    int *pids = malloc(n * sizeof(int));
    int teacherpipes[2];
    if (pipe(teacherpipes))
    {
        ERR("pipe");
    }
    for (int i = 0; i < n * 2; i += 2)
    {
        if (pipe(&pipes[i]))
        {
            ERR("pipe");
        }
        pids[i / 2] = fork();
        if (pids[i / 2] == -1)
        {
            ERR("fork");
        }
        if (pids[i / 2] == 0)
        {
            int tmpi = i - 1;
            while (tmpi >= 0)
            {
                close(pipes[tmpi]);
                tmpi--;
            }
            close(pipes[i + 1]);
            close(teacherpipes[0]);
            sarg_t studarg;
            studarg.myreadpipe = pipes[i];
            studarg.teacherwritepipe = teacherpipes[1];
            studentWork(&studarg);
            close(pipes[i]);
            close(teacherpipes[1]);
            free(pipes);
            free(pids);
            exit(EXIT_SUCCESS);
        }
    }
    int pid = fork();
    if (pid == 0)
    {
        close(teacherpipes[1]);
        for (int i = 0; i < 2 * n; i += 2)
        {
            close(pipes[i]);
        }
        targ_t teacharg;
        teacharg.mypipe = teacherpipes[0];
        teacharg.n = n;
        teacharg.pids = pids;
        teacharg.pipes = pipes;
        TeacherWork(&teacharg);

        free(pids);
        free(pipes);
        exit(EXIT_SUCCESS);
    }
    for (int i = 0; i < 2 * n; i++)
    {
        close(pipes[i]);
    }
    close(teacherpipes[0]);
    close(teacherpipes[1]);
    free(pipes);
    free(pids);
}
int main(int argc, char **argv)
{

    if (argc < 2)
    {
        ERR("args");
    }
    int n = atoi(argv[1]);
    if (n < 3 || n > 20)
    {
        ERR("n should be between 3 and 20");
    }
    int *pipes = malloc(2 * sizeof(int) * n);
    createChildrenAndPipes(n, pipes);
    while (wait(NULL) > 0)
        ;
}
