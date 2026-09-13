
#include <stdio.h>
 
#include "activities.h"
#include "background.h"


void exec_activities(void)
{
    struct activity_job jobs[MAX_ACTIVITY_PROCS];
    int job_count = get_activities(jobs, MAX_ACTIVITY_PROCS);
 
    for (int i = 0; i < job_count; i++)
    {
        printf("[%d] pgid %d\n", jobs[i].job_number, jobs[i].pgid);
 
        for (int j = 0; j < jobs[i].proc_count; j++)
        {
            printf("  %d %s %s\n",
                   jobs[i].procs[j].pid,
                   jobs[i].procs[j].name,
                   jobs[i].procs[j].stopped ? "Stopped" : "Running");
        }
    }
}
 