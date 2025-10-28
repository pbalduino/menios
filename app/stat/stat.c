#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

static const char* file_type_string(mode_t mode) {
  if(S_ISREG(mode)) {
    return "regular file";
  }
  if(S_ISDIR(mode)) {
    return "directory";
  }
  if(S_ISCHR(mode)) {
    return "character device";
  }
  if(S_ISBLK(mode)) {
    return "block device";
  }
  if(S_ISFIFO(mode)) {
    return "fifo";
  }
  if(S_ISSOCK(mode)) {
    return "socket";
  }
  if(S_ISLNK(mode)) {
    return "symlink";
  }
  return "unknown";
}

static char file_type_char(mode_t mode) {
  if(S_ISREG(mode)) {
    return '-';
  }
  if(S_ISDIR(mode)) {
    return 'd';
  }
  if(S_ISCHR(mode)) {
    return 'c';
  }
  if(S_ISBLK(mode)) {
    return 'b';
  }
  if(S_ISFIFO(mode)) {
    return 'p';
  }
  if(S_ISSOCK(mode)) {
    return 's';
  }
  if(S_ISLNK(mode)) {
    return 'l';
  }
  return '?';
}

static void format_mode(mode_t mode, char* out, size_t len) {
  if(len < 11) {
    return;
  }

  out[0] = file_type_char(mode);

  const mode_t permissions[9] = {
    S_IRUSR, S_IWUSR, S_IXUSR,
    S_IRGRP, S_IWGRP, S_IXGRP,
    S_IROTH, S_IWOTH, S_IXOTH,
  };
  const char symbols[9] = {
    'r', 'w', 'x',
    'r', 'w', 'x',
    'r', 'w', 'x',
  };

  for(size_t i = 0; i < 9; i++) {
    bool set = (mode & permissions[i]) != 0;
    out[i + 1] = set ? symbols[i] : '-';
  }

  if((mode & S_ISUID) != 0) {
    out[3] = (mode & S_IXUSR) ? 's' : 'S';
  }
  if((mode & S_ISGID) != 0) {
    out[6] = (mode & S_IXGRP) ? 's' : 'S';
  }
  if((mode & S_ISVTX) != 0) {
    out[9] = (mode & S_IXOTH) ? 't' : 'T';
  }

  out[10] = '\0';
}

static void format_time(time_t value, char* out, size_t len) {
  if(len == 0) {
    return;
  }

  struct tm tm_buf;
  if(localtime_r(&value, &tm_buf) == NULL) {
    snprintf(out, len, "%lld", (long long)value);
    return;
  }

  if(strftime(out, len, "%Y-%m-%d %H:%M:%S %z", &tm_buf) == 0) {
    snprintf(out, len, "%lld", (long long)value);
  }
}

static const char* bool_word(bool value) {
  return value ? "yes" : "no";
}

static int print_stat_for_path(const char* path) {
  struct stat st;
  if(path == NULL || *path == '\0') {
    fprintf(stderr, "stat: invalid path\n");
    return 1;
  }

  if(stat(path, &st) != 0) {
    int err = errno;
    fprintf(stderr, "stat: cannot stat '%s': ", path);
    errno = err;
    perror(NULL);
    return 1;
  }

  char mode_buffer[11] = {0};
  format_mode(st.st_mode, mode_buffer, sizeof(mode_buffer));

  char atime_buffer[64];
  char mtime_buffer[64];
  char ctime_buffer[64];
  format_time(st.st_atime, atime_buffer, sizeof(atime_buffer));
  format_time(st.st_mtime, mtime_buffer, sizeof(mtime_buffer));
  format_time(st.st_ctime, ctime_buffer, sizeof(ctime_buffer));

  printf("  File: '%s'\n", path);
  printf("  Size: %-11lld Blocks: %-11lld IO Block: %-6ld %s\n",
         (long long)st.st_size,
         (long long)st.st_blocks,
         (long)st.st_blksize,
         file_type_string(st.st_mode));
  printf("Device: 0x%lx/%lu   Inode: %-10lu  Links: %lu\n",
         (unsigned long)st.st_dev,
         (unsigned long)st.st_dev,
         (unsigned long)st.st_ino,
         (unsigned long)st.st_nlink);
  printf("Access: (%04o/%s)  Uid: %5u  Gid: %5u\n",
         (unsigned int)(st.st_mode & 07777),
         mode_buffer,
         (unsigned int)st.st_uid,
         (unsigned int)st.st_gid);
  if(S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
    printf("Device type: %lu\n", (unsigned long)st.st_rdev);
  }
  printf("Access: %s\n", atime_buffer);
  printf("Modify: %s\n", mtime_buffer);
  printf("Change: %s\n", ctime_buffer);
  printf(" Birth: -\n");

  if((st.st_menios_flags & ST_MENIOS_FLAG_HAS_DOS_ATTRS) != 0) {
    bool hidden = (st.st_menios_flags & ST_MENIOS_FLAG_DOS_HIDDEN) != 0;
    bool system = (st.st_menios_flags & ST_MENIOS_FLAG_DOS_SYSTEM) != 0;
    bool archived = (st.st_menios_flags & ST_MENIOS_FLAG_DOS_ARCHIVED) != 0;
    printf("  DOS: attrs=0x%02x hidden=%s system=%s archived=%s\n",
           st.st_menios_dos_attributes,
           bool_word(hidden),
           bool_word(system),
           bool_word(archived));
  } else {
    printf("  DOS: attrs unavailable\n");
  }

  printf("\n");
  return 0;
}

int main(int argc, char** argv) {
  if(argc < 2) {
    fprintf(stderr, "stat: missing operand\n");
    return 1;
  }

  int exit_code = 0;
  for(int i = 1; i < argc; i++) {
    if(print_stat_for_path(argv[i]) != 0) {
      exit_code = 1;
    }
  }
  return exit_code;
}
