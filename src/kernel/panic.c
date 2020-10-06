#include <kernel/console.h>
#include <stdarg.h>

const char *panicstr;

// TODO change when the OS support SMP
int cpunum() {
  return 0;
}

void _panic(const char *file, int line, const char *fmt, ...) {
	va_list ap;

	if (panicstr)
		goto dead;

	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	asm volatile("cli; cld");

	va_start(ap, fmt);
	printf("!!!!!!\n");
	printf("! =( ! kernel panic on CPU %d at %s:%d: \n", cpunum(), file, line);
	printf("!!!!!! ");
	vprintf(fmt, ap);
	printf("\n!!!!!!\n");
	va_end(ap);

	dead:
	/* break into the kernel monitor */
	while (1);
	// 	monitor(NULL);
}

void _warn(const char* file, int line, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	printf("kernel warning at %s:%d: ", file, line);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
}
