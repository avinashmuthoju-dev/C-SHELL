#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "terminal.h"
#include "background.h"


/*
 * Process group ID of the shell.
 */
static pid_t shell_pgid;


/*
 * 0 -> previous input was not Ctrl-D
 * 1 -> previous input was Ctrl-D
 */
static int previous_eof = 0;
static volatile sig_atomic_t signal_newline_needed = 0;

static void shell_signal_handler(int sig)
{
    (void)sig;
    signal_newline_needed = 1;
}

int consume_signal_newline(void)
{
    if (signal_newline_needed)
    {
        signal_newline_needed = 0;
        printf("\n");
        return 1;
    }

    return 0;
}

void init_job_control(void)
{
    shell_pgid = getpid();

    /*
     * Shell must never be stopped by terminal job-control signals.
     */
    signal(SIGINT, shell_signal_handler);
    signal(SIGTSTP, shell_signal_handler);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    /*
     * Put shell into its own process group.
     */
    if (setpgid(shell_pgid, shell_pgid) == -1)
    {
        /* It may already be in its own process group. */
    }

    /*
     * Shell must own the terminal before accepting commands.
     */
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1)
    {
        perror("tcsetpgrp");
    }
}

void give_terminal_to(pid_t pgid)
{
    if (tcsetpgrp(STDIN_FILENO, pgid) == -1)
    {
        perror("tcsetpgrp");
    }
}



void reclaim_terminal(void)
{
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1)
    {
        perror("tcsetpgrp");
    }
}



pid_t get_shell_pgid(void)
{
    return shell_pgid;
}



int wait_foreground_group(pid_t pgid,
                          pid_t *pids,
                          int process_count)
{
    int finished[100] = {0};
    int finished_count = 0;
    int was_signaled = 0;

    while (finished_count < process_count)
    {
        int status;

        pid_t pid = waitpid(-pgid, &status, WUNTRACED);

        if (pid == -1)
        {
            if (errno == EINTR)
                continue;

            break;
        }

        if (WIFSTOPPED(status))
        {
            return 1;
        }

        if (WIFSIGNALED(status))
        {
            was_signaled = 1;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            for (int i = 0; i < process_count; i++)
            {
                if (pids[i] == pid && !finished[i])
                {
                    finished[i] = 1;
                    finished_count++;
                    break;
                }
            }
        }
    }

    if (was_signaled)
        return 2;

    return 0;
}


int handle_eof(void)
{
    struct activity_job jobs[100];

    int count = get_activities(jobs, 100);

    int stopped = 0;

    for (int i = 0; i < count; i++)
    {
        for (int j = 0; j < jobs[i].proc_count; j++)
        {
            if (jobs[i].procs[j].stopped)
            {
                stopped = 1;
                break;
            }
        }

        if (stopped)
        {
            break;
        }
    }



    if (!stopped)
    {
        send_sighup_to_all_jobs();

        return 1;
    }


    if (!previous_eof)
    {
        fprintf(stderr,
                "cshell: there are stopped jobs\n");

        previous_eof = 1;

        return 0;
    }


    /*
     * Second consecutive Ctrl-D.
     */
    send_sighup_to_all_jobs();

    return 1;
}



void reset_eof_state(void)
{
    previous_eof = 0;
}

void send_sighup_to_all_jobs(void)
{
    struct activity_job jobs[100];

    int count = get_activities(jobs, 100);

    for (int i = 0; i < count; i++)
    {
        if (jobs[i].pgid > 0)
        {
            kill(-jobs[i].pgid, SIGHUP);
        }
    }
}
static volatile sig_atomic_t resume_alarm_fired = 0;

static void resume_alarm_handler(int sig)
{
    (void)sig;
    resume_alarm_fired = 1;
}

/* Same as wait_foreground_group, but with an optional timeout.
 * Returns: 0 finished, 1 stopped, 2 finished via signal, 3 timed out. */
int wait_foreground_group_timeout(pid_t pgid, pid_t *pids, int process_count, int timeout_seconds)
{
    int finished[100] = {0};
    int finished_count = 0;
    int was_signaled = 0;
    struct sigaction sa, old_sa;

    resume_alarm_fired = 0;

    if (timeout_seconds > 0)
    {
        sa.sa_handler = resume_alarm_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;   /* no SA_RESTART: we need waitpid to return EINTR */
        sigaction(SIGALRM, &sa, &old_sa);
        alarm(timeout_seconds);
    }

    while (finished_count < process_count)
    {
        int status;
        pid_t pid = waitpid(-pgid, &status, WUNTRACED);

        if (pid == -1)
        {
            if (errno == EINTR)
            {
                if (resume_alarm_fired)
                {
                    if (timeout_seconds > 0)
                        sigaction(SIGALRM, &old_sa, NULL);
                    return 3;
                }
                continue;
            }
            break;
        }

        if (WIFSTOPPED(status))
        {
            if (timeout_seconds > 0)
            {
                alarm(0);
                sigaction(SIGALRM, &old_sa, NULL);
            }
            return 1;
        }

        if (WIFSIGNALED(status))
            was_signaled = 1;

        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            for (int i = 0; i < process_count; i++)
            {
                if (pids[i] == pid && !finished[i])
                {
                    finished[i] = 1;
                    finished_count++;
                    break;
                }
            }
        }
    }

    if (timeout_seconds > 0)
    {
        alarm(0);
        sigaction(SIGALRM, &old_sa, NULL);
    }

    return was_signaled ? 2 : 0;
}