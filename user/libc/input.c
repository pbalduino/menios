#include <errno.h>
#include <menios/input.h>
#include <menios/syscall.h>
#include <menios/syscall_user.h>

int menios_input_poll(menios_key_event_t* event) {
  if(event == NULL) {
    errno = EINVAL;
    return -1;
  }

  long rc = __menios_syscall1(SYS_INPUT_EVENT, (long)event);
  if(rc < 0) {
    errno = (int)(-rc);
    return -1;
  }
  return 0;
}
