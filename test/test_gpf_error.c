#include <unity.h>
#include <kernel/arch/x86_64/idt.h>

void setUp(void) {}
void tearDown(void) {}

static void assert_decode(uint64_t code,
                          bool external,
                          bool has_selector,
                          idt_gpf_table_t table,
                          uint16_t index) {
    idt_gpf_error_info_t info;
    idt_decode_gpf(code, &info);
    TEST_ASSERT_EQUAL(external, info.external);
    TEST_ASSERT_EQUAL(has_selector, info.has_selector);
    TEST_ASSERT_EQUAL(table, info.table);
    TEST_ASSERT_EQUAL(index, info.descriptor_index);
}

void test_decode_gpf_zero(void) {
    assert_decode(0, false, false, IDT_GPF_TABLE_GDT, 0);
}

void test_decode_gpf_external_idt_index(void) {
    uint64_t code = (1ull << 0) | (1ull << 1) | (0x55ull << 3);
    assert_decode(code, true, true, IDT_GPF_TABLE_IDT, 0x55);
}

void test_decode_gpf_ldt(void) {
    uint64_t code = (2ull << 1) | (0x1ffull << 3);
    assert_decode(code, false, true, IDT_GPF_TABLE_LDT, 0x1ff);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_decode_gpf_zero);
    RUN_TEST(test_decode_gpf_external_idt_index);
    RUN_TEST(test_decode_gpf_ldt);
    return UNITY_END();
}
