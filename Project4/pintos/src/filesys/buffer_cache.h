#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H
	
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/synch.h"

/* Declare buffer cahce structure */	
struct buffer_head
{
  bool dirty;			// Flag for dirty or not
  bool used;			// Flag for used or not
  block_sector_t sector;	// Address of disk sector
  bool clock;			// Clock Bit  
  struct lock lock;		// Lock variable
  void *buffer;			// Point buffer_cache_entry
};

/* Added function */
void bc_init (void);
void bc_term (void);
bool bc_read(block_sector_t sector_idx, void *buffer, 
	     off_t bytes_read, int chunk_size, int sector_ofs);
bool bc_write(block_sector_t sector_idx, void *buffer, 
	     off_t bytes_written, int chunk_size, int sector_ofs);
void bc_flush_entry(struct buffer_head *p_flush_entry);
void bc_flush_all_entries(void);
struct buffer_head *bc_select_victim (void);
struct buffer_head *bc_lookup (block_sector_t);
	
#endif

 
