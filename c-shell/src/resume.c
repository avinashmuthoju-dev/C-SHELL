#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "resume.h"
#include "lexer.h"
#include "background.h"
#include "terminal.h"

#define MAX_RESUME_JOBS 100

int exec_resume(struct token *current)
{
    struct token *tok = current->next;

    /* %job_number */
    if (tok == NULL || tok->value == NULL || tok->value[0] != '%' || tok->value[1] == '\0')
    {
        printf("resume: invalid syntax\n");
        return 1;
    }

    char *endptr;
    long job_number = strtol(tok->value + 1, &endptr, 10);
    if (*endptr != '\0' || job_number <= 0)
    {
        printf("resume: invalid syntax\n");
        return 1;
    }

    /* fg | bg */
    tok = tok->next;
    if (tok == NULL || tok->value == NULL)
    {
        printf("resume: invalid syntax\n");
        return 1;
    }

    int is_fg;
    if (strcmp(tok->value, "fg") == 0)
        is_fg = 1;
    else if (strcmp(tok->value, "bg") == 0)
        is_fg = 0;
    else
    {
        printf("resume: invalid syntax\n");
        return 1;
    }

    /* optional --timeout <seconds> */
    int timeout_seconds = 0;
    tok = tok->next;

    if (tok != NULL)
    {
        if (!is_fg || tok->value == NULL || strcmp(tok->value, "--timeout") != 0)
        {
            printf("resume: invalid syntax\n");
            return 1;
        }

        tok = tok->next;
        if (tok == NULL || tok->value == NULL)
        {
            printf("resume: invalid syntax\n");
            return 1;
        }

        char *tendptr;
        long t = strtol(tok->value, &tendptr, 10);
        if (*tendptr != '\0' || t <= 0)
        {
            printf("resume: invalid syntax\n");
            return 1;
        }
        timeout_seconds = (int)t;

        if (tok->next != NULL)
        {
            printf("resume: invalid syntax\n");
            return 1;
        }
    }

    /* Look the job up */
    struct activity_job jobs[MAX_RESUME_JOBS];
    int count = get_activities(jobs, MAX_RESUME_JOBS);

    struct activity_job *job = NULL;
    for (int i = 0; i < count; i++)
    {
        if (jobs[i].job_number == (int)job_number)
        {
            job = &jobs[i];
            break;
        }
    }

    if (job == NULL)
    {
        printf("resume: no such job\n");
        return 1;
    }

    pid_t pids[MAX_ACTIVITY_PROCS];
    for (int i = 0; i < job->proc_count; i++)
        pids[i] = job->procs[i].pid;

    char cmdline[256];
    if (job->cmdline[0] != '\0')
        snprintf(cmdline, sizeof(cmdline), "%s", job->cmdline);
    else if (job->proc_count > 0)
        snprintf(cmdline, sizeof(cmdline), "%s", job->procs[0].name);
    else
        cmdline[0] = '\0';

    int job_number_i = job->job_number;
    pid_t pgid = job->pgid;
    int proc_count = job->proc_count;

    /* Block SIGCHLD for the whole sequence, same pattern used by the
     * rest of the shell for foreground/background job transitions. */
    sigset_t oldmask = block_sigchld();
    if (proc_count == 0)
{
    /* Every process in this job already exited before we got here. */
    remove_job_by_number(job_number_i);
    restore_sigchld(oldmask);
    printf("resume: job has already finished\n");
    return 0;
}

    kill(-pgid, SIGCONT);
    mark_job_running(job_number_i);

    if (!is_fg)
    {
        fprintf(stderr, "[%d] + Running %s\n", job_number_i, cmdline);
        fflush(stderr);
        restore_sigchld(oldmask);
        return 0;
    }

    /* fg */
    printf("%s\n", cmdline);
    fflush(stdout);

    give_terminal_to(pgid);

    int result = wait_foreground_group_timeout(pgid, pids, proc_count, timeout_seconds);

    reclaim_terminal();

    if (result == 3)
    {
        kill(-pgid, SIGTERM);
        printf("resume: job timed out\n");
        remove_job_by_number(job_number_i);
    }
    else if (result == 1)
    {
        mark_job_stopped(job_number_i);
        fprintf(stderr, "\n[%d] + Stopped %s\n", job_number_i, cmdline);
        fflush(stderr);
    }
    else
    {
        if (result == 2)
            printf("\n");
        remove_job_by_number(job_number_i);
    }

    restore_sigchld(oldmask);
    return 0;
}