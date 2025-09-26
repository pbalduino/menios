# Security Policy

## Project Context

meniOS is a hobby operating system kernel written in C and Assembly for the x86-64 architecture. As an educational and experimental OS project with the goal of running Doom in userland, security considerations are approached from both a learning perspective and responsible development practices.

**Important Note**: meniOS is currently in active development and **not intended for production use**. It is designed for educational purposes, experimentation, and the specific goal of running Doom in a custom userland environment.

## Supported Versions

Currently, meniOS follows a rolling development model on the `trunk` branch. Security updates are applied to the latest development version.

| Version | Supported          | Status |
| ------- | ------------------ | ------ |
| trunk   | :white_check_mark: | Active development |
| main    | :white_check_mark: | Stable development |

## Security Scope

### What We Consider Security Issues

For an operating system kernel, even in a hobby context, we take the following security concerns seriously:

#### **Memory Safety Issues**
- Buffer overflows and underflows
- Use-after-free vulnerabilities
- Double-free errors
- Integer overflow/underflow leading to memory corruption
- Stack overflow vulnerabilities
- Null pointer dereferences in critical paths

#### **Privilege Escalation**
- Improper user/kernel boundary enforcement
- Syscall parameter validation failures
- Ring 3 to Ring 0 privilege escalation vectors
- Memory permission bypass vulnerabilities

#### **Information Disclosure**
- Kernel memory leaks to userspace
- Uninitialized memory exposure
- Side-channel information leaks
- Debug information exposure in release builds

#### **Resource Management**
- Memory allocation/deallocation issues
- Resource exhaustion vulnerabilities
- Infinite loops or excessive CPU consumption
- Deadlock conditions in kernel code

#### **Input Validation**
- Malformed ELF file processing (once ELF loader is implemented)
- Syscall parameter validation
- Hardware input validation (keyboard, etc.)
- Boot parameter validation

### What We Don't Currently Consider Security Issues

Given meniOS's current development status and educational nature:

- **Cryptographic implementations** (not yet implemented)
- **Network security** (no network stack implemented)
- **Filesystem security** (VFS not yet implemented)
- **Multi-user security** (single-user system currently)
- **Hardware security features** (not yet utilizing advanced CPU security features)

## Reporting a Vulnerability

### For Security Researchers and Contributors

If you discover a security vulnerability in meniOS, we appreciate responsible disclosure. Please follow these steps:

#### **1. Initial Contact**
- **Email**: pbalduino [at] gmail [dot] com
- **Subject**: "[SECURITY] meniOS Vulnerability Report"
- **Encryption**: If you have security concerns about email communication, please indicate this in your initial contact

#### **2. Information to Include**

Please provide as much detail as possible:

```
Vulnerability Details:
- Component affected (memory management, syscalls, drivers, etc.)
- Vulnerability type (buffer overflow, privilege escalation, etc.)
- Potential impact and severity assessment
- Steps to reproduce the issue
- Proof of concept code (if applicable)
- Suggested mitigation or fix (if known)

Environment:
- meniOS version/commit hash
- Build configuration
- Testing environment (QEMU, real hardware)
- Compiler version and flags used
```

#### **3. Response Timeline**

As a hobby project with limited maintainer resources:

- **Initial acknowledgment**: Within 7 days
- **Initial assessment**: Within 14 days
- **Fix development**: Timeframe varies based on complexity
- **Public disclosure**: After fix is implemented and tested

### **4. Responsible Disclosure Policy**

We request that you:
- Give us reasonable time to investigate and fix the issue
- Do not publicly disclose the vulnerability until we've had a chance to address it
- Do not exploit the vulnerability for malicious purposes
- Help us understand and reproduce the issue

In return, we commit to:
- Acknowledge your responsible disclosure
- Keep you informed of our progress
- Give you credit for the discovery (unless you prefer anonymity)
- Work diligently to fix confirmed vulnerabilities

## Security Development Practices

### Current Security Measures

Even as a hobby project, meniOS implements several security-conscious development practices:

#### **Code Quality**
- **Static Analysis**: Regular code review for common vulnerability patterns
- **Unit Testing**: Unity-based testing framework for critical components
- **Coding Standards**: Strict adherence to [`CODING.md`](CODING.md) guidelines
- **Memory Management**: Careful memory allocation and deallocation practices

#### **Architecture Security**
- **User/Kernel Separation**: Implementation of Ring 0/Ring 3 privilege levels
- **Memory Protection**: Virtual memory management with proper page permissions
- **Syscall Validation**: Parameter validation in syscall interface
- **Stack Protection**: Careful stack management in kernel code

#### **Build Security**
- **Compiler Warnings**: Build with strict warning levels
- **Debug Symbols**: Separate debug builds from release builds
- **Reproducible Builds**: Docker-based build environment for consistency

### Planned Security Enhancements

As development progresses toward userland support:

#### **Short Term**
- Enhanced syscall parameter validation
- Improved memory permission enforcement
- Better error handling and bounds checking
- Stack canaries and guard pages

#### **Medium Term**
- ELF loader security validation
- Filesystem access controls
- Resource limiting and quotas
- Improved privilege separation

#### **Long Term**
- Hardware security feature utilization (SMEP, SMAP, etc.)
- Control flow integrity
- Address space layout randomization (ASLR)
- Kernel stack protection

## Security Testing

### How to Test for Security Issues

Contributors and security researchers can help by:

#### **Static Analysis**
```bash
# Example tools that can be used
clang-static-analyzer src/
cppcheck --enable=all src/
```

#### **Dynamic Testing**
```bash
# Build with debugging and sanitizers
make clean
make CFLAGS="-fsanitize=address -g" build

# Test in QEMU with memory debugging
make run QEMU_OPTS="-s -S"
```

#### **Fuzzing Opportunities**
- Syscall parameter fuzzing
- Keyboard input fuzzing
- Memory allocation/deallocation patterns
- Boot parameter fuzzing

### Security Review Areas

High-priority areas for security review:

1. **Memory Management** (`src/kernel/memory/`)
   - Physical and virtual memory allocation
   - Page table management
   - Kernel heap implementation

2. **System Calls** (`src/kernel/syscall/`)
   - Parameter validation
   - Privilege checking
   - Return value handling

3. **Process Management** (`src/kernel/process/`)
   - Thread creation and termination
   - Context switching
   - Scheduler implementation

4. **Hardware Drivers** (`src/drivers/`)
   - Input validation
   - Hardware interaction safety
   - Interrupt handling

## Educational Security Resources

Since meniOS is an educational project, we encourage learning about OS security:

### **Recommended Reading**
- "The Design and Implementation of the FreeBSD Operating System" (McKusick & Neville-Neil)
- "Operating Systems: Three Easy Pieces" (Arpaci-Dusseau)
- "Secure Coding in C and C++" (Seacord)

### **Security Research Areas**
- Kernel exploitation techniques
- Memory safety in systems programming
- Privilege escalation vectors
- Hardware security features

### **Tools and Techniques**
- Static analysis tools (Clang Static Analyzer, Coverity)
- Dynamic analysis (AddressSanitizer, Valgrind)
- Fuzzing frameworks (AFL, LibFuzzer)
- Debugging techniques (GDB, QEMU debugging)

## Acknowledgments

We appreciate all security researchers and contributors who help make meniOS more secure through responsible disclosure and security-conscious development practices.

### **Security Contributors**
*This section will be updated as security issues are reported and fixed.*

## Contact

For security-related questions or concerns:

- **Maintainer**: Plínio Balduino
- **Email**: pbalduino [at] gmail [dot] com
- **GitHub**: [@pbalduino](https://github.com/pbalduino)

For general project questions, please use the [GitHub Issues](https://github.com/pbalduino/menios/issues) tracker.

---

**Note**: While meniOS is a hobby project, we take security seriously as part of the learning process and responsible development practices. Your contributions to improving the security posture of meniOS are greatly appreciated!