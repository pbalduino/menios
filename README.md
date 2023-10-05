# MeniOS

![image](https://github.com/pbalduino/menios/assets/32979/bea730e3-1cfa-40ea-85b8-c03206f5d0f0)

I'm trying again. Let's see how far I can go.

Prerequisites:
  Linux:
  - gcc
  - ld
  - make
  - qemu

  MacOS:
  - Docker
  - make
  - qemu

To run:
 - make build run

To do:

  [ ] Add support to gdb

  [x] Create a todo list

  [ ] Dev environment

  [ ] ASM 512b bootloader

  [ ] Load bootloader beyond 512b

  [ ] Enable A20 bit

  [ ] Set GDT

  [ ] Set IDT

  [ ] Set PIC

  [ ] Add support to keyboard

  [ ] Set 32bits

  [ ] Set Protected Mode

  [ ] Enable memory paging

  [ ] Enable virtual memory

  [ ] Add a memory manager

  [ ] Add a process manager

  [ ] Read PCI bus and detect hardware

  [ ] Identify one drive

  [ ] Identify more drives

  [ ] Read data from drive

  [ ] Write data to drive

  [?] Fix MBR

  [ ] Add a file system

  [ ] Load dummy code from disk and run

  [ ] Segregate code for kernel and userland

  [ ] Call kernel task from userland dummy code

  [ ] Simple shell

  [ ] Connect to network using E1000

  [ ] Add support to soundcard

  [ ] Set 64bits

  [ ] UEFI support

  [ ] Test on real machines

  [ ] Run gcc inside meniOS (muahahahaha)

Reference
  - PIC:  https://pdos.csail.mit.edu/6.828/2014/readings/hardware/8259A.pdf
          http://www.brokenthorn.com/Resources/OSDevPic.html
  - APIC: http://web.archive.org/web/20070112195752/http://developer.intel.com/design/pentium/datashts/24201606.pdf
  - ATA:  http://learnitonweb.com/2020/05/22/12-developing-an-operating-system-tutorial-episode-6-ata-pio-driver-osdev/
          http://www.t13.org/Documents/UploadedDocuments/docs2016/di529r14-ATAATAPI_Command_Set_-_4.pdf p.74
  - ASM:  https://bitismyth.wordpress.com/assembly-bunker/
  - Mem:  https://arjunsreedharan.org/post/148675821737/memory-allocators-101-write-a-simple-memory
  - AMD:  https://developer.amd.com/resources/developer-guides-manuals/
          https://www.amd.com/system/files/TechDocs/48751_16h_bkdg.pdf

![image](https://user-images.githubusercontent.com/32979/212723683-73387eaf-4a48-4193-83b6-5ec155360a50.png)
