#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "threads/synch.h"
#include <stdio.h>
#include "filesys/directory.h"

/* ---------------------------------------------------------------------- */
/* inode : Structure that holds all information about a FILE or DIRECTORY */
/* ---------------------------------------------------------------------- */

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

// b'512 Wide (BLOCK_SECTOR_SIZE)
#define MAX_DIRECT			123		// 123		Addressable	Indexes
#define MAX_INDIRECT		128		// 128 		Addressable	Indexes
#define MAX_DOUBLY_INDIRECT	16384	// 128*128	Addressable	Indexes
#define EIGHT_MB 8980480
/* On-disk inode. Must be exactly BLOCK_SECTOR_SIZE bytes long. */
/* 	Avoid external fragmentation by direct, inderect, doubly inderect blocks 
		Blocks are 4KB : 64 blocks Total
		80 Files/Directories Total
		- 5 blocks holding inodes
		- 56 blocks holding data
		- 1 block for d-bmap
		- 1 block for i-bmap
		- 1 Super block		
*/
struct inode_disk
{	
	/* -------------------------------------------------------------- */
	/* In other words revise everything that uses inode_disk -> start */
	/* -------------------------------------------------------------- */
	
	// block_sector_t start;						// First data sector.
	// uint32_t unused[125];						// Not used.
	
	off_t length;								// File size in bytes.
	unsigned magic;								// Magic number.
		
	// 124 addressable Direct Blocks
	block_sector_t direct[MAX_DIRECT];
	
	// 128 Max. Indexes
	block_sector_t indirect;
		
	// 128*128 Max. Indexes of Indexes. Doubly Inderect blocks point to Inderect Blocks!
	block_sector_t doubly_indirect;
        bool isDirectory;
};

/* Returns the number of sectors to allocate for an inode SIZE
	 bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
	return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* -------------------------------------------------------- */
/* 	inode includes identification/peripheral information 	*/
/*	inode_disk includes actual data within the inode 		*/	
/* -------------------------------------------------------- */

/* In-memory inode. */
struct inode 
	{
		struct list_elem elem;		/* Element in inode list. */
		block_sector_t sector;		/* Sector number of disk location. */
		int open_cnt;				/* Number of openers. */
		bool removed;				/* True if deleted, false otherwise. */
		int deny_write_cnt;			/* 0: writes ok, >0: deny writes. */
		struct inode_disk data;		/* Inode content. */
		
		struct lock lock;			/* Synchronize during read/wrtie. */
		bool isDirectory;			/* Inodes can be file or dir. */
		block_sector_t previous;
	};

/* --------------------- */
/* Implemented Functions */
/* --------------------- */
bool	fileExtend(struct inode_disk *data, off_t length);
bool	indirectFileExtend(bool single, bool floor, block_sector_t *block, size_t length);
bool	freeInode(struct inode *inode, off_t length);
bool	indirectFreeInode(bool single, bool floor, block_sector_t block, size_t length);

/* ---------------------------------------------------- */
/* Searches for the sector of an inode given its index. */
/* ---------------------------------------------------- */
static block_sector_t findSector(const struct inode_disk *disk, off_t i) {
	//	0 : Direct, 1 : Inderect, 2 : Doubly Inderect 
	int identity = 0;
	block_sector_t found;
	
	// Find if in Direct, Inderect or Doubly Inderect Block Sectors
	if(i < MAX_DIRECT) {identity = 0;}
	else if(i < MAX_INDIRECT + MAX_DIRECT) {identity = 1;}
	else if(i < MAX_DOUBLY_INDIRECT + MAX_DIRECT + MAX_INDIRECT) {identity = 2;}
	else {
		// Index is out of bounds
		return -1;}
	
	// Incase indirect/doubly
	block_sector_t indirect[MAX_INDIRECT];
	
	// Category
	switch(identity) {
		// Direct
		case 0: 
			return disk -> direct[i];
			break;
		
		// Inderect
		case 1:
			// Search through Array of Indexes
			block_read(fs_device, disk->indirect, &indirect);
			found = indirect[i - MAX_DIRECT];
			return found;
			break;
			
		// Doubly Inderect
		case 2:	
			; // *Apparently* labels can't have statements after it
			
			// Get index for first level table
			off_t i1 =	((i 
				- (MAX_DIRECT + MAX_INDIRECT)) 
				/ MAX_INDIRECT
			);
			
			// Get index for second level table
			off_t i2 =	((i 
				- (MAX_DIRECT + MAX_INDIRECT)) 
				% MAX_INDIRECT
			);
			
			// Reads from Table 1 -> Table 2 -> Contents
			block_read(fs_device, disk->doubly_indirect, &indirect);
			block_read(fs_device, indirect[i1], &indirect);
			
			// Doubly indirect inode is found
			found = indirect[i2];
			return found;
			break;
			
		default:
			printf("Impossible");
			return -1;
	}
	
	// Unreachable
	return (-1);
}

/* Returns the block device sector that contains byte offset POS
	 within INODE.
	 Returns -1 if INODE does not contain data for a byte at offset
	 POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
	ASSERT (inode != NULL);
	
	// Pass pos as index
	if (pos <= inode->data.length) {
		return findSector(&inode->data, (pos / BLOCK_SECTOR_SIZE));
	}
	else {
		return -1;
	}
}

/* List of open inodes, so that opening a single inode twice
	 returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
	list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
	 writes the new inode to sector SECTOR on the file system
	 device.
	 Returns true if successful.
	 Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isDirectory)
{
	struct inode_disk *disk_inode = NULL;
	bool success = false;

	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
		 one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);
	if (disk_inode != NULL)
	{
		size_t sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->magic = INODE_MAGIC;
                disk_inode->isDirectory = isDirectory;
		
		/* Allocate Direct, Indirect, Doubly */
		if(fileExtend(disk_inode, length))
		{
			block_write (fs_device, sector, disk_inode);
			/*
			if (sectors > 0) 
			{
				static char zeros[BLOCK_SECTOR_SIZE];
				size_t i;
				
				// Since we are just intializing inode disk w/ zeros, can use 'start'
				for (i = 0; i < sectors; i++) 
					block_write (fs_device, disk_inode->start + i, zeros);
			}
			*/
			success = true; 
		} 
		free (disk_inode);
	}
	return success;
}

/* Reads an inode from SECTOR
	 and returns a `struct inode' that contains it.
	 Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
			 e = list_next (e)) 
		{
			inode = list_entry (e, struct inode, elem);
			if (inode->sector == sector) 
				{
					inode_reopen (inode);
					return inode; 
				}
		}

	/* Allocate memory. */
	inode = malloc (sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* Initialize. */
	list_push_front (&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	lock_init(&inode->lock);
	block_read (fs_device, inode->sector, &inode->data);
	inode->isDirectory = inode->data.isDirectory;	
	return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
	return inode->sector;
}

/* Closes INODE and writes it to disk.
	 If this was the last reference to INODE, frees its memory.
	 If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
	/* Ignore null pointer. */
	if (inode == NULL)
		return;

	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0)
		{
			/* Remove from inode list and release lock. */
			list_remove (&inode->elem);
 
			/* Deallocate blocks if removed. */
			if (inode->removed) 
				{
					free_map_release (inode->sector, 1);
					/* free_map_release (inode->data.start,
					bytes_to_sectors (inode->data.length)); */
					freeInode(
						inode, 
						inode->data.length
					);
				}

			free (inode); 
		}
}

/* Marks INODE to be deleted when it is closed by the last caller who
	 has it open. */
void
inode_remove (struct inode *inode) 
{
	ASSERT (inode != NULL);
	inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
	 Returns the number of bytes actually read, which may be less
	 than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;
	int chunk_size;

	while (size > 0) 
		{
			/* Disk sector to read, starting byte offset within sector. */
			block_sector_t sector_idx = byte_to_sector (inode, offset);
			int sector_ofs = offset % BLOCK_SECTOR_SIZE;

			/* Bytes left in inode, bytes left in sector, lesser of the two. */
			off_t inode_left = inode_length (inode) - offset;
			int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
			int min_left = inode_left < sector_left ? inode_left : sector_left;

			/* Number of bytes to actually copy out of this sector. */
			 chunk_size = size < min_left ? size : min_left;
			if (chunk_size <= 0)
				break;

			if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
				{
					/* Read full sector directly into caller's buffer. */
					block_read (fs_device, sector_idx, buffer + bytes_read);
				}
			else 
				{
					/* Read sector into bounce buffer, then partially copy
						 into caller's buffer. */
					if (bounce == NULL) 
						{
							bounce = malloc (BLOCK_SECTOR_SIZE);
							if (bounce == NULL)
								break;
						}
					block_read (fs_device, sector_idx, bounce);
					memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
				}
			
			/* Advance. */
			size -= chunk_size;
			offset += chunk_size;
			bytes_read += chunk_size;
		}
	if(chunk_size >0){free (bounce);}

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
	 Returns the number of bytes actually written, which may be
	 less than SIZE if end of file is reached or an error occurs.
	 (Normally a write at end of file would extend the inode, but
	 growth is not yet implemented.) */
off_t
inode_write_at (
		struct inode *inode, 
		const void *buffer_, 
		off_t size, 
		off_t offset
		) 
{	

	/* ---------------------------------------- */
	/* Extend inode if write is outside of file */
	/* ---------------------------------------- */
	if(size < 0 || offset < 0) {return 0;}
	off_t totalLength = offset + size;
	int extend = byte_to_sector(inode, totalLength);
	
	// Current file too small : Need to extend file
	if( (extend == -1) && (inode_length(inode) < totalLength) ) {
		
		/* Access to independent files/directories should not block each other */
		if(!(inode->isDirectory)) {
			lock_acquire(&((struct inode *)inode)->lock);
		}
		
		/* Call file extend function, 
		returns -1 if unable to get any more space */
		bool get = fileExtend(&inode->data, totalLength);
		
		// Unable to get space
		if(!get) { return 0; }
		
		// Update inode information
		inode -> data.length = totalLength;
		
		if(!(inode->isDirectory)) {
			lock_release(&((struct inode *)inode)->lock);
		}
	}
	
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;

	if (inode->deny_write_cnt)
		return 0;

	while (size > 0) 
		{
			/* Sector to write, starting byte offset within sector. */
			block_sector_t sector_idx = byte_to_sector (inode, offset);
			int sector_ofs = offset % BLOCK_SECTOR_SIZE;

			/* Bytes left in inode, bytes left in sector, lesser of the two. */
			off_t inode_left = inode_length (inode) - offset;
			int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
			int min_left = inode_left < sector_left ? inode_left : sector_left;

			/* Number of bytes to actually write into this sector. */
			int chunk_size = size < min_left ? size : min_left;
			if (chunk_size <= 0)
				break;

			if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
				{
					/* Write full sector directly to disk. */
					block_write (fs_device, sector_idx, buffer + bytes_written);
				}
			else 
				{
					/* We need a bounce buffer. */
					if (bounce == NULL) 
						{
							bounce = malloc (BLOCK_SECTOR_SIZE);
							if (bounce == NULL)
								break;
						}

					/* If the sector contains data before or after the chunk
						 we're writing, then we need to read in the sector
						 first.	Otherwise we start with a sector of all zeros. */
					if (sector_ofs > 0 || chunk_size < sector_left) 
						block_read (fs_device, sector_idx, bounce);
					else
						memset (bounce, 0, BLOCK_SECTOR_SIZE);
					memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
					block_write (fs_device, sector_idx, bounce);
				}

			/* Advance. */
			size -= chunk_size;
			offset += chunk_size;
			bytes_written += chunk_size;
		}
	free (bounce);

	return bytes_written;
}

/* Disables writes to INODE.
	 May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
	inode->deny_write_cnt++;
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
	 Must be called once by each inode opener who has called
	 inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
	return inode->data.length;
}

/*  Expands inode size from the given length 
	
	Searches Direct -> Indirect -> Doubly Indirect
	for available ('0' value) sectors until 'length' 
	# of sectors have been allocated.
*/
bool 
fileExtend(struct inode_disk *data, off_t length) 
{	
	// Create a file of 0 means don't create anything
	bool pass = false;
	
	// Total sectors of extended file
	size_t remainingSectors = bytes_to_sectors(length);
	
	/*
	if(!length){
		remainingSectors = bytes_to_sectors(1);}
	*/
	/* Fill in new inode previous data until sector number of 'length' reached,
	then fill with blank data.
	
	Direct Fill -> Indirect Fill -> Doubly Indirect Fill	*/
	if(!length)pass=true;

	size_t headSector = 0;
	
	// Can we achieve remaining sectors with Direct blocks?
	size_t tailSector;
	if(remainingSectors < MAX_DIRECT) { 
		tailSector = remainingSectors; }
	else {tailSector = MAX_DIRECT;}
	
	static char zeros[BLOCK_SECTOR_SIZE];
	
	// Search and fill Direct Sectors first
	for(; headSector < tailSector; headSector++) {
		
		/* Allocate sectors to the map (block by block). Fill zeros for sectors
		not originaly initialized from 'data' */
		if(!data->direct[headSector]) {
			
			// Single available Direct Sector block found. Use it.
			if(free_map_allocate(1, &data->direct[headSector])) {
				pass = true; 
				
				// Write into newly allocated block
				block_write(fs_device, data->direct[headSector], zeros);
			}
			else { return false; }
		}
	}
	
	// If no more sectors to meet 'length' # of sectors, extension success
	if(!(remainingSectors - tailSector)) { return pass; }
	remainingSectors = remainingSectors - tailSector;
	
	// Search and fill Indirect Block Sectors
	if(remainingSectors < MAX_INDIRECT) { 
		tailSector = remainingSectors; }
	else {tailSector = MAX_INDIRECT;}
	
	pass = indirectFileExtend(true, false, &data->indirect, tailSector);
	
	// Did it fail Indirect extend?
	if(!pass) return pass;
	
	// If no more sectors to meet 'length' # of sectors, extension success
	if(!(remainingSectors - tailSector)) { return pass; }
	remainingSectors = remainingSectors - tailSector;
	
	// Search and fill Doubly Indirect Block Sectors
	if(remainingSectors < MAX_DOUBLY_INDIRECT) { 
		tailSector = remainingSectors; }
	else {tailSector = MAX_DOUBLY_INDIRECT;}
	
	pass = indirectFileExtend(false, false, &data->doubly_indirect, tailSector);
	
	// Special case : Perfectly allocates and fills all available sectors
	if(!(remainingSectors - tailSector)) { return true; }
	
	// Failed : Asked for too much space, not enough available
	return pass;
}

/*  Indirectly expands inode size from the given length 
	Searches Indirect and Doubly Indirect
	for available ('0' value) sectors until 'length' 
	# of sectors have been allocated.
	'floor' will be true once done allocating all blocks
*/
bool
indirectFileExtend(bool single, bool floor, block_sector_t* block, size_t length)
{	
	// Extend and search indexes or indexes of indexes
	bool pass = false;
	
	// Either an index of indexes, or an index of index of indexes
	struct indirectInodeDisk indirectSearch;
	
	// Is this an indirect or doubly indirect extension?
	bool doubly = !(single);
	
	// Indexes for searching 'indirectSearch' indirectInodeDisk table
	size_t headSector = 0;
	size_t ceiling;
	if(doubly) {ceiling = MAX_INDIRECT;}
	else {ceiling = 1;}
	size_t tailSector = DIV_ROUND_UP (length, ceiling);
	
	static char zeros[BLOCK_SECTOR_SIZE];
	
	/* 	Either reached from indirect index to block or
		from first doubly indirect index to second index to block */
	if(floor) {
		// Free block. Allocate
		if(!(*block)) {
			if(free_map_allocate(1, block)) {	
				block_write(fs_device, *block, zeros);
				return true;
			}
			return false;
		}
		else { return true; }
	}
	
	// Fill before reading from 'block'
	if(!(*block)) {
		if(free_map_allocate(1, block)) {	
			block_write(fs_device, *block, zeros);
		}
		else { return false; }
	}
	
	// Get table of indexes, or table of indexes of indexes
	block_read(fs_device, *block, &indirectSearch);
	
	// Search + Allocate either single or double
	while(headSector < tailSector) {
		
		// Search through until 'length' sectors allocated
		size_t remainingSectors;
		if(length < ceiling) {remainingSectors = length;} 
		else {remainingSectors = ceiling;}
		
		// Get indirect index
		block_sector_t* current = &indirectSearch.indirect[headSector];
		
		if(single) {
			pass = indirectFileExtend(true, true, current, remainingSectors);
		}
		// Doubly Indirect are just indices with Indirect Blocks
		else {
			pass = indirectFileExtend(true, false, current, remainingSectors);
		}
		
		if(!pass) return false;
			
		// 'remainingSectors' closer to target length
		length = length - remainingSectors;
				
		// Search next index
		headSector++;
	}
	
	// Succesfully allocated either indirectly or double indirectly
	block_write(fs_device, *block, &indirectSearch);
	return pass;
}

/* -------------------------------------------------------------------- */
/* Indirectly De-allocate inode blocks starting from Indirect -> Doubly */
/* -------------------------------------------------------------------- */
bool	
indirectFreeInode(bool single, bool floor, block_sector_t block, size_t length)
{
	// Extend and search indexes or indexes of indexes
	bool pass = false;
	
	// Either an index of indexes, or an index of index of indexes
	struct indirectInodeDisk indirectSearch;
	
	// Is this an indirect or doubly indirect extension?
	bool doubly = !(single);
	
	/* 	Either reached from indirect index to block or
	from first doubly indirect index to second index to block */
	if(floor) {
		free_map_release(block, 1);
		return true;
	}
	
	// Indexes for searching 'indirectSearch' indirectInodeDisk table
	size_t headSector = 0;
	size_t ceiling;
	if(doubly) {ceiling = MAX_INDIRECT;}
	else {ceiling = 1;}
	size_t tailSector = DIV_ROUND_UP (length, ceiling);
	
	// Get table of indexes, or table of indexes of indexes
	block_read(fs_device, block, &indirectSearch);
	
	while(headSector < tailSector) {
		
		// Search through until 'length' sectors allocated
		size_t remainingSectors;
		if(length < ceiling) {remainingSectors = length;} 
		else {remainingSectors = ceiling;}
		
		// Get indirect index
		block_sector_t current = indirectSearch.indirect[headSector];
		
		if(single) {
			pass = indirectFreeInode(true, true, current, remainingSectors);
		}
		// Doubly Indirect are just indices with Indirect Blocks
		else {
			pass = indirectFreeInode(true, false, current, remainingSectors);
		}
		
		headSector++;
	}
	
	// Release final (initial) block
	free_map_release(block, 1);
	return pass;
}

/* ------------------------------------------------------------------- */
/* De-allocate inode blocks starting from Direct -> Indirect -> Doubly */
/* ------------------------------------------------------------------- */
bool	
freeInode(struct inode *inode, off_t length) 
{
	// Sector indexes before de-allocation
	size_t remainingSectors = bytes_to_sectors(length);
	size_t headSector = 0;
	size_t tailSector;
	
	// De-allocate sectors JUST within Direct, or de-alloc then seek more
	if(remainingSectors < MAX_DIRECT) {tailSector = remainingSectors;}
	else {tailSector = MAX_DIRECT;}
	
	// Direct	
	while(headSector < tailSector) {
		free_map_release(inode->data.direct[headSector], 1);
		headSector++;
	}
	remainingSectors = remainingSectors - tailSector;
	
	// Only Direct blocks need to free. Done
	if(!remainingSectors) return true;
	
	// De-allocate sectors within Indirect indexes, or de-alloc then seek more
	if(remainingSectors < MAX_INDIRECT) {tailSector = remainingSectors;}
	else {tailSector = MAX_INDIRECT;}
	
	// Indirect
	bool pass = false;
	pass = indirectFreeInode(
		true, 
		false, 
		inode->data.indirect, 
		tailSector
	);
	remainingSectors = remainingSectors - tailSector;	
	
	// Only Direct + Indirect blocks need to free. Done
	if(!remainingSectors) return pass;
	
	// De-allocate sectors within Double Indirect indexes, or de-alloc then seek more
	if(remainingSectors < MAX_DOUBLY_INDIRECT) {tailSector = remainingSectors;}
	else {tailSector = MAX_DOUBLY_INDIRECT;}
	
	// Doubly Indirect
	pass = indirectFreeInode(
		false, 
		false, 
		inode->data.doubly_indirect, 
		tailSector
	);
	remainingSectors = remainingSectors - tailSector;	
	
	if(!remainingSectors) {
		return pass;
	}
	
	// Length was somehow larger than total MB
	return false;
}

bool put_previous(block_sector_t prev, block_sector_t next){
	struct inode* inode = inode_open(next);
	if(!inode){return false;}
	inode->previous = prev;
	inode_close(inode);
	return true;
}

bool is_inode_directory(struct inode *inode){
	return inode->isDirectory;
}


void lock_inode(struct inode * inode){
	lock_acquire(&((struct inode *) inode)->lock);
}

void unlock_inode(struct inode * inode){
	lock_release(&((struct inode *) inode)->lock);
}

block_sector_t get_previous_inode(struct inode *inode){
	return inode->previous;
}

int get_open_count(struct inode * inode){
	return inode->open_cnt;

}
