#include <kernel/console.h>
#include <kernel/smp.h>
#include <arch.h>
#include <string.h>

#define CPUID_GETVENDORSTRING 0
#define CPUID_GETFEATURES     1
#define CPUID_FEAT_EDX_APIC   (1 << 9)
#define CPUID_FEAT_EDX_HTT    (1 << 28)


bool support_acpi;
bool support_multicore;
uint16_t cpu_count;
uint16_t core_count;

bool is_multicore() {
  uint32_t eaxp, ebxp, ecxp, edxp;
  cpuid(CPUID_GETFEATURES, &eaxp, &ebxp, &ecxp, &edxp);

  if(edxp & CPUID_FEAT_EDX_HTT) {
    cpu_count = (ebxp >> 16) & 0xff;

    cpuid(4, &eaxp, &ebxp, &ecxp, &edxp);
    core_count = ((ebxp >> 26) & 0x3f) + 1;

    printf("* HTT supported. %d logical CPU(s) found. %d core(s) found.\n", cpu_count, core_count);
  } else {
    printf("* No HTT supported.\n");
    cpu_count = 1;
  }

  return edxp & CPUID_FEAT_EDX_HTT;
}

bool can_i_use_acpi() {
  uint32_t eaxp, ebxp, ecxp, edxp;
  cpuid(CPUID_GETFEATURES, &eaxp, &ebxp, &ecxp, &edxp);
  return edxp & CPUID_FEAT_EDX_APIC;
}

int detect_cpu(void) { /* or main() if your trying to port this as an independant application */
	uint32_t ebx, unused;
	cpuid(CPUID_GETVENDORSTRING, &unused, &ebx, &unused, &unused);
	switch(ebx) {
		case 0x756e6547:
  		// do_intel();
      printf("* Intel chipset detected\n");
  		break;
		case 0x68747541:
  		// do_amd();
      printf("* AMD chipset detected\n");
  		break;
		default:
  		printf("Unknown x86 chipset detected\n");
  		break;
	}
	return 0;
}

void init_smp() {
  support_acpi = can_i_use_acpi();
  support_multicore = is_multicore();

  detect_cpu();

  if(support_acpi) {
    printf("* APIC detected\n");
  } else {
    printf("* APIC not found\n");
  }

  printf("* Enabling SMP\n");
}
