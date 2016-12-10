#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;

struct indirectInodeDisk {
  block_sector_t indirect[128];
};

void inode_init (void);
bool inode_create (block_sector_t, off_t, bool);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
bool is_inode_directory(struct inode *);
void lock_inode(struct inode *);
void unlock_inode(struct inode *);
block_sector_t get_previous_inode(struct inode *inode);
bool put_previous(block_sector_t prev, block_sector_t next);
bool is_inode_empty(struct inode *inode);
int get_open_count(struct inode * inode);

#endif /* filesys/inode.h */
