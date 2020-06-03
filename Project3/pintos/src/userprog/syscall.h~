#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* Added Headerfile */
/* For system call handler */
#include "threads/thread.h"       /* For using bool variable */
/* For file descriptor */
#include "threads/synch.h"        /* For using lock variable */
#include "vm/page.h"


void syscall_init (void);

/* Added function */
/* For system call handler */
struct vm_entry *check_address (void *addr, void *esp);
void get_argument (void *esp, int *arg, int count);
void halt (void);
void exit (int status);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
/* For process hierarchy */
tid_t exec (const char *cmd_line);
int wait (tid_t tid);
/* For file descriptor */
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/* For virtual memory */
void check_valid_buffer (void *buffer, unsigned size, void *esp, bool to_write);
void check_valid_string (const void *str, void *esp);

/* Added varialbes */
/* For file descriptor */
struct lock filesys_lock;

/* For Memory mapped file */
int mmap (int fd, void *addr);
void munmap (int mapping);
void do_munmap(struct mmap_file *mmap_file);

#endif /* userprog/syscall.h */
