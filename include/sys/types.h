#ifndef MENIOS_INCLUDE_SYS_TYPES_H
#define MENIOS_INCLUDE_SYS_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef long long int useconds_t;
typedef long long int time_t;
typedef int pid_t;
typedef int key_t;
typedef long long off_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int mode_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned long nlink_t;
typedef long blksize_t;
typedef long long blkcnt_t;

#define MENIOS_DEV_MAJOR_BITS 12u
#define MENIOS_DEV_MINOR_BITS 20u
#define MENIOS_DEV_MAJOR_MASK ((dev_t)((1ull << MENIOS_DEV_MAJOR_BITS) - 1ull))
#define MENIOS_DEV_MINOR_MASK ((dev_t)((1ull << MENIOS_DEV_MINOR_BITS) - 1ull))

#define MKDEV(ma, mi) \
  ((dev_t)(((((dev_t)(ma)) & MENIOS_DEV_MAJOR_MASK) << MENIOS_DEV_MINOR_BITS) | \
           (((dev_t)(mi)) & MENIOS_DEV_MINOR_MASK)))

#define MAJOR(dev) ((unsigned int)((((dev_t)(dev)) >> MENIOS_DEV_MINOR_BITS) & MENIOS_DEV_MAJOR_MASK))
#define MINOR(dev) ((unsigned int)(((dev_t)(dev)) & MENIOS_DEV_MINOR_MASK))

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_SYS_TYPES_H
