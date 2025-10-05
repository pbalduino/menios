# Contributing to meniOS

Thank you for your interest in contributing to meniOS! This hobby operating system aims to run Doom in userland, and we welcome contributions from developers of all skill levels who are passionate about operating systems development.

## Table of Contents
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Understanding the Project](#understanding-the-project)
- [How to Contribute](#how-to-contribute)
- [Development Workflow](#development-workflow)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Project Architecture](#project-architecture)
- [Communication](#communication)
- [License](#license)

## Getting Started

### Prerequisites

**For Linux Users:**
- GCC cross-compiler toolchain
- GNU Make
- QEMU (for testing)
- Git

**For macOS Users:**
- Docker
- GNU Make
- QEMU (for testing)
- Git

### Quick Setup

1. **Fork and Clone**
   ```bash
   git clone https://github.com/yourusername/menios.git
   cd menios
   ```

2. **Build and Test**
   ```bash
   make build run
   ```

3. **Verify Setup**
   - The system should boot in QEMU
   - You should see the meniOS console with kernel output
   - User mode demo should execute successfully (printing via syscall and exiting)

## Development Environment

### Docker Development (Recommended for macOS)

meniOS includes a Docker-based development environment that provides a consistent build environment across platforms:

```bash
# Build Docker image
make docker-build

# Build kernel in Docker
make docker-run-build

# Development with bind mount
make docker-dev
```

### Native Development (Linux)

For Linux users, native development is supported with appropriate cross-compilation tools:

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install gcc-multilib qemu-system-x86 make git

# Build and run
make build run
```

## Understanding the Project

### Current State

meniOS is an ambitious hobby OS project with these implemented features:

- **Kernel Foundation**: Limine bootloader integration, basic memory management
- **Process Management**: Kernel threads with basic scheduling
- **Memory Systems**: Physical/virtual memory allocation, page table management
- **I/O Systems**: PS/2 keyboard driver, ANSI console with scrolling
- **User Mode Infrastructure**: Ring 3 GDT segments, TSS, syscall dispatcher
- **Basic Syscalls**: `write()` and `exit()` system calls working
- **Testing Framework**: Unity-based unit tests for kernel components

### The Road to Doom

The ultimate goal is running Doom in userland. Current priorities include:

1. **Per-process virtual memory management** (In Progress)
2. **ELF loader** for userland programs
3. **Filesystem support** with VFS layer
4. **Userspace graphics/framebuffer** interface
5. **Cross-compiler toolchain** for userland
6. **Input/Audio subsystems** for gaming

See [`ROAD_TO_DOOM.md`](docs/road/road_to_doom.md) for the complete roadmap.

## How to Contribute

### Types of Contributions

We welcome various types of contributions:

- **Bug Reports**: Found an issue? Please report it!
- **Feature Implementation**: Pick a task from [`tasks.json`](tasks.json)
- **Documentation**: Improve comments, README, or architectural docs
- **Testing**: Add unit tests or improve test coverage
- **Code Review**: Review pull requests and provide feedback
- **Performance Improvements**: Optimize existing code

### Finding Issues to Work On

1. **Check [`tasks.json`](tasks.json)**: Our task tracking file contains detailed descriptions of all work items
2. **Browse GitHub Issues**: Look for issues labeled `good first issue` or `help wanted`
3. **Road to Doom**: Pick items from the roadmap that interest you
4. **Ask Questions**: Don't hesitate to ask about any aspect of the project

### Before You Start

1. **Check for Existing Work**: Search issues and PRs to avoid duplication
2. **Discuss Major Changes**: Open an issue for significant architectural changes
3. **Review Dependencies**: Check if your chosen task depends on other incomplete work

## Development Workflow

### 1. Planning Your Contribution

1. **Choose a Task**: Select from [`tasks.json`](tasks.json) or GitHub issues
2. **Understand the Scope**: Read the task description and dependencies
3. **Ask Questions**: Use GitHub issues or discussions for clarification

### 2. Implementation Process

1. **Create a Branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Implement Your Changes**
   - Follow the coding standards in [`CODING.md`](CODING.md)
   - Write tests for new functionality
   - Add comments explaining complex logic
   - Update documentation as needed

3. **Test Thoroughly**
   ```bash
   # Build and test
   make build run

   # Run unit tests
   make test

   # Test in QEMU
   # Verify your changes work as expected
   ```

4. **Update Task Tracking**
   - If working on a task from `tasks.json`, update its status
   - Add notes about your implementation approach

### 3. Submitting Your Contribution

1. **Commit Your Changes**
   ```bash
   git add .
   git commit -m "Brief description of changes

   - Detailed explanation of what was implemented
   - Any architectural decisions made
   - References to issues or tasks addressed"
   ```

2. **Push and Create PR**
   ```bash
   git push origin feature/your-feature-name
   ```

   Then create a Pull Request with:
   - Clear title and description
   - Reference to related issues/tasks
   - Testing performed
   - Any breaking changes

3. **Respond to Feedback**
   - Address code review comments promptly
   - Be open to suggestions and improvements
   - Update your branch as needed

## Coding Standards

### Code Style

meniOS follows specific coding guidelines outlined in [`CODING.md`](CODING.md):

- **Indentation**: 2 spaces (no tabs)
- **Line Length**: 120 characters maximum
- **Braces**: Opening brace on same line
- **Always use braces** for control statements
- **Header guards** with `MENIOS_` prefix convention

### Specific Guidelines

1. **Function Naming**: Use descriptive names with underscores
   ```c
   // Good
   void init_page_tables(void);
   int allocate_physical_page(size_t size);

   // Avoid
   void ipt(void);
   int alloc(size_t s);
   ```

2. **Error Handling**: Always check return values
   ```c
   // Good
   int result = kmalloc(size);
   if (result == NULL) {
     return -ENOMEM;
   }
   ```

3. **Comments**: Explain the "why", not just the "what"
   ```c
   // Good
   // Flush TLB to ensure page table changes are visible to all cores
   flush_tlb();

   // Less helpful
   // Call flush_tlb function
   flush_tlb();
   ```

4. **Constants**: Use descriptive macro names
   ```c
   #define PAGE_SIZE 4096
   #define MAX_PROCESSES 256
   ```

### Architecture-Specific Guidelines

- **Memory Management**: Always use virtual addresses in kernel code
- **Synchronization**: Use appropriate locking for shared data structures
- **Interrupts**: Document interrupt safety requirements
- **Assembly**: Keep assembly code minimal and well-documented

## Testing Guidelines

### Unit Testing

meniOS uses the Unity testing framework:

1. **Test Location**: Place tests in the `test/` directory
2. **Test Naming**: Use descriptive test function names
3. **Test Coverage**: Aim for good coverage of new functionality

```c
// Example test structure
void test_memory_allocation_success(void) {
  // Setup
  init_memory_system();

  // Action
  void* ptr = kmalloc(1024);

  // Assert
  TEST_ASSERT_NOT_NULL(ptr);
  TEST_ASSERT_TRUE(is_valid_pointer(ptr));

  // Cleanup
  kfree(ptr);
}
```

### Integration Testing

- **QEMU Testing**: Always test changes in QEMU
- **Real Hardware**: Test on real hardware when possible
- **Regression Testing**: Ensure existing functionality still works

### Performance Testing

- **Boot Time**: Measure impact on boot performance
- **Memory Usage**: Monitor memory allocation patterns
- **Scheduling**: Verify scheduler behavior with your changes

## Project Architecture

### Directory Structure

```
menios/
├── src/               # Kernel source code
│   ├── kernel/        # Core kernel functionality
│   ├── drivers/       # Hardware drivers
│   └── libc/          # Basic C library functions
├── include/           # Header files
│   └── kernel/        # Kernel-specific headers
├── test/              # Unit tests
├── bin/               # Build artifacts
├── vendor/            # Third-party dependencies
└── docs/              # Project documentation
```

### Key Components

1. **Memory Management** (`src/kernel/memory/`)
   - Physical memory manager (PMM)
   - Virtual memory manager (VMM)
   - Kernel heap (kmalloc)

2. **Process Management** (`src/kernel/process/`)
   - Kernel threading
   - Basic scheduler
   - Synchronization primitives

3. **I/O Systems** (`src/drivers/`)
   - PS/2 keyboard driver
   - Console/framebuffer
   - Future: VGA, audio, storage

4. **System Calls** (`src/kernel/syscall/`)
   - Syscall dispatcher
   - Individual syscall implementations

### Adding New Features

When adding new features:

1. **Follow Existing Patterns**: Study similar code in the codebase
2. **Maintain Modularity**: Keep components loosely coupled
3. **Consider Dependencies**: Understand what your code depends on
4. **Plan for Growth**: Design with future expansion in mind

## Communication

### Getting Help

- **GitHub Issues**: For bugs, feature requests, and general questions
- **GitHub Discussions**: For broader conversations about architecture
- **Code Comments**: For specific implementation questions

### Reporting Issues

When reporting bugs:

1. **Use the issue template** (if available)
2. **Include reproduction steps**
3. **Provide system information** (OS, Docker version, etc.)
4. **Add relevant logs** or error messages
5. **Describe expected vs. actual behavior**

### Proposing Features

For new features:

1. **Check the roadmap** first ([`ROAD_TO_DOOM.md`](docs/road/road_to_doom.md))
2. **Open an issue** for discussion
3. **Provide use cases** and benefits
4. **Consider implementation complexity**
5. **Be open to feedback** and alternative approaches

## Community Guidelines

Please review our [Code of Conduct](CODE_OF_CONDUCT.md). We are committed to providing a welcoming and inclusive environment for all contributors.

### Best Practices

- **Be respectful** in all interactions
- **Provide constructive feedback** in code reviews
- **Help newcomers** get up to speed
- **Celebrate progress** and learning
- **Share knowledge** and insights

## Development Tips

### Debugging

1. **QEMU GDB Integration**: Use QEMU's GDB support for debugging
2. **Logging**: Add strategic log messages for troubleshooting
3. **Assert Macros**: Use assertions to catch errors early
4. **Unit Tests**: Write tests to isolate and verify behavior

### Common Pitfalls

- **Memory Management**: Always check for NULL returns from allocation
- **Integer Overflow**: Be careful with size calculations
- **Concurrency**: Consider thread safety in shared code
- **Assembly Interface**: Double-check calling conventions

### Useful Commands

```bash
# Quick build and test
make clean build run

# Build with specific target
make ARCH=x86-64 build

# Run with different QEMU options
make run QEMU_OPTS="-m 512M"

# Run unit tests
make test

# Clean all build artifacts
make clean
```

## License

By contributing to meniOS, you agree that your contributions will be licensed under the same [MIT License](LICENSE) that covers the project.

All contributors retain copyright to their contributions while granting the project the right to use, modify, and distribute the code under the project license.

---

**Happy Hacking!** 🚀

Whether you're fixing a small bug or implementing a major feature for the Road to Doom, every contribution helps make meniOS better. Don't hesitate to ask questions, and remember that learning is part of the journey in OS development.

For questions about contributing, feel free to open an issue or reach out to the maintainers. We're here to help you succeed!
