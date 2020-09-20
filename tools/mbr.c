#ifndef __GNUC__
  #error Check your compiler to see how to keep a struct packed
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef struct {
  // 0x80 is active
  uint8_t status;
  uint8_t st_head;
  uint8_t st_sector;
  uint8_t st_cylinder;
  uint8_t partition_type;
  uint8_t end_head;
  uint8_t end_sector;
  uint8_t end_cylinder;
  uint32_t lba_st_sector;
  uint32_t lba_count_sector;
} __attribute__((packed)) pte_t;

typedef struct {
  uint8_t jump_code[3];
  uint8_t oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t fat_copies;
  uint16_t max_root_dir_entries;
  uint16_t small_sectors;
  uint8_t media_descriptor;
  uint16_t old_sectors_per_fat;
  uint16_t sectors_per_track;
  uint16_t heads;
  uint32_t hidden_sectors_in_partition;
  uint32_t sectors_in_partition;
  uint32_t sectors_per_fat;
  uint16_t flags;
  uint16_t fat32_version;
  uint32_t root_cluster_no;
  uint16_t fs_info_sector;
  uint16_t fs_backup_sector;
  uint8_t reserved[12];
  uint8_t log_drive_no_partition;
  uint8_t unused;
  uint8_t extended_signature;
  uint32_t serial_number;
  uint8_t volume_name[11];
  uint8_t fat_name[8];
  uint8_t executable_code[420];
  uint8_t signature[2];
} __attribute__((packed)) fat32_bri_t;

typedef struct {
  // MBR Bootstrap (flat binary executable code)
  uint8_t code[440];
  // Optional "Unique Disk ID / Signature"
  uint8_t	disk_id[4];
  // Optional, reserved 0x0000
  uint16_t optional;
  // Partition table entries for four partitions
  pte_t partition[4];
  // if it's 55aa the drive contains a bootloader
  uint16_t magic_number;
} mbr_t;

void fheader(mbr_t* mbr, fat32_bri_t* fat32) {
  if(fat32) { };

  printf("part_01: %s, status: %2xh, h: %2xh, s: %2xh, c: %2xh, pt: %2xh, eh: %xh, es: %xh, ec: %xh, lbas: %xh, lbcs: %xh, mag: %x\n",
    mbr->disk_id,
    mbr->partition[0].status,
    mbr->partition[0].st_head,
    mbr->partition[0].st_sector,
    mbr->partition[0].st_cylinder,
    mbr->partition[0].partition_type,
    mbr->partition[0].end_head,
    mbr->partition[0].end_sector,
    mbr->partition[0].end_cylinder,
    mbr->partition[0].lba_st_sector * 512,
    mbr->partition[0].lba_count_sector,
    mbr->magic_number);

  printf("part_02: %s, status: %2xh, h: %2xh, s: %2xh, c: %2xh, pt: %2xh, eh: %xh, es: %xh, ec: %xh, lbas: %xh, lbcs: %xh, mag: %x\n",
    mbr->disk_id,
    mbr->partition[1].status,
    mbr->partition[1].st_head,
    mbr->partition[1].st_sector,
    mbr->partition[1].st_cylinder,
    mbr->partition[1].partition_type,
    mbr->partition[1].end_head,
    mbr->partition[1].end_sector,
    mbr->partition[1].end_cylinder,
    mbr->partition[1].lba_st_sector,
    mbr->partition[1].lba_count_sector,
    mbr->magic_number);
}

void fdisk(FILE* disk, mbr_t* mbr) {
  fseek(disk, 0, SEEK_SET);
  mbr->partition[0].status = 0x80;
  mbr->partition[0].st_head = 0x00;
  mbr->partition[0].st_sector = 0x01;
  mbr->partition[0].st_cylinder = 0x01;
  mbr->partition[0].partition_type = 0x0c;
  mbr->partition[0].end_head = 0xfe;
  mbr->partition[0].end_sector = 0x3f;
  mbr->partition[0].end_cylinder = 0x13;
  mbr->partition[0].lba_st_sector = 0x3ec1;
  mbr->partition[0].lba_count_sector = 0xffac53;
  fwrite(mbr, 512, 1, disk);
}

void show_hex(uint8_t* sector) {
  for(int l = 0; l < 16; l++) {
    printf("0x%04x: ", (l * 32));
    for(int c = 0; c < 32; c++) {
      if(c > 0 && (c % 8) == 0) {
        printf(" ");
      }
      printf("%02x ", sector[(l * 32) + c]);
    }
    printf("   ");
    for(int c = 0; c < 32; c++) {
      int ch = sector[(l * 32) + c];
      if(c > 0 && (c % 8) == 0) {
        printf(" ");
      }
      if(ch >= ' ' && ch <= 127) {
        printf("%c", ch);
      } else {
        printf(".");
      }
    }
    printf("\n");
  }
}

int main(int argc, char *argv[]) {
  printf("sz: %lu - %lu - %lu\n", sizeof(pte_t), sizeof(mbr_t), sizeof(fat32_bri_t));
  assert(argc > 1);
  assert(sizeof(mbr_t) == 512);
  assert(sizeof(fat32_bri_t) == 512);

  FILE* disk = fopen(argv[1], "r+");
  uint8_t buffer[512];

  fseek(disk, 0, SEEK_SET);
  fread(buffer, 512, 1, disk);

  mbr_t* mbr = (mbr_t*)buffer;
  printf("MBR\n");
  show_hex(buffer);

  uint32_t start = (mbr->partition[0].lba_st_sector * 512);
  // 512 bytes per sector 0x00 0x02
  // printf("Writing @ %u\n", start);
  fseek(disk, start, SEEK_SET);

  fread(buffer, 512, 1, disk);
  fat32_bri_t* fat32 = (fat32_bri_t*)buffer;

  printf("FAT32\n");
  show_hex(buffer);

  for(int s = 1; s < 9; s++) {

  }

  fseek(disk, start + (fat32->sectors_per_cluster * fat32->root_cluster_no * 512), SEEK_SET);
  fread(buffer, 512, 1, disk);
  printf("Root?\n");
  show_hex(buffer);

  printf("vol name: %s\n", fat32->volume_name);
  printf("fat name: %s\n", fat32->fat_name);
  printf("root cl : %x\n", fat32->root_cluster_no);
  printf("sec p cl: %x\n", fat32->sectors_per_cluster);
  printf("fat sig : %x%x\n", fat32->signature[0], fat32->signature[1]);

  if(argc > 2) {
    if(!strcmp(argv[2], "fdisk")) {
      printf("Partitioning disk %s\n", argv[2]);
      fdisk(disk, mbr);
    } else if(!strcmp(argv[2], "ftell")) {
      printf("Disk header from %s - %x %x\n", argv[2], mbr->partition[0].lba_st_sector, mbr->partition[1].lba_st_sector);
      fheader(mbr, fat32);
    } else {
      printf("Don't know %s\n", argv[2]);
    }
  }

  fclose(disk);

  return 0;
}
