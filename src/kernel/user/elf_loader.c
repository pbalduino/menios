#include <kernel/user/elf_loader.h>

#include <kernel/mem.h>
#include <kernel/pmm.h>
#include <kernel/serial.h>
#include <string.h>

#define ELF_MAGIC0 0x7f
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ELFOSABI_SYSV 0
#define ET_EXEC 2
#define EM_X86_64 62

#define PT_LOAD 1

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

typedef struct {
  unsigned char e_ident[16];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint64_t e_entry;
  uint64_t e_phoff;
  uint64_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
  uint32_t p_type;
  uint32_t p_flags;
  uint64_t p_offset;
  uint64_t p_vaddr;
  uint64_t p_paddr;
  uint64_t p_filesz;
  uint64_t p_memsz;
  uint64_t p_align;
} Elf64_Phdr;

static bool elf_validate(const Elf64_Ehdr* hdr, size_t size) {
  if(size < sizeof(Elf64_Ehdr)) {
    serial_printf("ELF: header too small\n");
    return false;
  }

  if(hdr->e_ident[0] != ELF_MAGIC0 ||
     hdr->e_ident[1] != ELF_MAGIC1 ||
     hdr->e_ident[2] != ELF_MAGIC2 ||
     hdr->e_ident[3] != ELF_MAGIC3) {
    serial_printf("ELF: invalid magic\n");
    return false;
  }

  if(hdr->e_ident[4] != ELFCLASS64 || hdr->e_ident[5] != ELFDATA2LSB) {
    serial_printf("ELF: unsupported class/data\n");
    return false;
  }

  if(hdr->e_type != ET_EXEC || hdr->e_machine != EM_X86_64 || hdr->e_version != EV_CURRENT) {
    serial_printf("ELF: unsupported type/machine/version\n");
    return false;
  }

  if(hdr->e_phoff == 0 || hdr->e_phentsize != sizeof(Elf64_Phdr)) {
    serial_printf("ELF: invalid program header table\n");
    return false;
  }

  if(hdr->e_phnum == 0) {
    serial_printf("ELF: no program headers\n");
    return false;
  }

  if(hdr->e_phoff + (hdr->e_phnum * sizeof(Elf64_Phdr)) > size) {
    serial_printf("ELF: program header table exceeds image size\n");
    return false;
  }

  return true;
}

static bool map_segment(proc_info_p proc, phys_addr_t root_phys, const uint8_t* image, size_t size, const Elf64_Phdr* phdr) {
  if(phdr->p_memsz == 0) {
    return true;
  }

  if(phdr->p_offset + phdr->p_filesz > size) {
    serial_printf("ELF: segment exceeds image size\n");
    return false;
  }

  virt_addr_t aligned_vaddr = phdr->p_vaddr & ~((virt_addr_t)PAGE_SIZE - 1);
  size_t offset_into_page = phdr->p_vaddr - aligned_vaddr;
  size_t total_size = offset_into_page + phdr->p_memsz;
  size_t total_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

  phys_addr_t phys = pmm_alloc_pages(total_pages);
  if(phys == 0) {
    serial_printf("ELF: unable to allocate physical pages for segment\n");
    return false;
  }

  if(!proc_register_user_segment(proc, phys, total_pages)) {
    serial_printf("ELF: too many user segments\n");
    pmm_free_pages(phys, total_pages);
    return false;
  }

  uint8_t* dest = (uint8_t*)physical_to_virtual(phys);
  memset(dest, 0, total_pages * PAGE_SIZE);
  memcpy(dest + offset_into_page, image + phdr->p_offset, phdr->p_filesz);

  bool writable = (phdr->p_flags & PF_W) != 0;
  for(size_t page = 0; page < total_pages; page++) {
    virt_addr_t page_vaddr = aligned_vaddr + (page * PAGE_SIZE);
    phys_addr_t page_phys = phys + (page * PAGE_SIZE);
    if(!pmm_map_page_in_root(root_phys, page_vaddr, page_phys, writable, true)) {
      serial_printf("ELF: failed to map page at %lx\n", page_vaddr);
      return false;
    }
  }

  return true;
}

bool elf64_load_image(proc_info_p proc, phys_addr_t root_phys, const void* image_ptr, size_t size, uint64_t* entry_out) {
  if(image_ptr == NULL || size == 0) {
    return false;
  }

  const uint8_t* image = (const uint8_t*)image_ptr;
  const Elf64_Ehdr* hdr = (const Elf64_Ehdr*)image;

  if(!elf_validate(hdr, size)) {
    return false;
  }

  const Elf64_Phdr* phdrs = (const Elf64_Phdr*)(image + hdr->e_phoff);

  for(uint16_t i = 0; i < hdr->e_phnum; i++) {
    const Elf64_Phdr* ph = &phdrs[i];
    if(ph->p_type != PT_LOAD) {
      continue;
    }

    if(!map_segment(proc, root_phys, image, size, ph)) {
      return false;
    }
  }

  if(entry_out) {
    *entry_out = hdr->e_entry;
  }

  return true;
}
