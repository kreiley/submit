#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}
/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size,bool isDirectory) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = get_dir(name);
  char *filename = get_filename(name);
  bool success = false;
  if(strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0){
  	success = (dir != NULL
               	  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size,isDirectory)
                  && dir_add (dir, filename, inode_sector));
  }
  if (!success && inode_sector != 0){ 
    free_map_release (inode_sector, 1);
}
  dir_close (dir);
  free(filename);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if(strlen(name) == 0){return NULL;}
  struct dir *dir = get_dir(name);
  char * filename = get_filename(name);
  struct inode *inode = NULL;

  if (dir != NULL){
	if(strcmp(filename, "..") == 0){
		bool success = get_previous(dir, &inode);
		if(!success){free(filename); return NULL;}
	}
	else if(check_if_root(dir) && strlen(filename) == 0){
		free(filename);
		return (struct file*) dir;
	}
	else if(strcmp(filename, ".") == 0){
		free(filename);
		return (struct file*) dir;
	}
	else{
		dir_lookup (dir, filename, &inode);
	}
  } 
  dir_close (dir);
  free(filename);
  if(!inode){return NULL;}
  if(is_inode_directory(inode)){return (struct file *) dir_open(inode);}
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = get_dir(name);
  char * filename = get_filename(name);
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 
  free(filename);
  return success;
}

bool
change_directory( const char *name){
  struct dir * dir = get_dir(name);
  char * filename = get_filename(name);
  struct inode * inode = NULL;

  if(dir){
	if(strcmp(filename, "..") == 0){
		bool success = get_previous(dir, &inode);
		if(!success){free(filename); return false;}
	}
	else if(strcmp(filename,".") == 0){
		thread_current()->current_working_dir = dir;
		free(filename);
		return true;
	}
	else if(check_if_root(dir) && strlen(filename) == 0){
		thread_current()->current_working_dir = dir;
		free(filename);
		return true;
	}
	else{
		dir_lookup(dir, filename, &inode);
	}
  }

  dir_close(dir);
  free(filename);
	
  dir = dir_open(inode);
  if(dir){
	dir_close(thread_current()->current_working_dir);
	thread_current()->current_working_dir = dir;
	return true;
  }
  return false;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}


struct dir* get_dir(const char * path){
     char * i;
     char *next = NULL;
     char new_path[strlen(path) + 1];
     memcpy(new_path,path,strlen(path) +1);

     char *name = strtok_r(new_path, "/", &i);
     
     struct dir * dir;

     if(new_path[0] == 47 || thread_current()->current_working_dir == NULL){dir = dir_open_root();}
     else{dir = dir_reopen(thread_current()->current_working_dir);}

     if(name){next = strtok_r(NULL, "/", &i);}
     while(next != NULL){
	if(strcmp(name, ".") != 0){
		struct inode * inode;
		if(strcmp(next, "..") == 0){
			bool success = get_previous(dir, &inode);
			if(!success){return NULL;}
		}
		else{
			bool found = dir_lookup(dir, name, &inode);
			if(!found){return NULL;}
		}
		if(is_inode_directory(inode)){
			dir_close(dir);
			dir = dir_open(inode);
		}
		else{
			inode_close(inode);
		}
	}
	name = next;
	next = strtok_r(NULL, "/", &i);
      }
      return dir;
}

char * get_filename(const char * path){
	char *i;
	char * name;
	char * prev = "";
	char new_path[strlen(path) + 1];
	memcpy(new_path, path, strlen(path) + 1);
	name = strtok_r(new_path, "/", &i);
	while(name != NULL){
		prev = name;
		name = strtok_r(NULL, "/", &i);
	}
	char *filename = malloc(strlen(prev) +1);
	memcpy(filename, prev, strlen(prev) + 1);
	return filename;
}
