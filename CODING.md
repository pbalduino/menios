# meniOS Coding Style

This document outlines the coding style guidelines for meniOS, inspired by the Linux Kernel Coding Style but with specific modifications to suit the meniOS project. Following these guidelines ensures consistency and readability across the codebase.

## 1. Indentation

- **Rule**: Use two spaces for indentation. Do not use tabs.

- **Good Example**:

    ```c
    if (condition) {
      do_something();
    }
    ```

- **Bad Example**:

    ```c
    if (condition) {
        do_something();  // Four spaces used for indentation
    }

    if (condition) {
    do_something();  // No indentation used
    }

    if (condition) {
    do_something();  // No indentation used
    }

    if (condition) {
        do_something();  // Tab used for indentation
    }
    ```

## 2. Maximum Line Length

- **Rule**: The maximum line length is 120 characters.

- **Good Example**:

    ```c
    int calculate_result(int a, int b) {
      return a + b * 2;  // Line length is well under 120 characters
    }
    ```

- **Bad Example**:

    ```c
    int calculate_result(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
      return a + b + c + d + e + f + g + h + i + j;  // Line length exceeds 120 characters
    }
    ```

## 3. Braces Placement

- **Rule**: The opening brace should be on the same line as the declaration. This applies to all control statements (if, else, for, while, etc.) and function definitions.

- **Good Example**:

    ```c
    if (condition) {
      do_something();
    }

    for (int i = 0; i < n; i++) {
      process(i);
    }

    void function() {
      // function body
    }
    ```

- **Bad Example**:

    ```c
    if (condition)
    {
      do_something();  // Opening brace is on a new line
    }

    for (int i = 0; i < n; i++)
    {
      process(i);  // Opening brace is on a new line
    }

    void function()
    {
      // function body;  // Opening brace is on a new line
    }
    ```

## 4. Use of Braces for Statements

- **Rule**: Always use braces for all control statements, even if the body contains only one line.

- **Good Example**:

    ```c
    if (condition) {
      do_something();
    }

    while (condition) {
      continue_processing();
    }
    ```

- **Bad Example**:

    ```c
    if (condition)
      do_something();  // Braces are missing

    while (condition)
      continue_processing();  // Braces are missing
    ```

## 5. Header Guards

- **Rule**: Header guards should be formatted based on the file location:
  - `MENIOS_INCLUDE_KERNEL_` prefix for files inside `include/kernel`.
  - `MENIOS_INCLUDE_` prefix for files inside `include/`.
  - `MENIOS_` prefix for files located anywhere else.
  - Header guards should end with `_H`.

- **Good Examples**:

    ```c
    // File: include/kernel/somefile.h
    #ifndef MENIOS_INCLUDE_KERNEL_SOMEFILE_H
    #define MENIOS_INCLUDE_KERNEL_SOMEFILE_H

    // header file content

    #endif // MENIOS_INCLUDE_KERNEL_SOMEFILE_H
    ```

    ```c
    // File: include/util.h
    #ifndef MENIOS_INCLUDE_UTIL_H
    #define MENIOS_INCLUDE_UTIL_H

    // header file content

    #endif // MENIOS_INCLUDE_UTIL_H
    ```

    ```c
    // File: src/driver.h
    #ifndef MENIOS_DRIVER_H
    #define MENIOS_DRIVER_H

    // header file content

    #endif // MENIOS_DRIVER_H
    ```

- **Bad Examples**:

    ```c
    // File: include/kernel/somefile.h
    #ifndef KERNEL_SOMEFILE_H
    #define KERNEL_SOMEFILE_H  // Incorrect prefix and missing MENIOS_INCLUDE_

    // header file content

    #endif
    ```

    ```c
    // File: include/util.h
    #ifndef UTIL_H
    #define UTIL_H  // Incorrect prefix and missing MENIOS_INCLUDE_

    // header file content

    #endif
    ```

    ```c
    // File: src/driver.h
    #ifndef DRIVER_H
    #define DRIVER_H  // Incorrect prefix and missing MENIOS_

    // header file content

    #endif
    ```

For any scenarios or coding practices not covered by this document, please refer to the Linux Coding Style guidelines. Updates to this document will be communicated as necessary.

By following these guidelines, the meniOS codebase will maintain a consistent and readable format, improving collaboration and code maintenance.