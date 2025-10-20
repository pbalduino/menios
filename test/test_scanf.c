#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

#define ASSERT_EQ_INT(expected, actual)                                                        \
  do {                                                                                         \
    if((expected) != (actual)) {                                                               \
      printf("Assertion failed: %s == %s (expected %d, got %d)\n", #expected, #actual,        \
             (expected), (actual));                                                            \
      exit(1);                                                                                 \
    }                                                                                          \
  } while(0)

#define ASSERT_STR_EQ(expected, actual)                                                        \
  do {                                                                                         \
    if(strcmp((expected), (actual)) != 0) {                                                    \
      printf("Assertion failed: %s == %s (expected '%s', got '%s')\n", #expected, #actual,    \
             (expected), (actual));                                                            \
      exit(1);                                                                                 \
    }                                                                                          \
  } while(0)

static void test_sscanf_integers(void) {
  const char* input = "123 077 0x1f -42";
  int a = 0;
  unsigned int b = 0;
  unsigned int c = 0;
  int d = 0;
  int conversions = sscanf(input, "%d %o %x %d", &a, &b, &c, &d);
  ASSERT_EQ_INT(4, conversions);
  ASSERT_EQ_INT(123, a);
  ASSERT_EQ_INT(077, (int)b);
  ASSERT_EQ_INT(0x1f, (int)c);
  ASSERT_EQ_INT(-42, d);
}

static void test_sscanf_width_and_suppression(void) {
  const char* input = "abcdef";
  char buf[4];
  memset(buf, 0, sizeof(buf));
  int value = 0;

  int conversions = sscanf(input, "%3s%*1c%d", buf, &value);
  ASSERT_EQ_INT(1, conversions);
  ASSERT_STR_EQ("abc", buf);
  ASSERT_EQ_INT(0, value);
}

static void test_sscanf_character_and_n(void) {
  const char* input = "hi!";
  char chars[3];
  int count = 0;
  int conversions = sscanf(input, "%2c%n", chars, &count);
  ASSERT_EQ_INT(1, conversions);
  ASSERT_EQ_INT(2, count);
  ASSERT_EQ_INT('h', chars[0]);
  ASSERT_EQ_INT('i', chars[1]);
}

static void test_return_on_failure(void) {
  const char* input = "xyz";
  int value = 0;
  int conversions = sscanf(input, "%d", &value);
  ASSERT_EQ_INT(0, conversions);
}

static void test_fscanf_from_fd(void) {

  const char* path = "menios_scanf_test.txt";
  FILE* stream = fopen(path, "w+");
  if(stream == NULL) {
    printf("Failed to create temporary stream\n");
    exit(1);
  }

  const char* payload = "42 test\n";
  if(fputs(payload, stream) == EOF) {
    printf("Failed to write payload\n");
    fclose(stream);
    remove(path);
    exit(1);
  }

  if(fseek(stream, 0, SEEK_SET) != 0) {
    printf("Failed to rewind stream\n");
    fclose(stream);
    remove(path);
    exit(1);
  }

  int value = 0;
  char buf[16];
  int conversions = fscanf(stream, "%d %s", &value, buf);
  fclose(stream);
  remove(path);

  ASSERT_EQ_INT(2, conversions);
  ASSERT_EQ_INT(42, value);
  ASSERT_STR_EQ("test", buf);
}

int main(void) {
  test_sscanf_integers();
  test_sscanf_width_and_suppression();
  test_sscanf_character_and_n();
  test_return_on_failure();
  test_fscanf_from_fd();
  printf("All scanf tests passed.\n");
  return 0;
}
