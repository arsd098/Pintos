#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>               

#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "lib/kernel/list.h"

#include "vm/page.h"
/* Added Header file */
#include <threads/palloc.h>       // For using page function
#include <threads/malloc.h>       // For using memset function
#include "threads/vaddr.h"        // For using pg_round_down function
#include "threads/thread.h"       // For using thread & thread functions

/* Added function */
/* For virtual memory */

static unsigned vm_hash_func (const struct hash_elem *e, void *aux);
static bool vm_less_func (const struct hash_elem *a, const struct hash_elem *b);
static void vm_destroy_func (struct hash_elem *e, void *aux);

/* Computes and returns the hash value for hash_element */
static unsigned 
vm_hash_func (const struct hash_elem *e, void *aux)
{
  /* Get the vm_entry of element */
  struct vm_entry *vm_e = hash_entry(e, struct vm_entry, elem);
  /* Return the hash value for vaddr of vm_entry */
  return hash_int((int)vm_e->vaddr);
}

/* Compare vaddr of two hash elements A & B */
static bool
vm_less_func (const struct hash_elem *a, const struct hash_elem *b)
{
  /* Get the vm_entries of element a & b */
  struct vm_entry *vm_a = hash_entry(a, struct vm_entry, elem);
  struct vm_entry *vm_b = hash_entry(b, struct vm_entry, elem);
  /* Compare vaddr of a & b */
  if(vm_a->vaddr < vm_b->vaddr)
    return true;
  else 
    return false; 
}

/* Perfoms destory operation on hash element */
static void
vm_destroy_func (struct hash_elem *e, void *aux)
{
  /* Find vm_entry corressponding hash_element */
  struct vm_entry *vm_e = hash_entry(e, struct vm_entry, elem);
  void *p_addr;
  /* If physicall memory was successfully loaded */
  if(vm_e -> is_loaded == true)
  {
    /* Find physicall address */
    p_addr = pagedir_get_page(thread_current()->pagedir, vm_e->vaddr);
    /* Free the page */
    palloc_free_page(p_addr);
    /* Clear page table */
    pagedir_clear_page(thread_current()->pagedir, vm_e->vaddr);
  }
  /* Free the vm_entry */
  free(vm_e);
}

/* Initialize hash table */
void
vm_init (struct hash *vm)
{
  /* Initialize hash table */
  hash_init(vm, vm_hash_func, vm_less_func, NULL);
}

/* Insert vm_entry into hash_table */
bool 
insert_vme (struct hash *vm, struct vm_entry *vme)
{
  /* If vm_entry successfully insert into hash_table */
  if(hash_insert(vm, &vme->elem)==NULL)
    return true;
  else 
    return false;
}

/* Delete vm_entry in hash_table */
bool
delete_vme (struct hash *vm, struct vm_entry *vme)
{
  /* If vm_entry successfully delete in hash_table */
  if(hash_delete(vm, &vme->elem) != NULL)
  {
    /* Free the memory of vm_entry */
    free(vme);
    return true;
  }
  else 
  {
    free(vme);
    return false;
  }
}

/* Find vm_entry by searching hash_table */
struct vm_entry
*find_vme (void *vaddr)
{
  /* Find page number of vaddr */
  struct vm_entry vm_e;
  struct hash_elem *e;
  vm_e.vaddr = pg_round_down(vaddr);
  /* Find hash_elem of vm_entry */
  e = hash_find(&thread_current()->vm, &vm_e.elem);
  /* If elem is exist, return vm_entry */
  if (e!=NULL)
    return hash_entry(e, struct vm_entry, elem);
  else 
    return NULL;
}

/* Destroy bucket and vm_entry in hash_table */
void 
vm_destroy (struct hash *vm)
{
  /* Destory vm_entry and buket */
  hash_destroy (vm, vm_destroy_func);
}

/* Load the page into physical memory */
bool
load_file (void *kaddr, struct vm_entry *vme)
{
  /* Write read_bytes data to physical memory */
  int bytes_read = file_read_at(vme->file, kaddr, vme->read_bytes, vme->offset);
  /* If data is successfully write */
  if(bytes_read == (int)vme->read_bytes)
  {
    /* Padding the reminding part with 0 */
    memset(kaddr + vme->read_bytes, 0, vme->zero_bytes);
    return true;
  }
  else 
    return false;
}

