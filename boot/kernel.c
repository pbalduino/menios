#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/idt.h>
#include <kernel/keyboard.h>
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

  printf("\n");

  printf("MeniOS is good to go\n\n#");

  printf("Crash?\n");

  // stack_overflow()
  // int a = 0;
  // int b = 42;
  //
  // b = b / a;
  //
  // printf("hmmmm %d\n", b);
  // asm volatile("sti");

  // while(1) { }

  asm volatile("int $0x80"
               :
               : "a" (0x1234), "b" (0x5678), "c" (0x2468), "d" (0x1357));
    // int - print
    //   0 -  0
    //   6 -  6
    //  13 - 13
    //  80 - 13
  //   printf(".");
  // }
}
