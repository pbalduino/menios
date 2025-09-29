#include <kernel/input/keyboard.h>
#include <kernel/file.h>
#include <kernel/proc.h>
#include <unity.h>

static proc_info_t test_proc;
extern proc_info_p current;

void setUp(void) {
    current = &test_proc;
    test_proc.errno = 0;
    keyboard_device_reset();
}

void tearDown(void) {
    current = NULL;
}

static void test_keyboard_device_delivers_events(void) {
    keyboard_event_t event = {
        .timestamp_ns = 1234,
        .scancode = 0x001E,
        .ascii = 'a',
        .modifiers = 0,
        .pressed = 1,
        .reserved = {0, 0, 0},
    };

    keyboard_device_enqueue(&event);

    file_t* file = keyboard_device_open();
    TEST_ASSERT_NOT_NULL(file);

    keyboard_event_t buffer[2] = {0};
    int64_t bytes = file_read(file, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT64(sizeof(keyboard_event_t), bytes);
    TEST_ASSERT_EQUAL_UINT64(event.timestamp_ns, buffer[0].timestamp_ns);
    TEST_ASSERT_EQUAL_UINT16(event.scancode, buffer[0].scancode);
    TEST_ASSERT_EQUAL_UINT8(event.ascii, buffer[0].ascii);
    TEST_ASSERT_EQUAL_UINT8(event.modifiers, buffer[0].modifiers);
    TEST_ASSERT_EQUAL_UINT8(event.pressed, buffer[0].pressed);

    file_unref(file);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_keyboard_device_delivers_events);
    return UNITY_END();
}
