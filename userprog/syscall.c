#include "userprog/syscall.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <list.h>
#include <string.h>
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include <stdlib.h>

void check_arg(struct intr_frame *f, int *args, int paremc);
static bool is_user(const void* vaddr);
static struct fd_elem* find_file(int number);
static void syscall_handler (struct intr_frame *);
char * string_to_page(const char * string);

void halt (void);
void exit (int status);
tid_t exec(const char *cmd_line);
int wait (tid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
void valid_kernel(const void *check_valid);

/* Directory Support */
bool chdir(const char *dir);
bool mkdir(const char *dir);
bool readdir(int fd, char *name);
bool isdir(int fd);
int inumber(int fd);

int add_file(struct file * f);
int add_dir(struct dir * d);

static bool is_user(const void* vaddr){
    if(vaddr == NULL) return false;
    
    // User address not initalized
    return (
        vaddr < PHYS_BASE 
        && pagedir_get_page(thread_current()->pagedir, vaddr) 
        != NULL
    );
}

/*
Implement code to read the system call number from the user stack and 
dispatch to a handler based on it.
*/

void
syscall_init (void)
{
  //lock_init(&locker);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
   
    /* Check if esp is valid */
    const void *check_valid = (const void*)f->esp;
//    if 
	//(!
	valid_kernel(check_valid); //{exit(-1);}
   
  int call = * (int *)f->esp;
  int args[3]; // 3 maxargs
   
    // Retreive Arguments
   
   
  switch(call) {
    /* Halt the operating system. */
    case SYS_HALT:
      {
          halt();
        break;
      }
     
    /* Terminate this process. */
    case SYS_EXIT:                 
      {
          check_arg(f, &args[0], 1);
   // valid_kernel((const void*)args[0]);
        
		    exit((int)args[0]);
        break;
      }
     
    /* Start another process. */
    case SYS_EXEC:                 
      {
        check_arg(f, &args[0], 1);
   //   valid_kernel((const void*)args[0]);
        f->eax = exec((const char*)args[0]);
        break;
      }
     
    /* Wait for a child process to die. */
    case SYS_WAIT:                 
      {
    check_arg(f,&args[0], 1);
    f->eax = wait((tid_t)args[0]);
        break;
      }
     
    /* Create a file. */
    case SYS_CREATE:              
      {
        check_arg(f, &args[0],2);
      valid_kernel((const void*)args[0]);
          f->eax = create((const char *)args[0], (unsigned)args[1]);
        break;
      }
     
    /* Delete a file. */
    case SYS_REMOVE:              
      {
          check_arg(f, &args[0], 1);
      valid_kernel((const void*)args[0]);
        f->eax = remove((const char *)args[0]);
        break;
      }
     
    /* Open a file. */
    case SYS_OPEN:                 
      {
        check_arg(f,&args[0],1);
      valid_kernel((const void*)args[0]);
        f->eax = open((const char *)args[0]);
        break;
      }
     
    /* Obtain a file's size. */
    case SYS_FILESIZE:             
      {
          check_arg(f,&args[0], 1);
          f->eax = filesize((int)args[0]);
        break;
      }
     
    /* Read from a file. */
    case SYS_READ:                 
      {
          check_arg(f, &args[0], 3);
      valid_kernel((const void*)args[1]);
          f->eax = read((int) args[0], (void *)args[1], (unsigned) args[2]);
        break;
      }
     
    /* Write to a file. */
    case SYS_WRITE:                
      {
        check_arg(f,&args[0],3);
      valid_kernel((const void*)args[1]);
        f->eax = write((int)args[0],(void *)args[1], (unsigned) args[2]);
		break;
      }
     
    /* Change position in a file. */
    case SYS_SEEK:                 
      {
          check_arg(f,&args[0], 2);
          seek((int) args[0], (unsigned)args[1]);
        break;
      }
     
    /* Report current position in a file. */
    case SYS_TELL:
      {
        check_arg(f,&args[0],1);
        f->eax = tell((int)args[0]);
        break;
      }
     
    /* Close a file. */
    case SYS_CLOSE:
      {
          check_arg(f,&args[0], 1);
          close((int)args[0]);
        break;
      }
    
    /* Change directory */
    case SYS_CHDIR:
    {
     
      check_arg(f, &args[0],1);
      valid_kernel((const void*) args[0]);
      f->eax = chdir((const char *) args[0]);
      
      break;
      
    }
     
    /* Make directory */
    case SYS_MKDIR:
    {
        
      check_arg(f, &args[0], 1);
      valid_kernel((const void*) args[0]);
      f->eax = mkdir((const char *) args[0]);
      
      break;
      
    }
    
    /* Read/Open directory */
    case SYS_READDIR:
    {   
        
      check_arg(f, &args[0], 2);
      valid_kernel((const void*) args[1]);
      f->eax = readdir((int) args[0], (const char *) args[1]);
      
      break;
    }
                        
    /* Verify directory */
    case SYS_ISDIR:
    {
          
      check_arg(f, &args[0], 1);
      f->eax = isdir((int) args[0]);
      
      break;
      
    }
                        
    /* Get inode index number */   
    case SYS_INUMBER:
    {
      check_arg(f, &args[0], 1);
      f->eax = inumber((int) args[0]);
      
      break;
    }
    default:
    {
        exit(-1);
     }
  }
 
  // thread_exit ();
}
void check_arg(struct intr_frame *f, int *args, int paremc){
//    int ptr;
//    ptr = * (int *) f->esp + 1;
    int count = paremc;
    while(count > -1){
        if(!is_user(f->esp+count)){
        exit(-1);
        }
        count --;
    }
//    if(*ptr <SYS_HALT){
//        exit(-1);
//    }
    for(int i =0; i < paremc ; i++){
//        if(!is_user(*(int*)f->esp+i+1)){
//        exit(-1);
//        }
        args[i]=*((int*) f->esp+1+i);
    }
}

/* Terminates Pintos by calling power_off() (declared in threads/init.h). 
This should be seldom used, because you lose some information about
possible deadlock situations, etc. */
void halt (void) {
  shutdown_power_off();
}

/* Terminates the current user program, returning status to the kernel. 
If the process's parent waits for it (see below), 
this is the status that will be returned.

Conventionally, a status of 0 indicates success and nonzero 
values indicate errors. */
void exit (int status) {
    if(status < 0){status = -1;}
    struct thread * t = thread_current();
    if(thread_dead(t->parent) && t->process_status){
		t->process_status->exit_status = status;
    }  
    // Error message at
    printf("%s: exit(%d)\n", t->name, status);
      thread_exit();
}

/* Runs the executable whose name is given in cmd_line, passing any given arguments, 
and returns the new process's program id (pid).
Must return pid -1, which otherwise should not be a valid pid, 
if the program cannot load or run for any reason. 

Thus, the parent process cannot return from the exec until it knows whether 
the child process successfully loaded its executable. You must use 
appropriate synchronization to ensure this. */
tid_t exec (const char *cmd_line) {
	if(!cmd_line){return -1;}
	if(!is_user(cmd_line)){return -1;}
	tid_t tid = process_execute(cmd_line);
	struct status* child = get_child(tid);
	if(!child){return -1;}
	if(child->loaded == 0){sema_down(&child->load);}
	if(child->loaded == 2){kill_child(child);return -1;}
	return tid;
}

/*
If process pid is still alive, waits until it dies. Then, returns 
the status that pid passed to exit, or -1 if pid was terminated by the kernel
(e.g. killed due to an exception).

If pid does not refer to a child of the calling thread, 
or if wait has already been successfully called for the given pid, 
returns -1 immediately, without waiting.

You must ensure that Pintos does not terminate until the initial process exits. 
The supplied Pintos code tries to do this by calling
process_wait() (in userprog/process.c)
from main() (in threads/init.c). We suggest that you implement process_wait() 
according to the comment at the top of the function and then
implement the wait system call in terms of process_wait().

All of a process's resources, including its struct thread, must be freed whether 
its parent ever waits for it or not, and regardless of whether the
child exits before or after its parent.

Children are not inherited: if A has child B and B has child C, 
then wait(C) always returns immediately when called from A, even if B is dead.
Consider all the ways a wait can occur: nested waits (A waits for B, 
then B waits for C), multiple waits (A waits for B, then A waits for C), and so on.
Implementing this system call requires considerably more work than any of the rest.
*/
int wait (tid_t pid) {
    if(!pid){return -1;}
  return process_wait(pid);
}

/* Creates a new file called file initially initial_size bytes in size. 

Returns true if successful, false otherwise. 

Creating a new file does not open it: 
opening the new file is a separate operation which would require a open system call. */
bool create (const char *file, unsigned initial_size) {
    if(!file){return -1;}
    if(!is_user(file)){return -1;} 
  return filesys_create(file, initial_size,false);
}

/* Deletes the file called file. Returns true if successful, false otherwise. 
A file may be removed regardless of whether it is open or closed,
and removing an open file does not close it. See Removing an Open File, for details. */
bool remove (const char *file) {
    if(!file){return -1;}
    if(!is_user(file)){return -1;}
  return filesys_remove(file);
}

/* Opens the file called file. Returns a nonnegative integer handle called a 
"file descriptor" (fd), or -1 if the file could not be opened.
File descriptors numbered 0 and 1 are reserved for the console: fd 0 (STDIN_FILENO) 
is standard input, fd 1 (STDOUT_FILENO) is standard output.

The open system call will never return either of these file descriptors, which are 
valid as system call arguments only as explicitly described below.

Each process has an independent set of file descriptors. File descriptors are not 
inherited by child processes.
When a single file is opened more than once, whether by a single process or 
different processes, each open returns a new file descriptor.

    Different file descriptors for a single file are closed independently 
    in separate calls to close and they do not share a file position. */
int open (const char *file) {
    //lock_acquire(&locker);
    struct file *f = filesys_open(file);
    if(!f){return -1;}
    int handle;
    if(is_inode_directory(file_get_inode(f))){handle =  add_dir((struct dir *) f);}
    else{handle = add_file(f);}
    //handle = add_file(f); /*delete and replace*/
    //lock_release(&locker);
    return handle;
}

/* Returns the size, in bytes, of the file open as fd. */
int filesize (int fd) {
    //lock_acquire(&locker);
    struct fd_elem * f = find_file(fd);
    if(!f){return -1;}
    if(f->is_dir){return -1;}
    int file_size = file_length(f->file);
    //lock_release(&locker);
    return file_size;
}

/* Reads size bytes from the file open as fd into buffer. Returns the number of
bytes actually read (0 at end of file), or -1 if the file could
not be read (due to a condition other than end of file). Fd 0 reads from the 
keyboard using input_getc(). */
int read (int fd, void *buffer, unsigned size) {
    uint8_t * buffer_byte = (uint8_t *) buffer;
    if(buffer_byte == NULL){return -1;}
    if(!is_user(buffer_byte)){return -1;}
    if(fd == STDIN_FILENO){
	for(unsigned i =0; i < size; i++){
		buffer_byte[i] = input_getc();
	}
	return size;
    }
    else{
	//lock_acquire(&locker);
	struct fd_elem * f = find_file(fd);
	if(!f){/*lock_release*/return -1;}
    	if(f->is_dir){/*lock_release*/return -1;}
    	int num_bytes_read = file_read(f->file, buffer_byte, size);
	/*lock_release*/
	return num_bytes_read;
    }
}

/* Writes size bytes from buffer to the open file fd. 
Returns the number of bytes actually written, 
which may be less than size if some bytes could not be written.
Writing past end-of-file would normally extend the file, 
but file growth is not implemented by the basic file system. 

The expected behavior is to
write as many bytes as possible up to end-of-file and return the actual number written, 
or 0 if no bytes could be written at all. */

/* Fd 1 writes to the console. Your code to write to the console should write all 
of buffer in one call to putbuf(),
at least as long as size is not bigger than a few hundred bytes. 
(It is reasonable to break up larger buffers.)

Otherwise, lines of text output by different processes may 
end up interleaved on the console, 
confusing both human readers and our grading scripts. */
int write (int fd, const void *buffer, unsigned size) {
    if(size == 0){return 0;}
    if(!buffer){return -1;}
    if(!is_user(buffer)){return -1;}
    if(fd == STDOUT_FILENO){
	putbuf(buffer,size);
        return size;	
    }
    else{
         /*lock_acquire(lock_acquire(&locker);locker);*/
	struct fd_elem *f = find_file(fd);
        if(!f){/*lock_release*/return -1;}
	if(f->is_dir){/*lock_release*/return -1;}
        int num_bytes_written = file_write(f->file,buffer,size);
	/*lock_release*/
	return num_bytes_written;
    }
}
/* Changes the next byte to be read or written in open file fd to position, 
expressed in bytes from the beginning of the file. 
(Thus, a position of 0 is the file's start.)

A seek past the current end of a file is not an error. 
A later read obtains 0 bytes, indicating end of file.

A later write extends the file, filling any unwritten gap with zeros. 
(However, in Pintos files have a fixed length until project 4 is complete,
so writes past end of file will return an error.) 

These semantics are implemented in the file system and do not require any special 
effort in system call implementation. */
void seek (int fd, unsigned position) {
    /*lock_acquire(lock_acquire(&locker);locker);*/
    struct fd_elem *f = find_file(fd);
    if(!f){/*lock_release*/return -1;}
    if(f->is_dir){/*lock_release*/return -1;}
    file_seek(f->file, position);
    /*lock_release*/
}

/* Returns the position of the next byte to be read or written in open file fd, 
expressed in bytes from the beginning of the file. */
unsigned tell (int fd) {
    /*lock_acquire(lock_acquire(&locker);locker);*/
    struct fd_elem *f = find_file(fd);
    if(!f){/*lock_release*/return -1;}
    if(f->is_dir){/*lock_release*/return -1;}
    off_t off = file_tell(f->file);
    /*lock_release*/
    return off;
}

/* Closes file descriptor fd. Exiting or terminating a process implicitly
closes all its open file descriptors, as if by calling this function for each one. */
void close (int fd) { 
    struct fd_elem  *file_to_close = find_file(fd);
    if(!file_to_close){return -1;}
    if(!file_to_close->file){return -1;}
    /*lock_acquire(lock_acquire(&locker);locker);*/
    if(file_to_close->is_dir){dir_close(file_to_close->file);}
    else{file_close(file_to_close->file);}
    list_remove(&file_to_close->elem);
    free(file_to_close);
    /*lock_release*/
    return;
}

/* Find file element given handle number */
struct fd_elem * find_file (int number){
    /* still to do:  get current file_list*/
    struct thread *current_thread = thread_current();
    struct list_elem *i;
    for(i = list_begin(&current_thread->file_lists);i != list_end(&current_thread->file_lists);i = list_next(i)){
        struct fd_elem *fl = list_entry(i, struct fd_elem, elem);
        if(fl->handle == number){
            return fl;
        }
    }
    return(NULL); /*file not found*/
}

char * string_to_page(const char *string){
    char *page;
   
    page = palloc_get_page(0);
    if (page == NULL){exit(-1);}
    if(string == NULL){exit(-1);}
   
    for(int i = 0; i < PGSIZE; i++){
        if(string >= (char *) PHYS_BASE){
            palloc_free_page(page);
            thread_exit();
        }
        if(page[i] == '\0') {return page;}
    }
    page[PGSIZE -1] = '\0';
    return page;
}

/* Reads a byte at user virtual address UADDR must be below PHYS_BASE. 
Returns the byte value if successful, -1 if a segfault occured. */
static int get_user(const uint8_t *uaddr){
    int result;
    asm ("mov1 $1f, %0; movzb1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
    return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHY_BASE.
   Returns true if successful, false if a seqfault occurred. */
static bool put_user(uint8_t *udst, uint8_t byte) {
    int error_code;
    asm ("mov1 $1f, %0; movb %b2, %1: 1:" : "=&a" (error_code), "=m" (*udst) : "q" (byte));
    return error_code != -1;
}

// 1. User programs cannot access kernel VM
// 2. Kernal threads can access User VM ONLY if User VM is mapped already
void valid_kernel(const void *check_valid) {
    if (!is_user(check_valid)) {exit(-1);}
   
    // Check if process has allocated page
    void *page = pagedir_get_page(thread_current()->pagedir,check_valid);
    if(!page) {exit(-1);}
}
                        
                        
/* ------------------------- */
/*    Filsys System Calls    */
/* ------------------------- */
                        
/* Changes the current working directory of the process to dir, 
which may be relative or absolute. Returns true if successful, false on failure.
 */
bool chdir (const char *dir) {
	if(!dir){return -1;};
	return change_directory(dir);
}

/*
Creates the directory named dir, which may be relative or absolute. 
Returns true if successful, false on failure.

Fails if dir already exists or if any directory name in dir, 
besides the last, does not already exist. That is, mkdir("/a/b/c")

Succeeds only if /a/b already exists and /a/b/c does not.
*/                 
bool mkdir (const char *dir) {
	if(!dir){return -1;}
	return filesys_create(dir,0,true);
}
                        
/*
Reads a directory entry from file descriptor fd, which must represent a directory. 
If successful, stores the null-terminated file name in name, which must have room 
for READDIR_MAX_LEN + 1 bytes, and returns true. 

If no entries are left in the directory, returns false.
"." (This Directory) and ".." (Previous Directory) should not be returned by readdir.

If the directory changes while it is open, then it is acceptable for some 
entries not to be read at all or to be read multiple times. 
Otherwise, each directory entry should be read once, in any order.

READDIR_MAX_LEN is defined in lib/user/syscall.h. 
If your file system supports longer file names than the basic file system, 
you should increase this value from the default of 14.

*/
bool readdir (int fd, char *name) {
	struct fd_elem *f = find_file(fd);
	if(!f){return false;}
	else if(!f->is_dir){return false;}
	if(!name){return false;}
	bool success = dir_readdir(f->dir, name);
	return success;
}

/*
Returns true if fd represents a directory, false if it represents an ordinary file.
*/
bool isdir (int fd) {
	struct fd_elem *f = find_file(fd);
 	if(!f){return -1;}
    	return f->is_dir;       
}

/*
Returns the inode number of the inode associated with fd, 
which may represent an ordinary file or a directory.

An inode number persistently identifies a file or directory. 
It is unique during the file's existence. 

In Pintos, the sector number of the inode is suitable for use as an inode number.            
*/
int inumber (int fd) {
	struct fd_elem *f = find_file(fd);
	if(!f){return -1;}
	if(f->is_dir){
		return inode_get_inumber(dir_get_inode(f->dir));
	}
	else {
		return inode_get_inumber(file_get_inode(f->file));
	}
}          

int add_dir(struct dir *d){
	struct fd_elem *fd = malloc(sizeof(struct fd_elem));
	if(!fd){return -1;}
	struct thread * t = thread_current();
	fd->is_dir = true;
	fd->dir = d;
	fd->handle = t->handle;
	t->handle++;
	list_push_back(&t->file_lists, &fd->elem);
	return fd->handle;
}

int add_file(struct file *f){
	struct fd_elem *fd = malloc(sizeof(struct fd_elem));
	struct thread * t = thread_current();
	if(!fd){return -1;}
	fd->is_dir = false;
	fd->file = f;
	fd->handle = t->handle;
	t->handle++;
	list_push_back(&t->file_lists, &fd->elem);
	return fd->handle;
}

          
