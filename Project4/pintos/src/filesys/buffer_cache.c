#include <debug.h>
#include <string.h>
#include "threads/palloc.h"
#include "filesys/buffer_cache.h"

/* Global Variable */
/* Buffer Cache */
#define BUFFER_CACHE_ENTRIES 64		// 32KB/512B = 64
// Buffer stored data
static char p_buffer_cache[BUFFER_CACHE_ENTRIES * BLOCK_SECTOR_SIZE];
// Array for buffer_head
static struct buffer_head buffer_head[BUFFER_CACHE_ENTRIES];
// Variable for clock algorithm
static struct buffer_head *clock_hand;
// Lock for buffer_head
static struct lock bc_lock;
	
// Initialize buffer_head struct
void
bc_init(void)
{
  struct buffer_head *bh;
  void *point = p_buffer_cache;
  
  for(bh = buffer_head; bh != buffer_head + BUFFER_CACHE_ENTRIES;)
  {   
      // Allocation buffer_cache in memory
      memset (bh, 0, sizeof(struct buffer_head));
      // Initialize buffer_head variable
      lock_init(&bh->lock);
      bh->buffer = point;
      bh++; 
      point += BLOCK_SECTOR_SIZE;
  }
  clock_hand = buffer_head;
  lock_init(&bc_lock);
}


// Terminate buffer_cache
void
bc_term(void)
{
  // Flush all buffer_cache entry
  bc_flush_all_entries();
}

// Flush all buffer_cache entry
void
bc_flush_all_entries(void)
{
  struct buffer_head *bh;
  // Flush while searching for buffer_cache
  for(bh = buffer_head; bh != buffer_head + BUFFER_CACHE_ENTRIES; bh++)
  {
      lock_acquire(&bh->lock);
      bc_flush_entry (bh);
      lock_release(&bh->lock);
  }
}

// Flush buffer_cache data to Disk
void
bc_flush_entry(struct buffer_head *p_flush_entry)
{
   ASSERT (lock_held_by_current_thread (&p_flush_entry->lock));
   // When entry is not used or is clean 
   if (!p_flush_entry->used || !p_flush_entry->dirty)
      return;
   // Execute flush
   block_write (fs_device, p_flush_entry->sector, p_flush_entry->buffer);
   // Update dirty value
   p_flush_entry->dirty = false;
} 

// Select the victim entry in buffer_cache
struct buffer_head*
bc_select_victim(void)
{
  for( ; ; )
  {
      for( ; clock_hand != buffer_head + BUFFER_CACHE_ENTRIES; clock_hand++)
      {  
            lock_acquire(&clock_hand->lock);
            if(!clock_hand->used || !clock_hand->clock)
		return clock_hand++;
            clock_hand->clock = false;
            lock_release(&clock_hand->lock);
      }
      clock_hand = buffer_head;
  }
  NOT_REACHRED();
}

// Check the caching of disk block
struct buffer_head*
bc_lookup(block_sector_t sector)
{
  lock_acquire(&bc_lock);
  struct buffer_head *bh;
  // Searching for buffer_head
  for(bh = buffer_head; bh != buffer_head + BUFFER_CACHE_ENTRIES; bh++)
  { 
      // When disk block is caching
      if(bh->used && bh->sector == sector)
      {
            lock_acquire(&bh->lock);
            lock_release(&bc_lock);
            return bh;
      }  
  }
  return NULL;
}

// Read using buffer_cache
bool
bc_read(block_sector_t sector_idx, void *buffer, 
	off_t bytes_read, int chunk_size, int sector_ofs)
{
  // Check the caching or not
  struct buffer_head *bh = bc_lookup(sector_idx);
  // No sector in the buffer_head
  if( bh == NULL )
  {
      // Select buffer entry
      bh = bc_select_victim();     
      // Reset entry
      bc_flush_entry(bh);
      bh->dirty = false;
      bh->used = true;
      bh->sector = sector_idx;

      lock_release(&bc_lock);

      // Read
      block_read(fs_device, sector_idx, bh->buffer);
  }
  // Copy the disk block data in buffer
  memcpy(buffer+bytes_read, bh->buffer+sector_ofs, chunk_size);
  // Updata clock bit
  bh->clock = true;
  lock_release(&bh->lock);
  return true;
}

// Write data in buffer_cache
bool
bc_write(block_sector_t sector_idx, void *buffer, 
	 off_t bytes_written, int chunk_size, int sector_ofs)
{
  // Check the caching or not
  struct buffer_head *bh = bc_lookup(sector_idx);
  // No sector in the buffer_head
  if( bh == NULL )
  {
      // Select buffer entry
      bh = bc_select_victim();     
      // Reset entry
      bc_flush_entry(bh);
      bh->used = true;
      bh->sector = sector_idx;

      lock_release(&bc_lock);

      // Read
      block_read(fs_device, sector_idx, bh->buffer);
  }
  // Copy the disk block data in buffer
  bh->dirty =true;
  // Updata clock bit
  bh->clock = true;
  // Write on buffer
  memcpy(bh->buffer+sector_ofs, buffer+bytes_written, chunk_size);
  lock_release(&bh->lock);
  return true;

}

