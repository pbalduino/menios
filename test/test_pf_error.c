#include <unity.h>
#include <kernel/idt.h>

static void check_decode(uint64_t code,
                         bool present,
                         bool write,
                         bool user,
                         bool reserved,
                         bool instruction,
                         bool protection,
                         bool shadow,
                         bool sgx) {
    idt_pf_error_info_t info;
    idt_decode_page_fault(code, &info);
    TEST_ASSERT_EQUAL(present, info.present);
    TEST_ASSERT_EQUAL(write, info.write);
    TEST_ASSERT_EQUAL(user, info.user);
    TEST_ASSERT_EQUAL(reserved, info.reserved);
    TEST_ASSERT_EQUAL(instruction, info.instruction_fetch);
    TEST_ASSERT_EQUAL(protection, info.protection_key);
    TEST_ASSERT_EQUAL(shadow, info.shadow_stack);
    TEST_ASSERT_EQUAL(sgx, info.sgx_violation);
}

void setUp(void) {}
void tearDown(void) {}

void test_decode_zero(void) {
    check_decode(0, false, false, false, false, false, false, false, false);
}

void test_decode_all_bits(void) {
    check_decode(0xFF,
                 true,  /* present */
                 true,  /* write   */
                 true,  /* user    */
                 true,  /* reserved*/
                 true,  /* instruction */
                 true,  /* protection key */
                 true,  /* shadow stack */
                 true); /* sgx */
}

void test_decode_mixed(void) {
    check_decode((1ull << 2) | (1ull << 4) | (1ull << 6),
                 false,
                 false,
                 true,
                 false,
                 true,
                 false,
                 true,
                 false);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_decode_zero);
    RUN_TEST(test_decode_all_bits);
    RUN_TEST(test_decode_mixed);
    return UNITY_END();
}
