#include <assert.h>
#include <stdio.h>
// #include <stdlib.h>
#include <string.h>
#include <kernel/idt.h>
#include <kernel/keyboard.h>
#include <kernel/mosh.h>
#include <kernel/pci.h>
// #include <kernel/pic.h>
#include <kernel/pmap.h>
#include <kernel/storage.h>

void init_apic();

int main() {
  return 1;
}

void kernel_start() {
  init_apic();
  // init_idt();

  // init_keyboard();

  // init_memory();
  //
  // init_pci();
  //
  // init_storage();
  //
  // printf("\nMeniOS is good to go\n");

  // mosh();
  while(true) {};
}
