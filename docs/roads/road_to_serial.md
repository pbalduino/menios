# Road to Serial Console on meniOS

**Goal**: Enable full interactive access to meniOS through serial port, allowing headless operation and remote debugging without network support.

## Current Status

**Serial Output Working:**
- ✅ Kernel logging to serial port
- ✅ Debug output via serial
- ✅ Basic UART driver in `src/kernel/serial.c`

**What's Missing:**
- ❌ Interrupt-driven serial input
- ❌ `/dev/ttyS0` device file
- ❌ Getty/login on serial console
- ❌ Multi-console support (VGA + serial simultaneously)

### Recent Foundations
- **TTY subsystem** (#169 - CLOSED) provides line discipline, canonical mode, echo, and special character handling - all the hard terminal emulation work is done!
- **Init program** (#149 - CLOSED) can spawn and supervise getty processes
- **Devfs** (#152 - CLOSED) allows dynamic device node creation
- **Process groups and sessions** enable proper terminal control

## Why Serial Console?

### Use Cases
1. **Headless Operation**: Access system without monitor/keyboard attached
2. **Remote Debugging**: Serial console persists when everything else fails
3. **Embedded Systems**: Serial is often the primary interface
4. **Recovery Mode**: Access system when graphics/input fail
5. **Development**: QEMU/VirtualBox serial-to-stdio is incredibly convenient
6. **Hardware Debugging**: Serial output survives kernel panics

### Advantages Over VGA Console
- Works before video initialization
- Survives graphics crashes
- Can be captured/logged easily
- No special hardware needed (just 3 wires: TX, RX, GND)
- Remote access via serial-over-USB or serial-over-network
- Standard interface for embedded/server systems

## Implementation Roadmap

### Phase 1: Interrupt-Driven Serial Input (1 week)

**Goal**: Enable the serial port to receive data via interrupts instead of polling.

#### Issue #171: Serial Interrupt Input

**Tasks:**
1. **UART Interrupt Setup** (1-2 days)
   - Enable UART receive interrupts in IER register
   - Register IRQ handlers for COM1 (IRQ 4) and COM2 (IRQ 3)
   - Handle interrupt identification (RDA, THRE, RLS, MSI)

2. **Input Buffering** (1-2 days)
   - Implement circular buffer for received bytes (4KB default)
   - Handle overflow conditions gracefully
   - Add mutex protection for concurrent access
   - Support flow control (XON/XOFF or RTS/CTS)

3. **TTY Integration** (2-3 days)
   - Wire interrupt handler to existing TTY layer (#169)
   - Feed received bytes to TTY line discipline
   - Let TTY handle canonical mode, echo, backspace, Ctrl+C, etc.
   - Support raw mode for applications that need it

4. **Error Handling** (1 day)
   - Detect and handle overrun errors (lost data)
   - Handle framing errors (bad baud rate or noise)
   - Handle parity errors
   - Detect break conditions

**Success Criteria:**
- Characters typed on serial terminal appear in kernel
- No data loss under typical conditions
- TTY line discipline processes input correctly
- Canonical mode (line editing) works via serial

**Estimated Effort:** 3-5 days

---

### Phase 2: `/dev/ttyS0` Device Interface (1 week)

**Goal**: Expose serial port as a standard Unix device file for userspace access.

#### Issue #142: /dev/ttyS0 Device (already exists)

**Tasks:**
1. **Device File Operations** (2-3 days)
   - Implement `serial_open()`, `serial_close()`
   - Implement `serial_read()` - blocking read from input buffer
   - Implement `serial_write()` - output to UART
   - Handle multiple opens and reference counting

2. **TTY Integration** (2-3 days)
   - Register serial device as a TTY device
   - Implement termios compatibility
   - Support `tcgetattr()`, `tcsetattr()`
   - Set as controlling terminal for processes

3. **Serial Configuration IOCTLs** (2 days)
   - `TCGETS`/`TCSETS` - termios get/set
   - `TCSBRK` - send break signal
   - `TIOCMGET`/`TIOCMSET` - modem control lines
   - Baud rate configuration (300-115200 bps)
   - Data bits (5, 6, 7, 8), parity (none, odd, even), stop bits (1, 2)

4. **Multi-Port Support** (1 day)
   - Support `/dev/ttyS0` (COM1), `/dev/ttyS1` (COM2)
   - Auto-detect available serial ports at boot
   - Create device nodes dynamically

**Success Criteria:**
- `/dev/ttyS0` device file exists and is accessible
- Applications can `open()`, `read()`, `write()`, `ioctl()` serial port
- Configuration changes (baud rate, etc.) work correctly
- Compatible with standard Unix serial programming

**Estimated Effort:** 5-7 days

---

### Phase 3: Getty/Login on Serial (3-5 days)

**Goal**: Allow users to log in via serial console and get an interactive shell.

#### Issue #172: Getty on Serial

**Tasks:**
1. **Init Configuration** (1 day)
   - Extend init to spawn multiple getty instances
   - Add configuration for serial ports (device, baud rate)
   - Auto-detect available serial ports
   - Configure terminal type (TERM=vt100)

2. **Getty on Serial TTY** (2 days)
   - Spawn `getty /dev/ttyS0 115200` from init
   - Open serial device as stdin/stdout/stderr
   - Call `setsid()` to create new session
   - Set controlling terminal with `TIOCSCTTY` ioctl
   - Display login banner on serial console
   - Prompt for username

3. **Login and Shell** (1 day)
   - Login program works with serial TTY
   - Verify credentials (when multi-user support exists)
   - Set environment variables (HOME, USER, TERM, PATH)
   - Execute user's login shell with serial as terminal
   - Shell reads from serial, writes to serial

4. **Session Management** (1 day)
   - Support multiple simultaneous consoles (VGA + serial)
   - Each getty runs in separate session
   - Handle logout (getty respawns)
   - Proper process group and session cleanup

**Success Criteria:**
- Login prompt appears on serial terminal at boot
- Users can log in via serial connection
- Shell works interactively (command editing, history, Ctrl+C)
- Multiple logins work simultaneously (VGA console + serial)
- Getty respawns after logout

**Estimated Effort:** 3-5 days

---

## Timeline Estimate

**Total Effort:** 2-3 weeks for full serial console support

**Fast Track (Minimum Viable):** 1-2 weeks
- Serial interrupt input (minimal buffering)
- Basic `/dev/ttyS0` device (no fancy IOCTLs)
- Getty spawning on serial (hardcoded config)

**Production-Ready:** 2-3 weeks
- Robust error handling
- Full termios/ioctl support
- Configurable init system
- Multi-port support
- Comprehensive testing

## Dependencies

### Completed Prerequisites ✅
- **#169** - TTY subsystem (CLOSED) - handles line discipline, the hard part!
- **#149** - Init program (CLOSED) - can spawn getty
- **#152** - Devfs (CLOSED) - device file creation
- **#103** - Signals (CLOSED) - Ctrl+C, Ctrl+Z support
- **#93** - fork/exec (CLOSED) - process creation for getty/login
- **#145** - wait/waitpid (CLOSED) - init supervises getty

### New Issues (This Roadmap)
- **#171** - Serial interrupt input (NEW)
- **#142** - /dev/ttyS0 device (EXISTS, OPEN)
- **#172** - Getty/login on serial (NEW)

### Dependency Chain
```
#169 (TTY subsystem) ──┐
                       ├──→ #171 (serial input) ──→ #142 (/dev/ttyS0) ──→ #172 (getty/login)
#149 (init) ───────────┘                                                      ↑
#152 (devfs) ────────────────────────────────────────────────────────────────┘
```

## Success Criteria

meniOS will have a fully functional serial console when:
- ✅ Serial port receives input via interrupts (no polling)
- ✅ `/dev/ttyS0` device file provides bidirectional communication
- ✅ Getty displays login prompt on serial terminal
- ✅ Users can log in via serial connection
- ✅ Interactive shell works over serial (line editing, command execution)
- ✅ Multiple simultaneous logins work (VGA + serial)
- ✅ Serial console persists across reboots
- ✅ Compatible with standard Unix serial tools (screen, minicom, cu)

## Connection Methods

### From Host Machine

**Using screen:**
```bash
screen /dev/ttyUSB0 115200
# or for macOS
screen /dev/cu.usbserial 115200
```

**Using minicom:**
```bash
minicom -D /dev/ttyUSB0 -b 115200
```

**Using cu:**
```bash
cu -l /dev/ttyUSB0 -s 115200
```

**Using Python:**
```bash
python -m serial.tools.miniterm /dev/ttyUSB0 115200
```

### QEMU/VirtualBox

**QEMU with serial to stdio:**
```bash
qemu-system-i386 -kernel menios.bin -serial stdio
```

**QEMU with serial to file:**
```bash
qemu-system-i386 -kernel menios.bin -serial file:serial.log
```

**QEMU with serial to socket:**
```bash
qemu-system-i386 -kernel menios.bin -serial tcp::4444,server
# Connect with: telnet localhost 4444
```

## Expected User Experience

```
$ screen /dev/ttyUSB0 115200

meniOS v0.1.0
Kernel: i386-elf

ttyS0 login: root
Password:

Welcome to meniOS!

# pwd
/root

# ls /dev
console  null  zero  ttyS0  ttyS1  kbd0  fb0

# cat /proc/version
meniOS 0.1.0 (i386) #1 SMP Tue Sep 30 2025

# ps aux
PID  USER  TIME  COMMAND
  1  root  0:00  init
 12  root  0:00  getty /dev/console
 13  root  0:00  getty /dev/ttyS0 115200
 14  root  0:00  mosh

# echo "Hello from serial!" > /dev/ttyS1
# cat < /dev/ttyS1 &
#

# exit
logout

meniOS v0.1.0
ttyS0 login: _
```

## Current Priority

**Status:** MEDIUM-HIGH PRIORITY

Serial console is valuable for:
- Development and debugging (immediate benefit)
- Headless operation
- Recovery scenarios
- Professional OS credibility

**When to Implement:**
- After shell core features (#163-#165) are stable
- Alongside or before graphics-heavy features
- Before Doom (useful debugging tool)
- Anytime during "roads" phase - doesn't block other work

**Recommended Timeline:**
Implement after completing current shell work (built-ins, I/O, pipes). Serial console complements shell perfectly and provides a robust development interface.

## Testing Strategy

### Phase 1 Tests (Interrupt Input)
1. Type characters on serial terminal, verify kernel receives them
2. Test at various baud rates (9600, 19200, 38400, 57600, 115200)
3. Send rapid input, verify no data loss
4. Test special characters (Ctrl+C, Ctrl+D, Ctrl+Z)
5. Verify TTY canonical mode (backspace, line editing)

### Phase 2 Tests (Device File)
1. `cat < /dev/ttyS0` - should block waiting for input
2. `echo "test" > /dev/ttyS0` - should output to serial
3. `stty -F /dev/ttyS0` - should show terminal settings
4. Change baud rate with `stty`, verify communication continues
5. Test simultaneous reads and writes

### Phase 3 Tests (Getty/Login)
1. Boot system, verify login prompt on serial
2. Log in and execute commands
3. Test command editing (arrow keys, backspace)
4. Test job control (Ctrl+C, Ctrl+Z, fg, bg)
5. Log out, verify getty respawns
6. Simultaneous VGA + serial logins
7. Test in QEMU with `-serial stdio`

## Future Enhancements

Beyond basic serial console:
1. **Serial-over-USB** - USB CDC ACM device class
2. **Serial multiplexing** - Multiple sessions over one serial line
3. **Serial debugging protocol** - GDB remote serial protocol
4. **Serial networking** - SLIP/PPP for network-over-serial
5. **Serial logging daemon** - Dedicated kernel log output on serial
6. **Serial bootloader** - Load kernel via serial (XMODEM/YMODEM)
7. **Bluetooth serial** - RFCOMM profile for wireless console

## Notes

- Serial console is one of the easiest "roads" to complete
- Only ~2 weeks of work with huge payoff for development
- Reuses existing TTY infrastructure (#169) - the hard work is done!
- Standard UART hardware is simple and well-documented
- Every professional OS has serial console support
- Essential for embedded systems and servers

---

*Last Updated: 2025-09-30*
*Status: Ready to implement*
*Issues: #171, #142, #172*
*Estimated Completion: 2-3 weeks*
