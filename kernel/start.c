#include <assert.h>
#include <stdio.h>
#include <x86.h>

#include <kernel/idt.h>
#include <kernel/keyboard.h>

int main() {
  return 1;
}

void kernel_start() {
  debug();
  init_idt();
  debug();
  init_keyboard();
  debug();
  // init_memory();
  //
  // init_pci();
  //
  // init_storage();
  //
  // printf("\nMeniOS is good to go\n");

  // mosh();
  printf("Looping\n");
  // while(true) {
  //   io_wait();
  // };
}
