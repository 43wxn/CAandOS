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
  if (fd < 0) panic("loader: open %s failed", filename);

  fs_read(fd, &ehdr, sizeof(ehdr));

  // 🔥【致命修复】必须检查 ELF 魔数！
  if (*(uint32_t *)ehdr.e_ident != 0x464c457f) {
    panic("Not an ELF file");
  }

  fs_lseek(fd, ehdr.e_phoff, SEEK_SET);
  Elf_Phdr phdr[ehdr.e_phnum];
  fs_read(fd, phdr, sizeof(phdr[0]) * ehdr.e_phnum);

  for (int i = 0; i < ehdr.e_phnum; i++) {
    if (phdr[i].p_type != PT_LOAD) continue;

    // 🔥【终极修复】只加载有数据的部分，不要乱 memset！
    // 原来的 memset 把程序入口代码覆盖成 0 → 跳去 0x0 崩溃！
    void *va = (void *)phdr[i].p_vaddr;
    fs_lseek(fd, phdr[i].p_offset, SEEK_SET);
    fs_read(fd, va, phdr[i].p_filesz);
  }

  fs_close(fd);
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", (void *)entry);
  ((void(*)())entry)();
}