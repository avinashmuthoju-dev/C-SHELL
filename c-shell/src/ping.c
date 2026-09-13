#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "ping.h"
#include "lexer.h"
#include "background.h"

#define MAX_PING_JOBS 100

static int parse_non_negative(const char *s, long *out)
{
    if (s == NULL || s[0] == '\0')
        return 0;

    char *endptr;
    long v = strtol(s, &endptr, 10);
    if (*endptr != '\0' || v < 0)
        return 0;

    *out = v;
    return 1;
}

int exec_ping(struct token *current)
{
    struct token *tok = current->next;
    if (tok == NULL || tok->value == NULL)
    {
        printf("ping: invalid syntax\n");
        return 1;
    }
    struct token *target_tok = tok;

    tok = tok->next;
    if (tok == NULL || tok->value == NULL || tok->next != NULL)
    {
        printf("ping: invalid syntax\n");
        return 1;
    }
    struct token *sig_tok = tok;

    long sig_typed;
    if (!parse_non_negative(sig_tok->value, &sig_typed))
    {
        printf("ping: invalid syntax\n");
        return 1;
    }
    int actual_signal = (int)(sig_typed % 64);

    char *target_str = target_tok->value;
    int is_job = (target_str[0] == '%');

    struct activity_job jobs[MAX_PING_JOBS];
    int count = get_activities(jobs, MAX_PING_JOBS);

    if (is_job)
    {
        char *endptr;
        long job_number = (target_str[1] == '\0') ? -1
                           : strtol(target_str + 1, &endptr, 10);

        if (target_str[1] == '\0' || *endptr != '\0' || job_number <= 0)
        {
            printf("ping: invalid syntax\n");
            return 1;
        }

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
            printf("ping: no such process found\n");
            return 1;
        }

        kill(-job->pgid, actual_signal);
        printf("Sent signal %ld to %s\n", sig_typed, target_str);
        return 0;
    }
    else
    {
        char *endptr;
        long pid_val = strtol(target_str, &endptr, 10);
        if (*endptr != '\0' || pid_val <= 0)
        {
            printf("ping: invalid syntax\n");
            return 1;
        }

        pid_t target_pid = (pid_t)pid_val;
        int found = 0;

        for (int i = 0; i < count && !found; i++)
        {
            for (int j = 0; j < jobs[i].proc_count; j++)
            {
                if (jobs[i].procs[j].pid == target_pid)
                {
                    found = 1;
                    break;
                }
            }
        }

        if (!found)
        {
            printf("ping: no such process found\n");
            return 1;
        }

        kill(target_pid, actual_signal);
        printf("Sent signal %ld to %ld\n", sig_typed, pid_val);
        return 0;
    }
}