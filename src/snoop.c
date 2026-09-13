#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/user.h>

#include "snoop.h"
#include "lexer.h"

#define MAX_SYSCALLS 450
#define MAX_SNOOP_ARGS 128


static const char *syscall_name(long num)
{
    static const char *table[MAX_SYSCALLS];
    static int initialized = 0;

    if (!initialized)
    {
        memset(table, 0, sizeof(table));

        table[0]   = "read";
        table[1]   = "write";
        table[2]   = "open";
        table[3]   = "close";
        table[4]   = "stat";
        table[5]   = "fstat";
        table[6]   = "lstat";
        table[8]   = "lseek";
        table[17]  = "pread64";
        table[18]  = "pwrite64";
        table[21]  = "access";
        table[32]  = "dup";
        table[33]  = "dup2";
        table[72]  = "fcntl";
        table[78]  = "getdents";
        table[82]  = "rename";
        table[83]  = "mkdir";
        table[84]  = "rmdir";
        table[86]  = "link";
        table[87]  = "unlink";
        table[89]  = "readlink";
        table[90]  = "chmod";
        table[92]  = "chown";
        table[217] = "getdents64";
        table[257] = "openat";
        table[258] = "mkdirat";
        table[262] = "newfstatat";
        table[263] = "unlinkat";
        table[269] = "faccessat";

        table[9]   = "mmap";
        table[10]  = "mprotect";
        table[11]  = "munmap";
        table[12]  = "brk";
        table[25]  = "mremap";

        table[16]  = "ioctl";
        table[13]  = "rt_sigaction";
        table[14]  = "rt_sigprocmask";
        table[15]  = "rt_sigreturn";
        table[22]  = "pipe";
        table[34]  = "pause";
        table[35]  = "nanosleep";
        table[37]  = "alarm";
        table[39]  = "getpid";
        table[56]  = "clone";
        table[57]  = "fork";
        table[58]  = "vfork";
        table[59]  = "execve";
        table[60]  = "exit";
        table[61]  = "wait4";
        table[62]  = "kill";
        table[63]  = "uname";
        table[110] = "getppid";
        table[186] = "gettid";
        table[231] = "exit_group";
        table[234] = "tgkill";

        table[79]  = "getcwd";
        table[80]  = "chdir";

        table[102] = "getuid";
        table[104] = "getgid";
        table[107] = "geteuid";
        table[108] = "getegid";

        table[41]  = "socket";
        table[42]  = "connect";
        table[43]  = "accept";
        table[44]  = "sendto";
        table[45]  = "recvfrom";
        table[49]  = "bind";
        table[50]  = "listen";

        table[96]  = "gettimeofday";
        table[201] = "time";
        table[228] = "clock_gettime";
        table[230] = "clock_nanosleep";

        table[97]  = "getrlimit";
        table[99]  = "sysinfo";
        table[137] = "statfs";
        table[157] = "prctl";
        table[158] = "arch_prctl";
        table[202] = "futex";
        table[218] = "set_tid_address";
        table[273] = "set_robust_list";
        table[302] = "prlimit64";
        table[318] = "getrandom";

        initialized = 1;
    }

    if (num >= 0 && num < MAX_SYSCALLS && table[num] != NULL)
        return table[num];

    static char buf[32];
    snprintf(buf, sizeof(buf), "syscall_%ld", num);
    return buf;
}

typedef struct {
    long number;
    long count;
    double total_time;
    int order; /* order of first occurrence, -1 = unused */
} syscall_stat_t;

static syscall_stat_t stats[MAX_SYSCALLS];
static int order_counter;

static void reset_stats(void)
{
    for (int i = 0; i < MAX_SYSCALLS; i++)
    {
        stats[i].number = i;
        stats[i].count = 0;
        stats[i].total_time = 0.0;
        stats[i].order = -1;
    }
    order_counter = 0;
}

static void record_syscall(long num, double elapsed)
{
    if (num < 0 || num >= MAX_SYSCALLS)
        return;

    if (stats[num].order == -1)
        stats[num].order = order_counter++;

    stats[num].count++;
    stats[num].total_time += elapsed;
}

static int compare_stats(const void *a, const void *b)
{
    const syscall_stat_t *sa = *(const syscall_stat_t **)a;
    const syscall_stat_t *sb = *(const syscall_stat_t **)b;

    if (sa->count != sb->count)
        return (int)(sb->count - sa->count); /* descending count */

    return sa->order - sb->order; /* ascending first-occurrence order */
}

static void print_summary(void)
{
    syscall_stat_t *used[MAX_SYSCALLS];
    int used_count = 0;

    for (int i = 0; i < MAX_SYSCALLS; i++)
    {
        if (stats[i].order != -1)
            used[used_count++] = &stats[i];
    }

    qsort(used, used_count, sizeof(syscall_stat_t *), compare_stats);

    printf("%-13s %-7s %s\n", "syscall", "calls", "time");
    for (int i = 0; i < used_count; i++)
    {
        printf("%-13s %-7ld %.3fs\n",
               syscall_name(used[i]->number),
               used[i]->count,
               used[i]->total_time);
    }
}

static double timespec_diff(struct timespec *start, struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1e9;
}

static void run_syscall_trace(pid_t pid)
{
    reset_stats();

    int expecting_entry = 1;
    long current_syscall_num = -1;
    struct timespec entry_time = {0, 0};
    int resume_sig = 0;

    while (1)
    {
        if (ptrace(PTRACE_SYSCALL, pid, NULL, (void *)(long)resume_sig) == -1)
            break;
        resume_sig = 0;

        int status;
        if (waitpid(pid, &status, 0) == -1)
            break;

        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            if (!expecting_entry)
            {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                record_syscall(current_syscall_num, timespec_diff(&entry_time, &now));
            }
            break;
        }

        if (!WIFSTOPPED(status))
            continue;

        int sig = WSTOPSIG(status);

        if (sig != SIGTRAP)
        {
            resume_sig = sig;
            continue;
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (expecting_entry)
        {
            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS, pid, NULL, &regs) == 0)
            {
                current_syscall_num = (long)regs.orig_rax;
                entry_time = now;
            }
            expecting_entry = 0;
        }
        else
        {
            record_syscall(current_syscall_num, timespec_diff(&entry_time, &now));
            expecting_entry = 1;
        }
    }
}

static int is_all_digits(const char *s)
{
    if (s == NULL || *s == '\0')
        return 0;

    for (int i = 0; s[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }
    return 1;
}

static void snoop_attach(pid_t pid)
{
    if (kill(pid, 0) == -1)
    {
        printf("snoop: no such process\n");
        return;
    }

    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) == -1)
    {
        if (errno == ESRCH)
            printf("snoop: no such process\n");
        else
            printf("snoop: could not attach to process\n");
        return;
    }

    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        printf("snoop: could not attach to process\n");
        return;
    }

    run_syscall_trace(pid);
    print_summary();
}

static void snoop_command(char **argv)
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        printf("snoop: internal error\n");
        return;
    }
    fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid == -1)
    {
        printf("snoop: internal error\n");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0)
    {
        close(pipefd[0]);

        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execvp(argv[0], argv);
        int err = errno;
        ssize_t written = write(pipefd[1], &err, sizeof(err));
        (void)written;
        close(pipefd[1]);
        _exit(127);
    }
    close(pipefd[1]);

    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
        printf("snoop: internal error\n");
        close(pipefd[0]);
        return;
    }

    int child_err = 0;
    ssize_t n = read(pipefd[0], &child_err, sizeof(child_err));
    close(pipefd[0]);

    if (n == sizeof(child_err))
    {
        printf("snoop: command not found\n");
        return;
    }

    if (!WIFSTOPPED(status))
    {
        printf("snoop: command not found\n");
        return;
    }

    run_syscall_trace(pid);
    print_summary();
}

void exec_snoop(struct token *current)
{
    current = current->next;

    if (current == NULL || current->value == NULL)
    {
        printf("snoop: invalid syntax\n");
        return;
    }

    if (strcmp(current->value, "-p") == 0)
    {
        struct token *tok = current->next;

        if (tok == NULL || tok->value == NULL || !is_all_digits(tok->value))
        {
            printf("snoop: invalid syntax\n");
            return;
        }

        if (tok->next != NULL)
        {
            printf("snoop: invalid syntax\n");
            return;
        }

        pid_t pid = (pid_t)strtol(tok->value, NULL, 10);
        snoop_attach(pid);
        return;
    }

    char *argv[MAX_SNOOP_ARGS];
    int argc = 0;

    struct token *tok = current;
    while (tok != NULL && argc < MAX_SNOOP_ARGS - 1)
    {
        if (tok->value == NULL)
            break;
        argv[argc++] = tok->value;
        tok = tok->next;
    }
    argv[argc] = NULL;

    if (argc == 0)
    {
        printf("snoop: invalid syntax\n");
        return;
    }
    snoop_command(argv);
}