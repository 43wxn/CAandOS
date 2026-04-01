#include <proc.h>
#include <elf.h>
#include <string.h>
#include <fs.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

static uintptr_t loader(PCB *pcb, const char *filename) {
  Elf_Ehdr ehdr;
  int fd = fs_open(filename, 0, 0);
  fs_read(fd, &ehdr, sizeof(ehdr));

  Elf_Phdr phdr[ehdr.e_phnum];
  fs_lseek(fd, ehdr.e_phoff, SEEK_SET);
  fs_read(fd, phdr, sizeof(phdr[0])*ehdr.e_phnum);

  for(int i=0; i<ehdr.e_phnum; i++){
    if(phdr[i].p_type != PT_LOAD) continue;
    fs_lseek(fd, phdr[i].p_offset, SEEK_SET);
    fs_read(fd, (void*)phdr[i].p_vaddr, phdr[i].p_filesz);
  }
  fs_close(fd);
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  ((void(*)())entry)();
}