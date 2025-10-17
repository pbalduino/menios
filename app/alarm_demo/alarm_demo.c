#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static void on_alarm(int signo) {
  (void)signo;
  const char* msg = "[alarm_demo] ding!\n";
  write(STDOUT_FILENO, msg, 19);
}

int main(void) {
  if(signal(SIGALRM, on_alarm) == SIG_ERR) {
    perror("signal");
    return 1;
  }

  unsigned int leftover = alarm(3);
  printf("[alarm_demo] armed 3s alarm (previous=%u)\n", leftover);

  pause();

  printf("[alarm_demo] alarm delivered\n");
  return 0;
}
