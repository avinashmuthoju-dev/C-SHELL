#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "background.h"
#include "command.h"

#define MAX_JOBS 100
#define MAX_PROCS 100

#define PROC_RUNNING 0
#define PROC_STOPPED 1

struct proc_info {
    pid_t pid;
    char name[64];
    int state;    /* PROC_RUNNING or PROC_STOPPED */
    int exited;   /* 1 once this specific process has exited */
};

struct background_job {
    int job_number;
    pid_t pgid;
    struct proc_info procs[MAX_PROCS];
    int proc_count;
    int remaining;  /* how many procs in this job have NOT exited yet */
    int active;     /* 0 once the job has been fully reaped/removed */
    char cmdline[256];
};

static struct background_job jobs[MAX_JOBS];
static int job_count = 0;
static int next_job_number = 1;

/* Self-pipe for signal-safe notification */
static int notification_pipe[2] = {-1, -1};

struct notification {
    pid_t pid;              /* leader pid of the job that just finished */
    int exited_normally;
};

static void sigchld_handler(int sig)
{
    (void)sig;
    int saved_errno = errno;
    int status;
    pid_t pid;

    /* WUNTRACED lets us see children that got stopped (e.g. SIGTTIN),
     * not just ones that exited. */
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED |  WCONTINUED)) > 0)
    {
        for (int i = 0; i < job_count; i++)
        {
            if (!jobs[i].active)
                continue;

            int matched = 0;
            for (int j = 0; j < jobs[i].proc_count; j++)
            {
                if (jobs[i].procs[j].exited || jobs[i].procs[j].pid != pid)
                    continue;

                matched = 1;

                if (WIFSTOPPED(status))
                {
                    jobs[i].procs[j].state = PROC_STOPPED;
                }
                else if (WIFCONTINUED(status))
                {
                    jobs[i].procs[j].state = PROC_RUNNING;
                }
                else if (WIFEXITED(status) || WIFSIGNALED(status))
                {
                    jobs[i].procs[j].exited = 1;
                    jobs[i].remaining--;

                    /* Only notify/remove once every process in the
                     * group has exited. */
                    if (jobs[i].remaining == 0)
                    {
                        struct notification notif;
                        notif.pid = jobs[i].procs[0].pid;
                        notif.exited_normally = WIFEXITED(status) ? 1 : 0;

                        ssize_t written = 0;
                        while (written < (ssize_t)sizeof(notif))
                        {
                            ssize_t n = write(notification_pipe[1],
                                             ((char*)&notif) + written,
                                             sizeof(notif) - (size_t)written);
                            if (n <= 0) break;
                            written += n;
                        }
                    }
                }
                break;
            }

            if (matched)
                break;
        }
        /* If not found in any job, it was a helper process (e.g. an
         * input/output copier) or a foreground child - nothing to do,
         * waitpid() above already reaped it so it won't zombie. */
    }

    errno = saved_errno;
}

void init_background(void)
{
    /* Create self-pipe for notifications */
    if (pipe(notification_pipe) == -1)
    {
        perror("pipe");
        exit(1);
    }

    /* Make read end non-blocking */
    int flags = fcntl(notification_pipe[0], F_GETFL);
    if (flags != -1)
    {
        fcntl(notification_pipe[0], F_SETFL, flags | O_NONBLOCK);
    }

    /* Install signal handler */
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }
}

sigset_t block_sigchld(void)
{
    sigset_t newmask, oldmask;
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &newmask, &oldmask);
    return oldmask;
}

void restore_sigchld(sigset_t oldmask)
{
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

void register_background_job(pid_t pgid, pid_t *pids, char **names, int proc_count,const char *cmdline)
{
    if (job_count >= MAX_JOBS)
    {
        fprintf(stderr, "cshell: job table full\n");
        return;
    }
    if (proc_count > MAX_PROCS)
    {
        proc_count = MAX_PROCS;
    }

    struct background_job *job = &jobs[job_count];

    job->job_number = next_job_number++;
    job->pgid = pgid;
    job->proc_count = proc_count;
    job->remaining = proc_count;
    job->active = 1;
    snprintf(job->cmdline, sizeof(job->cmdline), "%s", cmdline ? cmdline : "");

    for (int i = 0; i < proc_count; i++)
    {
        job->procs[i].pid = pids[i];
        job->procs[i].state = PROC_RUNNING;
        job->procs[i].exited = 0;
        snprintf(job->procs[i].name, sizeof(job->procs[i].name), "%s", names[i]);
    }

    fprintf(stderr, "[%d] %d\n", job->job_number, pgid);
    fflush(stderr);

    job_count++;
}

void register_stopped_job(pid_t pgid,
                          pid_t *pids,
                          char **names,
                          int proc_count,const char *cmdline)
{
    if (job_count >= MAX_JOBS)
    {
        fprintf(stderr, "cshell: job table full\n");
        return;
    }

    if (proc_count > MAX_PROCS)
    {
        proc_count = MAX_PROCS;
    }

    struct background_job *job = &jobs[job_count];

    job->job_number = next_job_number++;
    job->pgid = pgid;
    job->proc_count = proc_count;
    job->remaining = proc_count;
    job->active = 1;
    snprintf(job->cmdline, sizeof(job->cmdline), "%s", cmdline ? cmdline : "");

    for (int i = 0; i < proc_count; i++)
    {
        job->procs[i].pid = pids[i];

        job->procs[i].state = PROC_STOPPED;

        job->procs[i].exited = 0;

        snprintf(job->procs[i].name,
                 sizeof(job->procs[i].name),
                 "%s",
                 names[i]);
    }

    fprintf(stderr,
            "\n[%d] + Stopped %s\n",
            job->job_number,
             job->cmdline);

    fflush(stderr);

    job_count++;
}

void drain_background_notifications(void)
{
    struct notification notif;

    while (1)
    {
        ssize_t n = read(notification_pipe[0], &notif, sizeof(notif));
        if (n != sizeof(notif))
        {
            /* No more notifications or incomplete read */
            break;
        }

        /* Find the job by its leader pid and print completion */
        for (int i = 0; i < job_count; i++)
        {
            if (jobs[i].active && jobs[i].procs[0].pid == notif.pid)
            {
                fprintf(stderr, "%s with pid %d %s\n",
                        jobs[i].procs[0].name,
                        notif.pid,
                        notif.exited_normally ? "exited normally" : "exited abnormally");
                fflush(stderr);

                /* Remove job by moving last job to this slot */
                jobs[i] = jobs[job_count - 1];
                job_count--;
                i--; /* Recheck this slot if we moved a job here */
                break;
            }
        }
    }
}

int get_activities(struct activity_job *out, int max_jobs)
{
    /* Block SIGCHLD so a process state change can't land mid-copy. */
    sigset_t oldmask = block_sigchld();

    int count = 0;
    for (int i = 0; i < job_count && count < max_jobs; i++)
    {
        if (!jobs[i].active)
            continue;

        struct activity_job *dst = &out[count];
        dst->job_number = jobs[i].job_number;
        dst->pgid = jobs[i].pgid;
        snprintf(dst->cmdline, sizeof(dst->cmdline), "%s", jobs[i].cmdline);
        dst->proc_count = 0;

        for (int j = 0; j < jobs[i].proc_count && dst->proc_count < MAX_ACTIVITY_PROCS; j++)
        {
            if (jobs[i].procs[j].exited)
                continue; /* already gone, don't hand it out */

            struct activity_proc *p = &dst->procs[dst->proc_count];
            p->pid = jobs[i].procs[j].pid;
            snprintf(p->name, sizeof(p->name), "%s", jobs[i].procs[j].name);
            p->stopped = (jobs[i].procs[j].state == PROC_STOPPED);
            dst->proc_count++;
        }

        count++;
    }

    restore_sigchld(oldmask);
    return count;
}

int run_background(char *path,
                   char **argv,
                   char **input_files,
                   int input_file_count,
                   struct output_file *files,
                   int file_count)
{
    int fds[100];
    int input_fd = -1;
    pid_t writer_pid = -1;

    /* Setup input redirection */
    if (input_file_count > 0)
    {
        input_fd = setup_input(input_files, input_file_count, &writer_pid);
        if (input_fd == -1)
        {
            fprintf(stderr, "cshell: no such file or directory\n");
            return 1;
        }
    }

    /* Open output files */
    if (file_count > 0)
    {
        if (!open_output_files(files, file_count, fds))
        {
            if (input_fd != -1)
                close(input_fd);
            return 1;
        }
    }

    /* Setup output pipe for multiple output files */
    int output_pipe[2] = {-1, -1};
    pid_t output_writer = -1;

    if (file_count > 1)
    {
        if (pipe(output_pipe) == -1)
        {
            if (input_fd != -1)
                close(input_fd);
            for (int i = 0; i < file_count; i++)
                close(fds[i]);
            return 1;
        }

        output_writer = fork();
        if (output_writer == -1)
        {
            close(output_pipe[0]);
            close(output_pipe[1]);
            if (input_fd != -1)
                close(input_fd);
            for (int i = 0; i < file_count; i++)
                close(fds[i]);
            return 1;
        }

        if (output_writer == 0)
        {
            /* Output copier child */
            close(output_pipe[1]);

            char buffer[4096];
            ssize_t bytes;

            while ((bytes = read(output_pipe[0], buffer, sizeof(buffer))) > 0)
            {
                for (int i = 0; i < file_count; i++)
                {
                    ssize_t written = 0;
                    while (written < bytes)
                    {
                        ssize_t result = write(fds[i],
                                              buffer + written,
                                              (size_t)(bytes - written));
                        if (result <= 0)
                            _exit(1);
                        written += result;
                    }
                }
            }

            close(output_pipe[0]);
            for (int i = 0; i < file_count; i++)
                close(fds[i]);
            _exit(0);
        }
    }

    /* Block SIGCHLD during fork/register to prevent race */
    sigset_t oldmask = block_sigchld();

    /* Create background command process */
    pid_t pid = fork();

    if (pid == -1)
    {
        restore_sigchld(oldmask);
        if (input_fd != -1)
            close(input_fd);
        if (file_count > 1)
        {
            close(output_pipe[0]);
            close(output_pipe[1]);
        }
        for (int i = 0; i < file_count; i++)
            close(fds[i]);
        return 1;
    }

    if (pid == 0)
    {
        /* Background command child */
        restore_sigchld(oldmask);
        setpgid(0, 0);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        if (input_fd != -1)
        {
            if (dup2(input_fd, STDIN_FILENO) == -1)
                _exit(1);
            close(input_fd);
        }

        /* Output redirection */
        if (file_count == 1)
        {
            if (dup2(fds[0], STDOUT_FILENO) == -1)
                _exit(1);
            close(fds[0]);
        }
        else if (file_count > 1)
        {
            close(output_pipe[0]);
            if (dup2(output_pipe[1], STDOUT_FILENO) == -1)
                _exit(1);
            close(output_pipe[1]);
            for (int i = 0; i < file_count; i++)
                close(fds[i]);
        }

        execv(path, argv);
        _exit(1);
    }

    /* Parent: put the child in its own group, register the job, then
     * restore signals. */
    char cmdline[256] = "";
    for (int i = 0; argv[i] != NULL; i++) {
     strncat(cmdline, argv[i], sizeof(cmdline) - strlen(cmdline) - 1);
     if (argv[i+1] != NULL) strncat(cmdline, " ", sizeof(cmdline) - strlen(cmdline) - 1);
   }
    setpgid(pid, pid);
    register_background_job(pid, &pid, &argv[0], 1,cmdline);
    restore_sigchld(oldmask);

    /* Close parent's file descriptors */
    if (input_fd != -1)
        close(input_fd);

    if (file_count == 1)
    {
        close(fds[0]);
    }
    else if (file_count > 1)
    {
        close(output_pipe[0]);
        close(output_pipe[1]);
        for (int i = 0; i < file_count; i++)
            close(fds[i]);
    }

    return 0;
}
int mark_job_running(int job_number)
{
    sigset_t oldmask = block_sigchld();
    int found = 0;

    for (int i = 0; i < job_count; i++)
    {
        if (jobs[i].active && jobs[i].job_number == job_number)
        {
            for (int j = 0; j < jobs[i].proc_count; j++)
                if (!jobs[i].procs[j].exited)
                    jobs[i].procs[j].state = PROC_RUNNING;
            found = 1;
            break;
        }
    }

    restore_sigchld(oldmask);
    return found;
}

int mark_job_stopped(int job_number)
{
    sigset_t oldmask = block_sigchld();
    int found = 0;

    for (int i = 0; i < job_count; i++)
    {
        if (jobs[i].active && jobs[i].job_number == job_number)
        {
            for (int j = 0; j < jobs[i].proc_count; j++)
                if (!jobs[i].procs[j].exited)
                    jobs[i].procs[j].state = PROC_STOPPED;
            found = 1;
            break;
        }
    }

    restore_sigchld(oldmask);
    return found;
}

int remove_job_by_number(int job_number)
{
    sigset_t oldmask = block_sigchld();
    int found = 0;

    for (int i = 0; i < job_count; i++)
    {
        if (jobs[i].active && jobs[i].job_number == job_number)
        {
            jobs[i] = jobs[job_count - 1];
            job_count--;
            found = 1;
            break;
        }
    }

    restore_sigchld(oldmask);
    return found;
}

void set_job_cmdline(pid_t pgid, const char *cmdline)
{
    sigset_t oldmask = block_sigchld();

    for (int i = 0; i < job_count; i++)
    {
        if (jobs[i].active && jobs[i].pgid == pgid)
        {
            snprintf(jobs[i].cmdline, sizeof(jobs[i].cmdline), "%s", cmdline);
            break;
        }
    }

    restore_sigchld(oldmask);
}