#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Added Headerfile */
/* For system call handler */
#include <stdint.h>                     // For unint32_t
#include "devices/shutdown.h"           // For SYS_HALT
#include "filesys/filesys.h"            // For SYS_CRATE, REMOVE
/* For process hierarchy */
#include "userprog/process.h"           // For SYS_EXEC
/* For file descriptor */
#include "filesys/file.h"               // For using filesys function
#include "threads/synch.h"              // For using lock function
#include "devices/input.h"              // For using input_getc function

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void *sp = f->esp;
  int **arg[3]; 
  check_address(sp);
  int num = *(int*)sp;

  switch(num)
  {
    case SYS_HALT : 
    halt();
    break;

    case SYS_EXIT : 
    get_argument(sp, arg, 1);
    check_address((void*)arg[0]);
    exit((int)*arg[0]);
    break;

    case SYS_EXEC : 
    get_argument(sp, arg, 1);
    check_address((void*)arg[0]);
    f->eax = exec((const char*)*arg[0]);
    break;

    case SYS_WAIT : 
    get_argument(sp, arg, 1);
    check_address((void*)arg[0]);
    f->eax = wait((tid_t)*arg[0]);
    break;

    case SYS_CREATE : 
    get_argument(sp,arg,2);
    check_address((void*)arg[0]);
    check_address((void*)arg[1]);
    f->eax = create((const char*)*arg[0],(unsigned)*arg[1]);
    break;

    case SYS_REMOVE : 
    get_argument(sp,arg,1);
    check_address((void*)arg[0]);
    f->eax = remove((const char*)*arg[0]);
    break;

    case SYS_OPEN : 
    get_argument(sp, arg, 1);
    check_address((void*)arg[0]);
    f->eax = open((const char*)*arg[0]);
    break;

    case SYS_FILESIZE : 
    get_argument(sp, arg, 1); 
    check_address((void*)arg[0]);
    f->eax = filesize((tid_t)*arg[0]);
    break;  

    case SYS_READ : 
    get_argument(sp,arg,3);
    check_address((void*)arg[0]);
    check_address((void*)arg[1]);
    check_address((void*)arg[2]);
    f->eax = read((int)*arg[0],(void*)*arg[1],(unsigned)*arg[2]);
    break;

    case SYS_WRITE :
    get_argument(sp,arg,3); 
    check_address((void*)arg[0]);
    check_address((void*)arg[1]);
    check_address((void*)arg[2]);
    f->eax = write((int)*arg[0],(void*)*arg[1],(unsigned)*arg[2]);
    break;

    case SYS_SEEK : 
    get_argument(sp,arg,2);
    check_address((void*)arg[0]);
    check_address((void*)arg[1]);
    seek((int)*arg[0],(unsigned)*arg[1]);
    break;

    case SYS_TELL : 
    get_argument(sp, arg, 1);
    check_address((void*)arg[0]);
    f->eax = tell((int)*arg[0]);
    break;

    case SYS_CLOSE : 
    get_argument(sp, arg, 1);
    check_address((void*)arg[0]);
    close((int)*arg[0]);
    break;
  }
}

/* Added function */
/* For system call handler */
/* Check whether the address is user area or not */
void
check_address (void *addr)
{
  uint32_t check_add = addr;
  uint32_t Max_add = 0xc0000000;
  uint32_t Min_add = 0x08048000;
  /* If the address is not user area */
  if(!(check_add>Min_add && check_add<Max_add))
  {
    /* Exit proecss */
    exit(-1);
  }
}

/* Save the argument value to kernel as many as count */
void
get_argument (void *esp, int *arg, int count)
{
  int i;
  /* Calculate address where address of argument saved */
  void *r_esp = esp + 4;
  /* Save the argumnet value as many as count */
  for(i = 0; i < count; i++)
  { /* Save the value pointed to by address */
    arg[i] = (uint32_t*)r_esp;
    /* Recalculate the next address of argument saved */
    r_esp = r_esp + 4;
  }
}

/* Shutdown pintos */
void
halt (void)
{
  shutdown_power_off();
}

/* Exit current proecess */
void
exit (int status)
{
  struct thread *cur = thread_current();
  /* Save the exit_status as input status */
  cur->exit_status = status; 
  /* Print name and status of process that exited */
  printf("%s: exit(%d)\n", cur->name, status );
  /* Exited current process */
  thread_exit();
}

/* Create file */
bool
create (const char *file, unsigned initial_size)
{ 
  /* Check the file */
  if (file == NULL)
    exit(-1);
  else
  {
    /* Return value of filesys_create */
    return filesys_create(file, initial_size);
  }
}

/* Remove file */
bool
remove (const char *file)
{
  /* Return value of filesys_remove */ 
  return filesys_remove(file);
}

/* For process hierarchy */
/* Execute child process */
tid_t
exec (const char *cmd_line)
{
  /* Create child process */
  tid_t child_pid = process_execute(cmd_line);
  /* Get the child process */
  struct thread *child_thread = get_child_process(child_pid);
  /* Wait for child process load */
  sema_down(&child_thread->sema_load);
  /* Select return value */
  if(child_thread->success_load == true)
    return child_pid;
  else 
    return -1;
}
/* Wait until child process exit */
int
wait (tid_t tid)
{
  int status = process_wait(tid);
  return status;
}

/* For file descriptor */
/* Open the file */
int 
open (const char *file)
{
  /* Check the file */
  if(file == NULL )
    exit(-1);
  /* Open the file */
  struct file *open_file = filesys_open(file);
  if (open_file != NULL)
  {
    /* Return the fd of file*/
    return process_add_file(open_file);
  }
  return -1;
}
/* Return value of file_size */
int 
filesize (int fd)
{
  /* Find file */
  struct file *size_file = process_get_file(fd);
  if(size_file != NULL)
  {
    /* Return the filesize of file */
    return file_length(size_file);
  }
  return -1;
}
/* Read data of opened file */
int 
read (int fd, void *buffer, unsigned size)
{
  int bytes = 0;
  /* Check the buffer address */
  check_address(buffer);
  char *read_buffer = (char *)buffer;
  /* Prevent simultaneous access to file */
  lock_acquire(&filesys_lock);
  /* If fd is 0 */
  if(fd == 0)
    {
      /* Save the Keyborad input to buffer */
      for(bytes=0; bytes <= size; bytes++)
        read_buffer[bytes] = input_getc();
      /* Add \0 value for data classification */
      read_buffer[bytes] = '\0';
    }
  /* If fd is not 0 */
  else
  {
    /* Find file corresponding to fd */
    struct file *read_file = process_get_file(fd);
    if(read_file != NULL)
    {
      /* Save the size of data to bytes */
      bytes = file_read(read_file, buffer, size);   
    }
  }  
  /* Release the filesys lock */
  lock_release(&filesys_lock);
  return bytes;
}
/* Write data of opened file */
int
write (int fd, void *buffer, unsigned size)
{
  
  int bytes = 0 ;
  /* Prevent simultaneous access to file */
  lock_acquire(&filesys_lock);
  /* If fd is 1 */
  if(fd == 1)
    {
      /* Print data stored in the buffer */
      putbuf((const char*)buffer, size);
      /* Save the size to bytes variable */
      bytes = size;
    }
  /* If fd is 1 */
  else
    {
      /* Find file corresponding to fd */
      struct file *write_file = process_get_file(fd);
      if(write_file != NULL)
      {
        /* Write & Save the size of data to bytes */
        bytes = file_write(write_file, buffer, size);
      }      
    }
  /* Release the filesys lock */
  lock_release(&filesys_lock);
  return bytes;
}
/* Move the file offset */
void 
seek (int fd, unsigned position)
{
  /* Find file corresponding to fd */
  struct file *seek_file = process_get_file(fd);
  if(seek_file != NULL)
  {
    /* Move the offset by position */
    file_seek(seek_file, position);
  }
}
/* Tell the offset position of opened file */
unsigned 
tell (int fd)
{
  /* Find file corresponding to fd */
  struct file *tell_file = process_get_file(fd);
  if(tell_file != NULL)
    /* Return offset of file */
    return file_tell(tell_file);
  else 
    return -1;
}
/* Close the opened file */
void
close (int fd)
{
  /* Find file corresponding to fd */
  struct file *close_file = process_get_file(fd);
  if(close_file != NULL)
  {
    /* Close the file */
    file_close(close_file);
    /* Initialize fd_table entry corresponding to file */
    thread_current()->fd_table[fd] = NULL;
  }
}


