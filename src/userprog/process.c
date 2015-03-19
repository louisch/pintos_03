#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/list.h>
#include <kernel/hash.h>
#include <user/syscall.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/read_page.h"
#include "userprog/install_page.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/filesys_lock.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#ifdef VM
#include <vm/frame.h>
#include <vm/stack_growth.h>
#include <vm/supp_page.h>
#endif

#define ABNORMAL_EXIT_STATUS -1
#define EXEC_FILE 2

/* Maximum number of allowed open files per process. */
static unsigned OPEN_FILE_LIMIT = 128;

static thread_func start_process NO_RETURN;

static persistent_info *create_persistent_info (struct thread *t);
static bool load (char *cmdline, void (**eip) (void), void **esp);

static void process_info_free (process_info *info);

/* fd_file hash related funcitons */
static unsigned fd_hash_func (const struct hash_elem*, void*);
static void fd_hash_destroy (struct hash_elem *e, void *aux UNUSED);
static bool fd_less_func (const struct hash_elem *a,
                          const struct hash_elem *b,
                          void *aux UNUSED);

/* children hash related funcitons */
static unsigned children_hash_func (const struct hash_elem*, void*);
static void children_hash_destroy (struct hash_elem *e, void *aux UNUSED);
static bool children_less_func (const struct hash_elem *a,
                          const struct hash_elem *b,
                          void *aux UNUSED);

static void print_exit_message (const char *file_name, int status);

enum process_status
  {
    NO_REFERENCES,
    RUNNING_WITH_PARENT = 2
  };
static void process_persistent_info_counter_decrement (persistent_info *info);

/* Struct for linking files to fds. */
struct file_fd
  {
    int fd;
    struct file *file;
    struct hash_elem elem;
  };

/* Same as process_execute, but returns a pid_t instead.
   This will be PID_ERROR if a thread could not be created.
   This pid can be used to access the process_info attached to
   the process. */
pid_t
process_execute_pid (const char *file_name)
{
  persistent_info *persistent_c_info = process_execute_aux (file_name);
  return persistent_c_info == NULL ? PID_ERROR : persistent_c_info->pid;
}

/* Return info on the currently running process.
   Returns NULL if no p_info is present (PID_ERROR). */
process_info *
process_current (void)
{
  return &thread_current ()->p_info;
}

/* Performs the work of the process_execute functions, returning
   the process_info created. */
persistent_info *
process_execute_aux (const char *file_name)
{
  char *fn_copy;
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (PAL_NONE);
  if (fn_copy == NULL)
    return NULL;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  persistent_info *child_info = thread_create_thread (file_name, PRI_DEFAULT,
                                             start_process, fn_copy);

  if (child_info == NULL || child_info->pid == TID_ERROR)
    {
      palloc_free_page (fn_copy);
      // HAXX WHAT DO
    }

  /* Wait for child if it hasn't loaded, or unblocks immediately if child has
     loaded (and thus called sema_up). */
  sema_down (&child_info->wait_sema);

  return child_info;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  struct thread *t = thread_current ();
#ifdef VM
  supp_page_table_init (&t->supp_page_table);
#endif

  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  persistent_info *persistent_info = process_current ()->persistent;
  if (!success)
    {
      persistent_info->pid = ABNORMAL_EXIT_STATUS;
    }

  sema_up (&persistent_info->wait_sema);

  strlcpy (t->name, file_name, sizeof t->name);
  /* If load failed, quit. */
  /* This file_name is allocated above in process_execute_aux. */
  palloc_free_page (file_name);

  if (!success)
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Creates a process_info from a thread and adds it to the hash table.
   If a TID_ERROR is passed in, then the pid will be set to PID_ERROR,
   and the pid set to TID_ERROR. Also, other fields will not be
   initialized. The process_info will also not be added to the hash
   table. */
void
process_create_process_info (struct thread *t)
{
  struct process_info *info = &t->p_info;
  /* Init children hashtable. */
  hash_init (&info->children, children_hash_func, children_less_func, NULL);
  /* Init open_files hashtable. */
  info->fd_counter = 2; /* 0 and 1 are reserved for stdin and stdout. */
  hash_init (&info->open_files, fd_hash_func, fd_less_func, NULL);
  /* Add information for process waiting: */
  /* Create persistent_info struct. */
  persistent_info *persistent_info = create_persistent_info (t);
  /* Point the child's process_info at its parent's persistent_info. */
  info->persistent = persistent_info;
  /* Add persistent_info to the parent's chidren hash. Surprisingly there are no
     concurrency issues here; if the child terminates before its persistent_info is
     added to the parent's hash, the correct information will still be added. */
  hash_insert (&process_current ()->children, &persistent_info->persistent_elem);
}

static persistent_info *
create_persistent_info (struct thread *t)
{
  struct process_info *p_info = &t->p_info;
  persistent_info *persistent_info = calloc (1, sizeof *persistent_info);
  if (persistent_info == NULL) thread_exit ();


  persistent_info->pid = t->tid;
  persistent_info->exit_status = ABNORMAL_EXIT_STATUS;
  persistent_info->reference_counter = RUNNING_WITH_PARENT;
  persistent_info->process_info = p_info;

  lock_init (&persistent_info->persistent_info_lock);
  sema_init (&persistent_info->wait_sema, 0);

  return persistent_info;
}

/* Waits for process PID to die and returns its exit status.  If it was
   terminated by the kernel (i.e. killed due to an exception), returns
   ABNORMAL_EXIT_STATUS.  If PID is invalid or if it was not a child of the
   calling process, or if process_wait() has already been successfully called
   for the given PID, returns ABNORMAL_EXIT_STATUS immediately, without
   waiting. */
int
process_wait (pid_t child_pid)
{
  /* Hashtable retrieval related things. */
  persistent_info temp_child_info;
  temp_child_info.pid = child_pid;
  struct hash_elem *child_hash_elem;

  /* Search children hashtable for a persistent_info with child_pid, storing the
     result in child_hash_elem. */
  child_hash_elem = hash_find (&process_current ()->children,
                               &temp_child_info.persistent_elem);

  if (child_hash_elem != NULL)
    {
      persistent_info *child_info = hash_entry (child_hash_elem, persistent_info, persistent_elem);

      /* If the process is still running, we block.
         If the process has exited, the semaphore will already be upped.
         So we can down it either way. */
      sema_down (&child_info->wait_sema);

      int status = child_info->exit_status;

      /* Remove persistent_info from hash so it cannot be waited on again. */
      hash_delete (&process_current ()->children, child_hash_elem);
      /* Decrement child's persistent data counter. */
      process_persistent_info_counter_decrement (child_info);
      return status;
    }
  else
    {
      /* Child not found, meaning it is not a child of the current process
         or has already been waited on. */
      return ABNORMAL_EXIT_STATUS; /* See function comment. */
    }
}

/* Utility function for printing exit messages from process exit. */
static void
print_exit_message (const char *file_name, int status)
{
  printf ("%s: exit(%d)\n", file_name, status);
}

/* Free the current process's resources. */
void
process_exit (void)
{
  process_info *process = process_current ();
  /* If process_current still has a parent, send it status information and
     unblock it if necessary. */
  persistent_info *persistent_info = process->persistent;
  lock_acquire (&persistent_info->persistent_info_lock);

  int exit_status = persistent_info->exit_status;

  /* Just up the sema: if parent is waiting, it is unblocked.
     If parent wants to wait in future, parent immediately unblocks. */
  sema_up (&persistent_info->wait_sema);

  lock_release (&persistent_info->persistent_info_lock);
  /* Decrement counter of persistent info and free it if no references exist. */
  process_persistent_info_counter_decrement (persistent_info);

  /* Orphan all children, free all persistent_infos and destroy the children and fd
     hashtable. Also free the process_info itself. */
  process_info_free (process);

  struct thread *cur = thread_current ();
  uint32_t *pd;
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
  print_exit_message (cur->name, exit_status);
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
static bool put_args_on_stack (void **esp, const char *args, int arg_length);

static const char* delimiters = " \n\t\0";

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (char *fn_args, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Separate file name from command-line arguments. */
  int arg_length = strlen (fn_args);
  const char* file_name = strtok_r (NULL, delimiters, &fn_args);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Lock filesystem to deny write to file that is being read. */
  filesys_lock_acquire ();
  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL)
    {
      filesys_lock_release ();
      printf ("load: %s: open failed\n", file_name);
      goto done;
    }
  /* Deny write to opened executables. */
  file_deny_write (file);
  process_add_file (file);

  /* Read and verify executable header. */
  off_t bytes_read = file_read (file, &ehdr, sizeof ehdr);
  filesys_lock_release ();

  if (bytes_read != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      filesys_lock_acquire ();
      if (file_ofs < 0 || file_ofs > file_length (file))
        {
          filesys_lock_release ();
          goto done;
        }
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        {
          filesys_lock_release ();
          goto done;
        }
      filesys_lock_release ();

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

  /* Set up command arguments on stack. */
  success = put_args_on_stack (esp, file_name, arg_length);

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

 done:
  /* We arrive here whether the load is successful or not.
     We also reallow writes to the files again. */
  return success;
}

/* Writes arguments to stack according to the calling convention.
   Note that the caller already guarantees that the arg_string
   actually contains arguments. */
static bool
put_args_on_stack (void **esp, const char *arg_string, int arg_length)
{
  ASSERT (arg_string != NULL && arg_length > 0);

  bool success = false;

  int read = arg_length ? arg_length - 1 : arg_length;
  char *esp_char = *esp;
  /* arg_length/2+1 is the maximum possible number of distincts args in string.*/
  uint32_t argv[arg_length/2 + 1];
  uint32_t argc = 0;

  while (read >= 0 && (void *) esp_char <= PHYS_BASE)
    {
      /* Skip delimiter characters. */
      while (read >= 0 && strchr (delimiters, arg_string[read]) != NULL)
        {
          --read;
        }
      /* Write characters from arg_string onto stack. */
      --esp_char;
      *esp_char = '\0';
      while (read >= 0 && strchr (delimiters, arg_string[read]) == NULL
                       && (void *) esp_char <= PHYS_BASE)
        {
            *--esp_char = arg_string[read--];
        }
      /* Remember position of first character of the argument. */
      argv[argc++] = (uint32_t) esp_char;
    }

  /* Word-align esp address. */
  uint8_t *esp_fill = (uint8_t *) esp_char;
  while ((uint32_t) esp_fill % (uint32_t) 8 != 0)
  {
    *--esp_fill = 0;
  }

  uint32_t *esp_pointer = (uint32_t *) esp_fill;
  /* Check that we have enough space to fit all remaining relevant information
     to the stack.  The first 4 is the amount of extra arguments on top of the
     pointers to argv, while the second 4 is the size of each pointer. */
  if ((uint32_t) esp_pointer < (argc + 4) * 4
       || (void *) esp_pointer > PHYS_BASE)
  {
    return success;
  }

  /* Add pointer addresses to arguments. */
  /* argv[argc] = nullptr */
  *--esp_pointer = 0;
  uint32_t i;
  for (i = 0; i < argc; ++i)
      *--esp_pointer = argv[i];

  /* Pointer to argv. */
  --esp_pointer;
  *esp_pointer = (uint32_t) (esp_pointer + 1);

  /* argc value. */
  *--esp_pointer = argc;

  /* Return address. */
  *--esp_pointer = 0;

  /* Set esp pointer to bottom of stack. */
  *esp = esp_pointer;

  success = true;
  return success;
}

/* load() helpers. */

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

#ifndef VM
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
      filesys_lock_acquire ();
      if (!read_page (kpage, file, page_read_bytes, page_zero_bytes))
        {
          filesys_lock_release ();
          return false;
        }
      filesys_lock_release ();

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
#else
#ifdef VM
#endif
  struct thread *t = thread_current ();
  supp_page_set_file_data (supp_page_create_segment (&t->supp_page_table, upage,
                                                     writable, read_bytes + zero_bytes),
                           file, ofs, read_bytes);
#endif

  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp)
{
  void *upage = ((uint8_t *) PHYS_BASE) - PGSIZE;

#ifndef VM
  uint8_t *kpage;
  const bool WRITABLE = true;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (upage, kpage, WRITABLE);
      if (success)
        {
          *esp = PHYS_BASE;
        }
      else
        {
          palloc_free_page (kpage);
        }
    }
  return success;
#else
  *esp = PHYS_BASE;
  stack_growth_init ();
  /* Create the page right now instead of waiting for it to fault,
     as some kernel code needs it set up anyway. */
  supp_page_map_addr (&thread_current ()->supp_page_table, upage);

  return true;
#endif
}

/* Frees the process info struct, destroys its children and fd hashes. */
static void
process_info_free (process_info *info)
{
  hash_destroy (&info->open_files, fd_hash_destroy);
  /* Frees all children_info and destroys children hashtable. */
  hash_destroy (&info->children, children_hash_destroy);
}

/* Adds file to open_files hash. Returns the fd it generates. */
int
process_add_file (struct file *file)
{
  process_info *process = process_current ();
  int fd = process->fd_counter++;
  struct hash *open_files = &process->open_files;
  struct file_fd *file_fd = malloc (sizeof(struct file_fd));
  if (file_fd == NULL || hash_size (open_files) > OPEN_FILE_LIMIT)
    return -1; /* File not found or too many files open. */

  file_fd->fd = fd;
  file_fd->file = file;
  hash_insert (open_files, &file_fd->elem);
  return fd;
}

/* Finds file_fd in open_files hash. */
static struct file_fd*
process_find_file (int fd)
{
  struct hash *open_files = &process_current ()->open_files;
  struct file_fd file_fd;
  file_fd.fd = fd;
  struct hash_elem *elem = hash_find (open_files, &file_fd.elem);
  if (elem == NULL) /* File not Found. */
    {
      return NULL;
    }
  return hash_entry (elem, struct file_fd, elem);
}

/* Gets file from open_files hash. */
struct file*
process_fetch_file (int fd)
{
  struct file_fd* file_fd = process_find_file (fd);
  return file_fd == NULL ? NULL : file_fd->file;
}

/* Removes and returns file from open_file hash. */
struct file*
process_remove_file (int fd)
{
  struct hash *open_files = &process_current ()->open_files;
  struct file_fd *file_fd = process_find_file (fd);
  if (file_fd == NULL) /* File was not found. */
    return NULL;
  hash_delete (open_files, &file_fd->elem);

  struct file *file = file_fd->file;
  free (file_fd);
  return file;
}

/* Hashes open files by fd. */
static unsigned
fd_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  return (unsigned) hash_entry (e, struct file_fd, elem)->fd;
}

/* Compares two files by fd. */
static bool
fd_less_func (const struct hash_elem *a,
              const struct hash_elem *b,
              void *aux UNUSED)
{
  return hash_entry (a, struct file_fd, elem)->fd
         < hash_entry (b, struct file_fd, elem)->fd;
}

/* Destroy function for the open_files hash. Frees file_fd struct
   and closes associated file. */
static void
fd_hash_destroy (struct hash_elem *e, void *aux UNUSED)
{
  struct file_fd *file_fd = hash_entry (e, struct file_fd, elem);

  /* Allow writes on the process's file so other processes can write to it. */
  if (file_fd->fd == EXEC_FILE)
    {
      file_allow_write (file_fd->file);
    }

  file_close (file_fd->file);
  free (file_fd);
}

/* Decrements counter of persistent info object.
   When counter reaches 0, the object is freed. */
static void
process_persistent_info_counter_decrement (persistent_info *info)
{
  lock_acquire (&info->persistent_info_lock);
  int value = --info->reference_counter;
  lock_release (&info->persistent_info_lock);
  ASSERT (value >= NO_REFERENCES);
  if (value <= NO_REFERENCES)
    {
      free (info);
    }
}

/* Hashes persistent_info by pid. */
static unsigned
children_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  unsigned hash =
    (unsigned) hash_entry (e, persistent_info, persistent_elem)->pid;
  return hash;
}

/* Compares two persistent_info structs by pid. */
static bool
children_less_func (const struct hash_elem *a,
              const struct hash_elem *b,
              void *aux UNUSED)
{
  return hash_entry (a, persistent_info, persistent_elem)->pid
         < hash_entry (b, persistent_info, persistent_elem)->pid;
}

/* Destroys children hash elements by setting them free. */
static void
children_hash_destroy (struct hash_elem *e, void *aux UNUSED)
{
  persistent_info *pers_info =
    hash_entry (e, persistent_info, persistent_elem);
  /* Decrement counter of persistent info and free it if no references exist. */
  process_persistent_info_counter_decrement (pers_info);
}
