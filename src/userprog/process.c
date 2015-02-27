#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/list.h>
#include <kernel/hash.h>
#include <user/syscall.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

/* The lock for the below hash table */
static struct lock process_info_lock;
/* Maps pids to process_infos. Also serves to keep track of all
   processes that exist. */
static struct hash process_info_table;

static process_info *process_execute_aux (const char *file_name);
static process_info *create_process_info (struct thread *inner_thread);
static thread_func start_process NO_RETURN;
static bool load (char *cmdline, void (**eip) (void), void **esp);

static unsigned process_info_hash_func (const struct hash_elem *e, void *aux);
static bool process_info_less_func (const struct hash_elem *a,
                                    const struct hash_elem *b,
                                    void *aux);

/* fd_file hash related funcitons */
static unsigned fd_hash_func (const struct hash_elem*, void*);
static bool fd_less_func (const struct hash_elem *a,
                          const struct hash_elem *b,
                          void *aux UNUSED);

/* Initializes the process_info system. */
void
process_info_init (void)
{
  lock_init (&process_info_lock);
  lock_acquire (&process_info_lock);
  hash_init (&process_info_table, process_info_hash_func,
             process_info_less_func, NULL);
  lock_release (&process_info_lock);
}

/* Struct for linking files to fds. */
struct file_fd
  {
    int fd;
    struct file *file;
    struct hash_elem elem;
  };


/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
  return process_execute_aux (file_name)->tid;
}

/* Same as process_execute, but returns a pid_t instead.
   This will be PID_ERROR if a thread could not be created.
   This pid can be used to access the process_info attached to
   the process. */
pid_t
process_execute_pid (const char *file_name)
{
  return process_execute_aux (file_name)->pid;
}

/* Performs the work of the process_execute functions, returning
   the process_info created. */
static process_info *
process_execute_aux (const char *file_name)
{
  char *fn_copy;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return create_process_info (NULL);
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  struct thread *thread =
    thread_create_return_t (file_name, PRI_DEFAULT, start_process, fn_copy);
  if (thread->tid == TID_ERROR)
    palloc_free_page (fn_copy);

  return create_process_info (thread);
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

/* Creates a process_info from a thread and add to the hash table.
   If a TID_ERROR is passed in, then the pid will be set to PID_ERROR,
   and the tid set to TID_ERROR. Also, other fields will not be
   initialized. The process_info will also not be added to the hash
   table. */
static process_info *
create_process_info (struct thread *inner_thread)
{
  process_info *info = calloc(1, sizeof *info);
  ASSERT(info != NULL);

  /* For now, the pid is the same as the tid.
     The following assignments are a bit unnecessary right now, but will allow
     for easier changing later on if pid and tid need to be different in the
     future (for example, for multiple threaded processes). */
  if (inner_thread == NULL)
    {
      info->pid = PID_ERROR;
      info->tid = TID_ERROR;
    }
  else
    {
      info->pid = inner_thread->tid == TID_ERROR ? PID_ERROR : inner_thread->tid;
      info->tid = inner_thread->tid;
      inner_thread->info = info;
    }

  lock_init (&info->children_lock);
  lock_acquire (&info->children_lock);
  list_init (&info->children);
  lock_release (&info->children_lock);

  lock_acquire (&process_info_lock);
  hash_insert (&process_info_table, &info->process_elem);
  info->fd_counter = 2; /* 0 and 1 are reserved for stdin and stdout. */
  hash_init (&info->open_files, fd_hash_func, fd_less_func, NULL);
  lock_release (&process_info_lock);

  return info;
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED)
{
  /* return -1; */
  for(;;); /* Infinite loop DIRTY HAXX */
}

/* Free the current process's resources. */
void
process_exit (void)
{
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
static void write_args_to_stack (void **esp, const char *args, int arg_length);

const char* delimiters = " \n\t\0";

/* Size limit in bytes for command line arguments.
  Equals about half page size. */
static int arg_size_limit = 2048;

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

  if (arg_length > arg_size_limit)
  {
    printf("Warning: command line arguments exceed "
           "argument size limit. Errors will occur.\n");
    arg_length = arg_size_limit;
  }

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", file_name);
      goto done;
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
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
              if ((phdr.p_flags & PF_X) != 0)
                { /* Deny write to newly opened executables. */
                  file_deny_write (file);
                }
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
  write_args_to_stack (esp, file_name, arg_length);

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not.
     We also reallow writes to the files again. */
  file_close (file);
  file_allow_write (file);
  return success;
}

/* Writes arguments to stack according to the calling convention.
   Note that the caller already guarantees that the argv_string
   actually contains arguments. */
static void
write_args_to_stack (void **esp, const char *argv_string, int arg_length)
{
  ASSERT (argv_string != NULL && arg_length > 0);

  int read = arg_length ? arg_length - 1 : arg_length;
  char *esp_char = *esp;
  /* arg_length/2+1 is the maximum possible number of distincts args in string.*/
  uint32_t argv[arg_length/2 + 1];
  uint32_t argc = 0;

  while (read >= 0)
    {
      /* Skip delimiter characters. */
      while (read >= 0 && strchr (delimiters, argv_string[read]) != NULL)
          --read;
      /* Write characters from argv_string onto stack. */
      *--esp_char = '\0';
      while (read >= 0 && strchr (delimiters, argv_string[read]) == NULL)
          *--esp_char = argv_string[read--];
      /* Remember position of first character of the argument. */
      argv[argc++] = (uint32_t) esp_char;
    }

  /* Word-align esp address. */
  uint8_t *esp_fill = (uint8_t *) esp_char;
  while ((uint32_t) esp_fill % (uint32_t) 8 != 0)
    *--esp_fill = 0;

  /* Add pointer addresses to arguments. */
  uint32_t *esp_pointer = (uint32_t *) esp_fill;
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

  /* Test things */
/*
  uint32_t *test = (uint32_t *) *esp;
  char **arr = (char **) *(test+2);
  printf("%d, %d, %d, %s, %s, %s, %s\n", *test, *(test+1), arr[4] == NULL, arr[0], arr[1], arr[2], arr[3]);
*/
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
        *esp = PHYS_BASE;
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

/* Gets the process_info corresponding to a given pid_t. */
process_info *
process_get_info (pid_t pid)
{
  process_info info;
  info.pid = pid;

  struct hash_elem *e = hash_find (&process_info_table, &info.process_elem);
  return e != NULL ? hash_entry (e, process_info, process_elem) : NULL;
}

/* This hash_func simply returns the process_info's pid */
static unsigned
process_info_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  process_info *info = hash_entry (e, process_info, process_elem);
  return (unsigned)info->pid;
}

/* Returns whether a's pid is less than b's pid. */
static bool
process_info_less_func (const struct hash_elem *a,
                        const struct hash_elem *b,
                        void *aux UNUSED)
{
  return process_info_hash_func (a, NULL) <
    process_info_hash_func (b, NULL);
}

/* Adds file to open_files hash. Returns the fd it generates. */
int
process_add_file (struct file *file)
{
  ASSERT (false); // UNIMPLEMENTED!
  int fd = 0; // current_process ()->fd_counter++;
  struct file_fd *file_fd = malloc (sizeof(struct file_fd));
  if (file_fd == NULL) /* File not found. */
    return -1;

  file_fd->fd = fd;
  file_fd->file = file;
  struct hash *open_files = NULL; // current->process ()->open_files;
  hash_insert (open_files, &file_fd->elem);
  return fd;
}

/* Finds file_fd in open_files hash. */
static struct file_fd*
process_find_file_fd (int fd)
{
  ASSERT (false); // UNIMPLEMENTED!
  struct hash *open_files = NULL; // current->process ()->open_files;
  struct file_fd file_fd;
  file_fd.fd = fd;
  struct hash_elem *elem = hash_find (open_files, &file_fd.elem);
  if (elem == NULL) /* File not Found. */
    return NULL;
  return hash_entry (elem, struct file_fd, elem);
}

/* Gets file from open_files hash. */
struct file*
process_fetch_file (int fd)
{
  return process_find_file_fd (fd)->file;
}

/* Removes and returns file from open_file hash. */
struct file*
process_remove_file (int fd)
{
  ASSERT (false); // UNIMPLEMENTED
  struct hash *open_files = NULL; // current->process ()->open_files;
  struct file_fd *file_fd = process_find_file_fd (fd);
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
