#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
struct status* give_birth(tid_t tid);
struct status* get_child(tid_t tid);
void kill_child(struct status* child);
void kill_all_the_children(void);
#endif /* userprog/process.h */
