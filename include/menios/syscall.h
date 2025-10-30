#ifndef MENIOS_INCLUDE_MENIOS_SYSCALL_H
#define MENIOS_INCLUDE_MENIOS_SYSCALL_H

#ifdef __cplusplus
extern "C" {
#endif

// System call numbers shared between kernel and userland.
#define SYS_READ        0
#define SYS_WRITE       1
#define SYS_OPEN        2
#define SYS_CLOSE       3
#define SYS_LSEEK       8
#define SYS_MMAP        9
#define SYS_MUNMAP     11
#define SYS_PIPE       22
#define SYS_YIELD      24
#define SYS_SLEEP      35
#define SYS_GETPID     39
#define SYS_DUP        32
#define SYS_DUP2       33
#define SYS_FORK       57
#define SYS_EXECVE     59
#define SYS_EXIT       60
#define SYS_WAITPID    61
#define SYS_LISTDIR    62
#define SYS_STDIN_POLL 63
#define SYS_PROC_KILL  64
#define SYS_PROC_LIST  65
#define SYS_KILL       66
#define SYS_SIGACTION  67
#define SYS_SIGPROCMASK 68
#define SYS_GETSOCKOPT 69
#define SYS_FCNTL      72
#define SYS_IOCTL      73
#define SYS_SHMGET     74
#define SYS_SHMAT      75
#define SYS_SHMDT      76
#define SYS_SHMCTL     77
#define SYS_CHDIR      78
#define SYS_GETCWD     79
#define SYS_GETPAGESIZE 80
#define SYS_TIME        81
#define SYS_GETTIMEOFDAY 82
#define SYS_NANOSLEEP   83
#define SYS_CLOCK_GETTIME 84
#define SYS_CLOCK_SETTIME 85
#define SYS_CLOCK_GETRES  86
#define SYS_SETITIMER     87
#define SYS_GETITIMER     88
#define SYS_ALARM         89
#define SYS_SIGRETURN     90
#define SYS_INPUT_EVENT   91
#define SYS_UNLINK        92
#define SYS_MKDIR         93
#define SYS_RMDIR         94
#define SYS_RENAME        95
#define SYS_SIGPENDING    96
#define SYS_SIGWAITINFO   97
#define SYS_SIGSUSPEND    98
#define SYS_STAT           99
#define SYS_FSTAT         100
#define SYS_LSTAT         101
#define SYS_CHMOD         102
#define SYS_FCHMOD        103
#define SYS_UTIME         104
#define SYS_SHUTDOWN      105

#ifdef __cplusplus
}
#endif

#endif
