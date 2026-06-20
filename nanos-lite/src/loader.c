#include <proc.h>
#include <elf.h>
#include <string.h>
#include <fs.h>
#include <memory.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

/*
 * Arguments are written to a fixed page near the end of physical memory
 * (0x90000000 is end of 256MB NEMU RAM).  The C runtime (crt0) reads
 * from this address to obtain argc / argv.
 */
#define USER_ARGS_ADDR  0x8FFF0000
#define USER_ARGS_MAX   1024

static void write_user_args(int argc, char *const argv[]) {
  uint32_t *base = (uint32_t *)USER_ARGS_ADDR;
  char *dst = (char *)(base + 1);  /* strings go right after argc */

  /* argc */
  *base = (uint32_t)argc;

  /* Concatenate argv strings with \0 separators */
  if (argc > 0 && argv != NULL) {
    for (int i = 0; i < argc && dst < (char *)USER_ARGS_ADDR + USER_ARGS_MAX; i++) {
      if (argv[i] == NULL) break;
      size_t len = strlen(argv[i]);
      if (len > (size_t)((char *)USER_ARGS_ADDR + USER_ARGS_MAX - dst - 1))
        len = (size_t)((char *)USER_ARGS_ADDR + USER_ARGS_MAX - dst - 1);
      memcpy(dst, argv[i], len);
      dst += len;
      *dst++ = '\0';
    }
  }

  Log("write_user_args: argc=%d base=%p", argc, (void *)base);
}

static uintptr_t loader(PCB *pcb, const char *filename) {
  (void)pcb;
  Elf_Ehdr ehdr;
  int fd = fs_open(filename, 0, 0);
  if (fd < 0) {
    Log("loader: file not found: %s", filename);
    return 0;
  }

  fs_read(fd, &ehdr, sizeof(ehdr));
  fs_lseek(fd, ehdr.e_phoff, SEEK_SET);

  Elf_Phdr phdr[ehdr.e_phnum];
  fs_read(fd, phdr, ehdr.e_phnum * sizeof(Elf_Phdr));

  uintptr_t max_vaddr_end = 0;

  for (int i = 0; i < ehdr.e_phnum; i++) {
    if (phdr[i].p_type != PT_LOAD) continue;

    fs_lseek(fd, phdr[i].p_offset, SEEK_SET);
    fs_read(fd, (void *)phdr[i].p_vaddr, phdr[i].p_filesz);

    if (phdr[i].p_memsz > phdr[i].p_filesz) {
      memset((void *)(phdr[i].p_vaddr + phdr[i].p_filesz), 0,
             phdr[i].p_memsz - phdr[i].p_filesz);
    }

    uintptr_t seg_end = phdr[i].p_vaddr + phdr[i].p_memsz;
    if (seg_end > max_vaddr_end) max_vaddr_end = seg_end;
  }

  set_initial_brk((max_vaddr_end + PGSIZE - 1) & ~((uintptr_t)PGSIZE - 1));

  fs_close(fd);
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename, int argc, char *const argv[]) {
  Log("naive_uload: %s (argc=%d)", filename, argc);
  write_user_args(argc, argv);
  uintptr_t entry = loader(pcb, filename);
  if (entry == 0) {
    Log("naive_uload failed: %s", filename);
    return;
  }
  Log("Jump to entry = %p", (void *)entry);
  ((void (*)())entry)();
}
