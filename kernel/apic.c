#include <stdio.h>
#include <types.h>

#define APIC_BASE 0xfee00000

#define APICREG_EOI      0x0b0
#define APICREG_SPURIOUS 0x0f0
#define APICREG_ICR0     0x300
#define APICREG_ICR1     0x310
#define APICREG_TIMER    0x320
#define APICREG_LINT0    0x350
#define APICREG_LINT1    0x360


uint32_t apic_read(uint32_t offset) {
  return *((volatile uint32_t*)(APIC_BASE + offset));
}

void apic_write(uint32_t offset, uint32_t data) {
  *((volatile uint32_t*)(APIC_BASE + offset)) = data;
}

void init_apic() {
  printf("Initing LAPIC\n");
  uint32_t val = apic_read(APICREG_SPURIOUS);
  val |= 0x100;
  apic_write(APICREG_SPURIOUS, val);
}
