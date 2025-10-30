#define _XOPEN_SOURCE 700
#include "unity.h"

#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}


void test_pathconf_name_max(void) {
  long value = pathconf("/", _PC_NAME_MAX);
  TEST_ASSERT_TRUE(value >= 0);
}

void test_pathconf_path_max(void) {
  long value = pathconf("/", _PC_PATH_MAX);
  TEST_ASSERT_TRUE(value >= 0);
}

void test_pathconf_link_max(void) {
  long value = pathconf("/", _PC_LINK_MAX);
  TEST_ASSERT_TRUE(value >= 1);
}

void test_pathconf_chown_restricted(void) {
  long value = pathconf("/", _PC_CHOWN_RESTRICTED);
  TEST_ASSERT_TRUE(value >= 0);
}

void test_pathconf_no_trunc(void) {
  long value = pathconf("/", _PC_NO_TRUNC);
  TEST_ASSERT_TRUE(value >= 0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pathconf_name_max);
  RUN_TEST(test_pathconf_path_max);
  RUN_TEST(test_pathconf_link_max);
    RUN_TEST(test_pathconf_chown_restricted);
  RUN_TEST(test_pathconf_no_trunc);
  return UNITY_END();
}
