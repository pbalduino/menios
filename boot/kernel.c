#include <assert.h>
#include <stdio.h>
// #include <stdlib.h>
#include <string.h>
#include <kernel/idt.h>
#include <kernel/keyboard.h>
#include <kernel/mosh.h>
#include <kernel/pci.h>
#include <kernel/pic.h>
#include <kernel/pmap.h>
#include <kernel/storage.h>

int main() {
  return 1;
}

void kernel_start() {
  init_idt();

  init_memory();

  init_pci();

  init_storage();

  init_keyboard();

  asm volatile("sti\n");

  printf("\nMeniOS is good to go\n");

  mosh();
}
