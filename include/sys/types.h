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

#ifdef __cplusplus
}
#endif

#endif // MENIOS_INCLUDE_SYS_TYPES_H
