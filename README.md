# MeniOS

![image](https://github.com/pbalduino/menios/assets/32979/ea081272-ecb0-44ba-9b50-bf73e5052cc0)

I'm trying again again. Let's see how far I can go.

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

[X] Integration with Limine

[X] Map physical memory

[ ] Request a page from physical memory

[ ] Implement a malloc to provide virtual memory to the process

Reference
  - Intel® 64 and IA-32 Architectures Software Developer’s Manual Combined Volumes: 1, 2A, 2B, 2C, 2D, 3A, 3B, 3C, 3D, and 4: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
  - PIC:  https://pdos.csail.mit.edu/6.828/2014/readings/hardware/8259A.pdf
          http://www.brokenthorn.com/Resources/OSDevPic.html
  - APIC: http://web.archive.org/web/20070112195752/http://developer.intel.com/design/pentium/datashts/24201606.pdf
  - ATA:  http://learnitonweb.com/2020/05/22/12-developing-an-operating-system-tutorial-episode-6-ata-pio-driver-osdev/
          http://www.t13.org/Documents/UploadedDocuments/docs2016/di529r14-ATAATAPI_Command_Set_-_4.pdf p.74
  - ASM:  https://bitismyth.wordpress.com/assembly-bunker/
  - Mem:  https://arjunsreedharan.org/post/148675821737/memory-allocators-101-write-a-simple-memory
  - AMD:  https://developer.amd.com/resources/developer-guides-manuals/
          https://www.amd.com/system/files/TechDocs/48751_16h_bkdg.pdf
  - Limine 8.x: https://github.com/limine-bootloader/limine/blob/v8.x/PROTOCOL.md

![image](https://user-images.githubusercontent.com/32979/212723683-73387eaf-4a48-4193-83b6-5ec155360a50.png)
