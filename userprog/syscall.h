#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"
#include <list.h>
#include <stdbool.h>
#include "filesys/directory.h"
#include "filesys/file.h"

struct fd_elem{
    struct list_elem elem;
    struct file *file;
    int handle;
    struct dir *dir;
    bool is_dir;
};


struct lock locker;
void syscall_init (void);
void exit (int);
#endif /* userprog/syscall.h */
