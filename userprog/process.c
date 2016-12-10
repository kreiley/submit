#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static thread_func start_process NO_RETURN;
bool load (const char *cmdline, void (**eip) (void), void **esp);
void destroy_all_files(void);
int process_status_wait_for_child_to_die(tid_t child_tid);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
      char *fn_copy;
    char *fn_copy_ex;
      tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
    char *save_ptr;
 
    fn_copy = palloc_get_page (0);
	if (fn_copy == NULL){return TID_ERROR;}
    strlcpy (fn_copy, file_name, PGSIZE);
    
	fn_copy_ex = palloc_get_page(0);
	if(fn_copy_ex == NULL){return TID_ERROR;}
	
    strlcpy (fn_copy_ex, file_name, PGSIZE); 

	char* pname = strtok_r(fn_copy, " ", &save_ptr);

      /* Create a new thread to execute FILE_NAME. */
     
    tid = thread_create (pname, PRI_DEFAULT, start_process, fn_copy_ex);

    struct file *f = filesys_open(pname);
    if (f){// return -1;}

    file_deny_write(f);
    palloc_free_page(fn_copy);
      thread_current()->rox = f;
    }   
    if (tid == TID_ERROR){palloc_free_page (fn_copy_ex);}
/* 
     else{
        sema_down(&inited);
		struct thread *child = get_thread_with_tid(tid);
		if(child == TID_ERROR){
		thread_unblock(child);		
		if(!child->success){ return -1;}
		}
		process_status_add_child();
        //sema_up(&inited);
    }
*/
    return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
      char *file_name = file_name_;
      struct intr_frame if_;
      bool success;
 
    /* Initialize interrupt frame and load executable. */
      memset (&if_, 0, sizeof if_);
      if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
      if_.cs = SEL_UCSEG;
      if_.eflags = FLAG_IF | FLAG_MBS;
      success = load (file_name, &if_.eip, &if_.esp);
    
     /* If load failed, quit. */
      palloc_free_page (file_name);
  	struct thread * cur = thread_current();
	if(success){
		cur->process_status->loaded = 1;
	}
	else{
		cur->process_status->loaded = 2;
	}

        if(!cur->current_working_dir){cur->current_working_dir = dir_open_root();}
	sema_up(&cur->process_status->load);
	cur->success = success;
	if(!success){thread_exit();}
/*
	struct thread *cur = thread_current();
	cur -> success = success;
	
 if (!success) { thread_exit ();}
      else{
	int fail =  process_status_init();
    if(fail == -1){ cur->success = false;}
		}

		sema_up(&inited);   
*/   
    /* Start the user process by simulating a return from an
        interrupt, implemented by intr_exit (in
         threads/intr-stubs.S).  Because intr_exit takes all of its
         arguments on the stack in the form of a `struct intr_frame',
         we just point the stack pointer (%esp) to our stack frame
         and jump to it. */
 
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
      NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.
   This function will be implemented in problem 2-2.  For now, it
   does nothing. */

/* waits for a child process and retrieves the child's exit status.
if pid is still alive waits until it terminates.
then returns the status that pid passed to exit. (write this in exit)
if pid did not call exit(), but was terminated by the kernel( e.g. killed due to an exception), then
wait must return -1.
it is perfectly legal for a parent process to wait for child processes that have already terminated...
by the time the parent calls wait,
but the kernel must still alow the parent to retrieve its child's exit status or learn that
the child was terminated by the kernel.
wait must fail and return -q immediately if any of the following conditons*/

/* return -1, pid does not refer to a direct child of the calling process.
pid is a direct child of the calling process if and only if the calling process recieved pid
as a return value from a succesful call to exec*/

/* Note that children are not inherited: if A spawns child B and B spawns child process C,
   then A cannot wait for C, even if B is dead. A call to wait(c) by process A must fail.
   Similarly, orphaned process are not assigned to a new parent if their parent process exits before they do.*/

/* return -1, if the process that calls wait has aready called wait on pid. That is, a process may wait for any
   given child a most once. */

/* processes may spawn any number of children, wait for them in any order, and may even exit without having
   waited for some or all of their children. Your design should consider all the ways in which wait can occur.
   All of a process's resources, includeing its struct thread, must be freed whether its parent ever waits for
   it or not, and regardless of whether the child exits befor or after its parent.
*/

/* you must ensure that Pintos does not termnate until the initial process exits.*/

int
process_wait (tid_t child_tid UNUSED)
{
    return process_status_wait_for_child_to_die(child_tid);
}

/* Free the current process's resources. */
void
process_exit (void)
{
      struct thread *cur = thread_current ();
      uint32_t *pd;
    destroy_all_files();
    if(cur->rox){file_close(cur->rox);}
    kill_all_the_children();
    if(!thread_current()->current_working_dir){
	dir_close(thread_current()->current_working_dir);
    }

    if(thread_dead(cur->parent) && cur->process_status){
	cur->process_status->died = true;
	sema_up(&cur->process_status->dead);
    }

      /* Destroy the current process's page directory and switch back
         to the kernel-only page directory. */
      pd = cur->pagedir;
      if (pd != NULL)
        {
            /* Correct ordering here is crucial.  We must set
            cur->pagedir to NULL before switching page directories,
            so that a timer interrupt can't switch back to the
            process page directory.  We must activate the base page
            directory before destroying the process's page
            directory, or our active page directory will be one
            that's been freed (and cleared). */
              cur->pagedir = NULL;
              pagedir_activate (NULL);
              pagedir_destroy (pd);
        }
   
    //process_status_kill_self(cur->process_status, 1);
    //process_status_kill_the_children();

}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
 
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
    char* copy_fn;
    copy_fn = palloc_get_page (0);
      if (copy_fn == NULL){return TID_ERROR;}
    char *save_ptr;
    strlcpy (copy_fn, file_name, PGSIZE);
   
      file_name = strtok_r(file_name, " ", &save_ptr);


  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL)
    {
      goto done;
    }

   // file_deny_write(file);
   
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

   
   
  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;
    //char* argv[128];
  char* argv[24];
 

   
  char *token;
 // char *save_ptr;
  argv[0] = strtok_r(copy_fn, " ", &save_ptr);
  int argc = 1; //firstcmd
 
  /*int counter = 0;*/
  while((token = strtok_r(NULL, " ", &save_ptr))!=NULL)
  {
    argv[argc] = token;
     /*counter ++;*/
    argc++;
  } /* now argc will have number of cmds and argv will have tokenized command*/
   
    argv[argc]= NULL;
/*if no error set up stack*/
  int argc_count = argc;
   
   
    uint32_t ** argv_pointer = (uint32_t**) malloc(sizeof(uint32_t)*25); 

/*put int char for argv*/

int tester = 0;
   
    /*(if(argv[argc_count] != NULL)
        tester = (strlen(argv[argc_count])+1)*sizeof(char);
    */
    int counter_letter =0;
 while(argc_count > -1)
  {
   
    if(argv[argc_count] != NULL)
    {
        tester = (strlen(argv[argc_count])+1);
        *esp = *esp - (tester); /* cmd put in from right to left ! so just use i instead of making new counter*/
   
        argv_pointer[argc_count] = (uint32_t *)*esp;                /*put in the address of esp to remember where argv[i] is*/

        memcpy(*esp,argv[argc_count],tester);/*copy over , by doing strlen+1 i copy over null as well? or it's initialized to 0 from start*/
 
        counter_letter = counter_letter + tester;    /*so shouldn't metter to much check here later if i get errors*/

    }
    else{ tester = 0;}
  
    argc_count--;
  }

int filler = counter_letter%4;


    *esp = *esp - filler;
     *esp = *esp - 4;
     (*(uint32_t *)(*esp)) = 0; /*  buffer 0 thing*/


  argc_count = argc;

   while( argc_count != 0)
  {
    *esp = *esp - 4;/*32bit?*/
    (*(uint32_t **)(*esp)) = argv_pointer[argc_count-1];
    argc_count--;
   }
   
    *esp = *esp - 4;
    (*(uintptr_t  **)(*esp)) = (*esp+4); /* argv -> argv[0]*/
    *esp = *esp - 4;
    *(int *)(*esp) = argc;
    *esp = *esp - 4;
    (*(int *)(*esp))=0;    /* return address =0 */

   
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
 /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}






/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
 
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:
        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.
        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.
   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false;
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
        {
          palloc_free_page (kpage);
          return false;
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp)
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE-4;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
/*
int process_status_init(void){
    struct thread *cur = thread_current();
    struct status *cps = malloc(sizeof(struct status));
    cur->process_status = malloc(sizeof *cur->process_status);
    current_process_status = cur->process_status;
    if(current_process_status == NULL){return -1;}
    current_process_status->tid = cur->tid;
    current_process_status->child = 1;
    current_process_status->parent = 1;
    current_process_status->exit_status = -1;
    sema_init(&current_process_status->process_dead, 0);
    init = 1;
    sema_up(&inited);
    return 1;
}

void process_status_add_child(void){
    struct thread *cur = thread_current();
    list_push_back(&cur->child_processes, &current_process_status->elem);
}
*/
int process_status_wait_for_child_to_die(tid_t child_tid){
    /*struct thread *cur = thread_current();
    struct list_elem *i;
    struct status *child_status;
    for(i = list_begin(&(cur->child_processes)); i != list_end(&(cur->child_processes)); i = list_next(i)){
        child_status = list_entry(i, struct status, elem);
        if(child_status->tid == child_tid){
            sema_down(&child_status->process_dead);
            int exit_status = child_status->exit_status;
            list_remove(i);
            return exit_status;   
        }
    }
    return -1;
    */
    struct status * child = get_child(child_tid);
    if(!child){return -1;}
    if(child->waiting == true){return -1;}
    child->waiting = true;
    if(child->died == false){sema_down(&child->dead);}
    int last_words = child->exit_status;
    kill_child(child);
    return last_words;
}
/*
void process_status_kill_self(struct status *ps, int child_or_parent){
    if(child_or_parent == 1){ps->parent = ps->parent -1;}
    else if(child_or_parent == 2){ps->child = ps->child -1;}
    sema_up(&ps->process_dead);
    if(ps->child + ps->parent <= 0){free(ps);}
   
}

void process_status_kill_the_children(void){
    struct thread *cur = thread_current();
	if(cur->process_status != NULL){
		struct status *child_status = cur->process_status;
	//	printf("%s: exit(%d)\n", cur->name, child_status->exit_status);
		process_status_kill_self(cur->process_status, 1);
	}   
	if(cur->process_status == NULL) {sema_up(&inited);}
	struct list_elem *i;
    struct list_elem *next_child = list_begin(&(cur->child_processes));
    struct status *child_status;
    for(i = list_begin(&(cur->child_processes)); i != list_end(&(cur->child_processes)); i = next_child){
        child_status = list_entry(i, struct status, elem);
        next_child = list_remove(i);
        process_status_kill_self(child_status, 2);
    }
	 
}
*/
struct status* give_birth(tid_t tid){
	struct status * child = malloc(sizeof(struct status));
	if(!child){return NULL;}
	child->tid = tid;
	child->loaded = 0;
	child->waiting = false;
	child->died = false;
	sema_init(&child->load, 0);
	sema_init(&child->dead, 0);
	list_push_back(&thread_current()->child_processes, &child->elem);
	return child;
}

struct status* get_child(tid_t tid){
	struct thread * t = thread_current();
	struct list_elem * i;
	for (i = list_begin(&t->child_processes); i != list_end(&t->child_processes); i = list_next(i)){
		struct status *cps = list_entry(i, struct status, elem);
		if(cps->tid == tid){
			return cps;
		}
	}
	return NULL;
}

void kill_child(struct status * child){
	list_remove(&child->elem);
	free(child);
}

void  kill_all_the_children(void){
	struct thread * t = thread_current();
	struct list_elem * i = list_begin(&t->child_processes);
	struct list_elem * next_child = i;
	while(i != list_end(&t->child_processes)){
		next_child = list_next(i);
		struct status *cps = list_entry(i, struct status, elem);
		kill_child(cps);
		i = next_child;
	}
}

void destroy_all_files(void){
	struct thread * cur = thread_current();
	struct list_elem *i = list_begin(&cur->file_lists);
	struct list_elem *next_file = i;
	while(i != list_end(&cur->file_lists)){
		next_file = list_next(i);
		struct fd_elem *fd = list_entry(i, struct fd_elem, elem);
		if(fd->is_dir){dir_close(fd->dir);}
		else{file_close(fd->file);}
		list_remove(&fd->elem);
		free(fd);
		i=next_file;
	}
} 
