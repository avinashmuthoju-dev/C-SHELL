#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <sys/types.h>
#include <signal.h>

struct output_file;
#define MAX_ACTIVITY_PROCS 100

struct activity_proc {
    pid_t pid;
    char name[64];
    int stopped;
};
 
struct activity_job {
    int job_number;
    pid_t pgid;
    struct activity_proc procs[MAX_ACTIVITY_PROCS];
    int proc_count;
    char cmdline[256]; 
};

void init_background(void);

void register_background_job(pid_t pgid, pid_t *pids, char **names, int proc_count,const char *cmdline);

void drain_background_notifications(void);

int get_activities(struct activity_job *out, int max_jobs);
sigset_t block_sigchld(void);
void restore_sigchld(sigset_t oldmask);

int run_background(char *path,char **argv,char **input_files,int input_file_count,struct output_file *files,
                   int file_count);

void register_stopped_job(pid_t pgid,
                          pid_t *pids,
                          char **names,
                          int proc_count,
                          const char *cmdline);

int  mark_job_running(int job_number);
int  mark_job_stopped(int job_number);
int  remove_job_by_number(int job_number);
void set_job_cmdline(pid_t pgid, const char *cmdline);

#endif
