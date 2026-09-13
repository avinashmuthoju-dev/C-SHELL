#ifndef JOBCONTROL_H
#define JOBCONTROL_H

#include <sys/types.h>

void init_job_control(void);

void give_terminal_to(pid_t pgid);
void reclaim_terminal(void);

pid_t get_shell_pgid(void);

int wait_foreground_group(pid_t pgid,
                          pid_t *pids,
                          int process_count);

int handle_eof(void);
void reset_eof_state(void);
void send_sighup_to_all_jobs(void);
int consume_signal_newline(void);
int wait_foreground_group_timeout(pid_t pgid,
                                  pid_t *pids,
                                  int process_count,
                                  int timeout_seconds); /* 0 = no timeout */
                                  
#endif