# Road to Real Hardware: Running meniOS on Physical Laptops

🎯 **Goal**: Boot and run meniOS on real hardware, specifically targeting three test laptops from 2008-2012 era.

## 📊 **Target Hardware**

### **Target Laptop 1: ACER Extensa 5620-6830 (2008)**
- **CPU**: Intel Core 2 Duo T5250 (1.5GHz, 667MHz FSB, 2MB L2 cache)
- **Chipset**: Mobile Intel 965 Express (GMA965)
- **Graphics**: Intel GMA X3100 integrated
- **Memory**: 1GB DDR2-667 (expandable to 4GB)
- **Storage**: 200GB SATA HDD (4200rpm, 8MB cache)
- **Network**: Intel 3945 802.11a/b/g Mini-PCIe WiFi
- **Display**: 15.4" WXGA (1280x800) Crystal Brite LCD
- **Input**: PS/2 keyboard and trackpad
- **Boot**: Legacy BIOS

### **Target Laptop 2: MacBook White (2008-2010)**
- **CPU**: Intel Core 2 Duo P7350/P8600 (2.0-2.4GHz, Penryn)
- **Chipset**: Intel PM45/GM45 Express (Early 2008: Intel X3100, Late 2008+: NVIDIA chipset)
- **Graphics**:
  - Early 2008: Intel GMA X3100
  - Late 2008+: NVIDIA GeForce 9400M/320M (256MB shared)
- **Memory**: 2-4GB DDR2/DDR3 SDRAM
- **Storage**: SATA HDD/SSD
- **Network**: Broadcom BCM4321/BCM4322 802.11n WiFi
- **Display**: 13.3" glossy widescreen (1280x800)
- **Input**: USB keyboard and trackpad (internal)
- **Boot**: EFI firmware (32-bit EFI on some models, 64-bit on later)

### **Target Laptop 3: Dell Latitude E6420/E6430 (2011-2012)**
- **CPU**:
  - E6420: Intel Core i5-2520M/i7-2620M (Sandy Bridge, 2nd gen)
  - E6430: Intel Core i5-3320M/i7-3520M (Ivy Bridge, 3rd gen)
- **Chipset**:
  - E6420: Mobile Intel QM67 Express
  - E6430: Mobile Intel QM77/HM77 Express
- **Graphics**: Intel HD Graphics 3000/4000 or NVIDIA NVS 4200M (512MB DDR3)
- **Memory**: Up to 8GB DDR3-1333/1600
- **Storage**: SATA HDD/SSD
- **Network**: Intel WiFi (various models), Gigabit Ethernet
- **Display**: 14" HD/HD+ (1366x768 or 1600x900)
- **Input**: PS/2 keyboard and trackpad (internal), USB ports
- **Boot**: UEFI firmware with CSM (Legacy BIOS compatibility)
- **Ports**: 2x USB 3.0 (E6430), VGA, DisplayPort, eSATA

## 🏗️ **Current meniOS Hardware Support**

### ✅ **Already Implemented**
- **Bootloader**: Limine (supports both BIOS and UEFI) ✅
- **CPU**: x86-64 architecture, Core 2 Duo and newer ✅
- **Memory**: Physical memory manager, virtual memory, paging ✅
- **Storage**: AHCI SATA driver with DMA ✅
- **Input**: PS/2 keyboard driver ✅
- **Graphics**: Framebuffer console (text mode via `/dev/fb0`) ✅
- **Timers**: PIT, TSC, RTC, APIC timer ✅
- **Interrupts**: IDT, APIC, I/O APIC ✅
- **PCI**: PCI root complex, device enumeration ✅
- **ACPI**: uACPI integration for power management tables ✅

### 🚧 **Partially Implemented**
- **Graphics**: Framebuffer exists but only character-mode, no pixel-addressable mmap (#301)
- **Input**: PS/2 keyboard works but only ASCII key-down events, no scan codes/key-up (#302)
- **Network**: None implemented
- **USB**: None implemented (critical for MacBook)
- **WiFi**: None implemented

### ❌ **Missing / Not Implemented**
- **USB Host Controllers** (UHCI/OHCI/EHCI/xHCI)
- **USB HID drivers** (keyboard, mouse, trackpad)
- **Network drivers** (Ethernet, WiFi)
- **GPU acceleration** (Intel GMA, NVIDIA)
- **Audio drivers** (HD Audio, AC'97)
- **Power management** (ACPI S3/S4, CPU frequency scaling)
- **Battery/AC adapter** monitoring
- **Laptop-specific** (lid switch, function keys, backlight control)

## 🛣️ **Phase-Based Roadmap**

### **Phase 1: Bootloader & Basic Hardware** ✅ **MOSTLY COMPLETE**

#### **Limine Bootloader Integration** ✅ **COMPLETE**
- ✅ BIOS boot support (for older laptops)
- ✅ UEFI boot support (for MacBook, Dell E6430)
- ✅ Multiboot2 protocol
- ✅ Framebuffer initialization
- **Status**: Works in QEMU, should work on real hardware

#### **USB Installer Creation** ⛳ **TODO** (No existing issue)
- **Scope**:
  - Create bootable USB stick with Limine
  - FAT32 partition with kernel and boot files
  - Test on all three target laptops
- **Tools needed**: `dd`, `sgdisk`, `mformat`, `mcopy`
- **Impact**: Required to boot meniOS on real hardware
- **Estimated effort**: 1-2 days
- **Priority**: **Critical** - Without this, can't test on real hardware

#### **Serial Console Support** ⛳ **TODO** (No existing issue)
- **Scope**:
  - Enable serial output for debugging on real hardware
  - Configure COM1/COM2 ports
  - Early boot messages via serial
- **Current state**: Serial output exists for QEMU (`-serial file:com1.log`)
- **Impact**: Essential for debugging boot failures on real hardware
- **Estimated effort**: 1 week
- **Priority**: **High** - Critical for debugging

### **Phase 2: Storage & File System** ✅ **COMPLETE**

#### **SATA/AHCI Driver** ✅ **COMPLETE** (Issues #62, #114)
- ✅ Works in QEMU with AHCI controller
- ✅ DMA support for performance
- **Compatibility**: Should work on all three laptops (all have SATA)
- **Real hardware test needed**: Verify on physical SATA controllers

#### **FAT32 File System** ✅ **COMPLETE** (Issues #64, #189)
- ✅ Read support
- ✅ Write support (#291, #292, #293)
- **Compatibility**: FAT32 is universal, will work everywhere
- **Real hardware test needed**: Verify write operations on real HDD/SSD

#### **VFS Layer** ✅ **COMPLETE** (Issue #65)
- ✅ Generic VFS with mount points
- ✅ Block cache and streaming I/O (#294-#298)
- **Status**: Production ready

### **Phase 3: Input Devices**

#### **PS/2 Keyboard** ✅ **PARTIAL** (Issue #32)
- ✅ Basic ASCII key-down events work
- ❌ **Missing**: Scan codes, key-up events, extended keys (#302)
- **Compatibility**:
  - ✅ ACER Extensa 5620: Native PS/2 keyboard
  - ❌ MacBook White: USB-only, requires USB HID driver
  - ✅ Dell Latitude E6420/E6430: PS/2 emulation via BIOS
- **Priority**: **High** - Need full keyboard support for shell interaction

#### **PS/2 Mouse** ⛳ **TODO** (Issue #143 - Mouse driver)
- **Scope**:
  - PS/2 mouse protocol implementation
  - Trackpad support (may work as PS/2 mouse)
  - Scroll wheel support
- **Compatibility**:
  - ✅ ACER Extensa 5620: Native PS/2 trackpad
  - ❌ MacBook White: USB trackpad
  - ✅ Dell Latitude: PS/2 emulation
- **Impact**: Required for GUI applications and Doom
- **Estimated effort**: 2-3 weeks
- **Priority**: **Medium** - Nice to have but not critical for early testing

#### **USB Host Controller** ⛳ **TODO** (No existing issue)
- **Scope**:
  - EHCI driver (USB 2.0) - priority for MacBook
  - xHCI driver (USB 3.0) - for Dell E6430
  - UHCI/OHCI (USB 1.1) - legacy support
- **Compatibility**:
  - **CRITICAL for MacBook White**: All input is USB-only
  - Dell E6430: Has USB 3.0 ports
  - ACER Extensa: Has USB 2.0 ports
- **Impact**: **CRITICAL for MacBook**, blocks all input/output
- **Estimated effort**: 8-12 weeks (complex driver)
- **Priority**: **Critical** for MacBook support

#### **USB HID Driver** ⛳ **TODO** (No existing issue)
- **Scope**:
  - USB keyboard driver
  - USB mouse/trackpad driver
  - HID report descriptor parsing
- **Dependencies**: USB host controller
- **Impact**: Required for MacBook keyboard/mouse
- **Estimated effort**: 4-6 weeks
- **Priority**: **Critical** for MacBook support

### **Phase 4: Graphics**

#### **Pixel-Addressable Framebuffer** ⛳ **TODO** (Issue #301)
- **Status**: `/dev/fb0` exists but only supports text mode
- **Scope**:
  - mmap support for direct VRAM access
  - FBIOGET_VSCREENINFO ioctl for geometry
  - Expose width, height, pitch, pixel format
- **Compatibility**: Universal (all laptops have framebuffer)
- **Impact**: Required for Doom and graphical applications
- **Estimated effort**: 2-3 weeks
- **Priority**: **High** - Already tracked in Doom milestone

#### **Intel GMA Graphics Driver** ⛳ **TODO** (No existing issue)
- **Scope**:
  - Mode setting for GMA 965/X3100 (ACER, early MacBook)
  - Mode setting for Intel HD Graphics 3000/4000 (Dell)
  - Hardware acceleration (optional, future)
- **Compatibility**:
  - ✅ ACER Extensa 5620: GMA X3100
  - ✅ MacBook White (Early 2008): GMA X3100
  - ✅ Dell Latitude E6420/E6430: Intel HD Graphics 3000/4000
- **Impact**: Better graphics performance, native resolutions
- **Estimated effort**: 6-8 weeks (complex driver)
- **Priority**: **Medium** - Framebuffer mode sufficient for early testing

#### **NVIDIA Graphics Driver** ⛳ **TODO** (No existing issue)
- **Scope**:
  - Mode setting for GeForce 9400M/320M (MacBook)
  - Mode setting for NVS 4200M (Dell E6420 with discrete GPU)
  - Nouveau-style open source driver
- **Compatibility**:
  - MacBook White (Late 2008+): GeForce 9400M/320M
  - Dell Latitude E6420: NVS 4200M (optional discrete GPU)
- **Impact**: Support for MacBooks with NVIDIA graphics
- **Estimated effort**: 8-12 weeks (very complex)
- **Priority**: **Low** - Can use integrated graphics or framebuffer mode

### **Phase 5: Network**

#### **Intel Gigabit Ethernet** ⛳ **TODO** (No existing issue)
- **Scope**:
  - e1000/e1000e driver for Intel NICs
  - Link detection, autonegotiation
  - Basic packet TX/RX
- **Compatibility**:
  - Dell Latitude E6420/E6430: Intel Gigabit Ethernet
- **Impact**: Wired network access for testing, package downloads
- **Estimated effort**: 4-6 weeks
- **Priority**: **Medium** - Useful but not critical for boot testing

#### **WiFi Drivers** ⛳ **TODO** (No existing issue)
- **Scope**:
  - Intel WiFi (iwlwifi-style) for Dell
  - Broadcom WiFi (b43/brcmsmac) for MacBook
  - Intel 3945ABG for ACER Extensa
  - WPA2 supplicant
- **Compatibility**:
  - ACER Extensa 5620: Intel 3945ABG
  - MacBook White: Broadcom BCM4321/BCM4322
  - Dell Latitude: Intel WiFi Link
- **Impact**: Wireless network access
- **Estimated effort**: 12-16 weeks (very complex, requires firmware)
- **Priority**: **Low** - Long-term goal, wired ethernet sufficient

### **Phase 6: Power Management**

#### **ACPI Sleep States** ⛳ **TODO** (No existing issue)
- **Scope**:
  - S3 (Suspend to RAM)
  - S4 (Hibernate to disk) - related to #254-#258
  - S5 (Soft power off)
- **Dependencies**: uACPI integration (already exists)
- **Impact**: Laptop power management, battery life
- **Estimated effort**: 6-8 weeks
- **Priority**: **Medium** - Nice to have for laptop usage

#### **CPU Frequency Scaling** ⛳ **TODO** (No existing issue)
- **Scope**:
  - Intel SpeedStep/Turbo Boost
  - ACPI P-states
  - CPU governor (ondemand, performance, powersave)
- **Impact**: Better battery life, thermal management
- **Estimated effort**: 4-6 weeks
- **Priority**: **Low** - Can run at full speed for testing

#### **Battery Monitoring** ⛳ **TODO** (No existing issue)
- **Scope**:
  - ACPI battery status (BAT0/BAT1)
  - AC adapter detection
  - Expose via `/sys/class/power_supply` or similar
- **Impact**: Show battery percentage, charging status
- **Estimated effort**: 2-3 weeks
- **Priority**: **Low** - Can run on AC power for testing

### **Phase 7: Audio**

#### **Intel HD Audio** ⛳ **TODO** (Issue #33 - Audio subsystem)
- **Scope**:
  - HD Audio (Azalia) controller driver
  - Codec detection and configuration
  - PCM playback
- **Compatibility**: All three laptops use Intel HD Audio or compatible
- **Impact**: Sound output for Doom and applications
- **Estimated effort**: 8-12 weeks (complex driver)
- **Priority**: **Medium** - Required for full Doom experience

### **Phase 8: Laptop-Specific Features**

#### **Lid Switch & Function Keys** ⛳ **TODO** (No existing issue)
- **Scope**:
  - ACPI lid switch event
  - Brightness control (Fn+F5/F6)
  - Volume control (Fn+F7/F8)
  - Wireless toggle (Fn+F2)
- **Impact**: Better laptop integration
- **Estimated effort**: 3-4 weeks
- **Priority**: **Low** - Nice to have but not essential

#### **Backlight Control** ⛳ **TODO** (No existing issue)
- **Scope**:
  - LCD backlight PWM control
  - ACPI or GPU-based brightness adjustment
- **Impact**: Adjust screen brightness
- **Estimated effort**: 2-3 weeks
- **Priority**: **Low** - Can use default brightness

## 🎯 **Critical Path to First Boot**

The **minimum viable path** to boot meniOS on real hardware:

### **Week 1-2: Bootable USB Creation**
1. **Create USB installer** (No issue yet)
   - Build bootable USB stick with Limine
   - Include kernel and essential binaries
   - Test USB boot process

2. **Test BIOS boot path**
   - Verify Limine BIOS boot on ACER Extensa
   - Verify legacy CSM boot on Dell Latitude

3. **Test UEFI boot path**
   - Verify UEFI boot on MacBook White
   - Verify UEFI boot on Dell Latitude

### **Week 3-4: First Real Hardware Boot**
4. **Serial console debugging** (No issue yet)
   - Enable early serial output
   - Debug boot failures on real hardware
   - Identify hardware-specific issues

5. **AHCI/SATA validation**
   - Test disk detection on real SATA controllers
   - Verify block I/O on physical drives
   - Test FAT32 read/write on real hardware

### **Week 5-6: Input & Display**
6. **PS/2 keyboard validation**
   - Test on ACER Extensa (native PS/2)
   - Test on Dell Latitude (PS/2 emulation)
   - Fix any real-hardware quirks

7. **USB host controller** (MacBook only)
   - Implement basic EHCI driver
   - Detect USB keyboard on MacBook
   - Basic USB HID input

8. **Framebuffer validation**
   - Verify framebuffer mode on all laptops
   - Test different resolutions
   - Pixel-addressable mmap (#301)

### **Success Criteria**

meniOS will be "real hardware ready" when:
- ✅ Boots from USB on all three target laptops
- ✅ Kernel starts and reaches userland init
- ✅ Serial console shows boot messages
- ✅ Can mount and read from USB stick or internal HDD
- ✅ Keyboard input works (PS/2 on ACER/Dell, USB on MacBook)
- ✅ Framebuffer console displays text
- ✅ Shell (mosh) is interactive and usable
- ✅ Can run basic `/bin` utilities

### **Stretch Goals**

- ✅ Mouse/trackpad input works
- ✅ Network (Ethernet) connectivity
- ✅ Pixel graphics mode (Doom-ready)
- ✅ Audio output
- ✅ Power management (battery, sleep)

## 📋 **Hardware Compatibility Matrix**

| Feature | ACER Extensa 5620 | MacBook White | Dell Latitude E6420/E6430 | Status |
|---------|-------------------|---------------|---------------------------|--------|
| **Boot (BIOS)** | ✅ Native | ❌ N/A | ✅ CSM | ✅ Limine supports |
| **Boot (UEFI)** | ❌ N/A | ✅ 32/64-bit EFI | ✅ UEFI | ✅ Limine supports |
| **CPU (x86-64)** | ✅ Core 2 Duo | ✅ Core 2 Duo | ✅ Core i5/i7 | ✅ Supported |
| **SATA/AHCI** | ✅ 200GB HDD | ✅ HDD/SSD | ✅ HDD/SSD | ✅ Driver exists |
| **PS/2 Keyboard** | ✅ Native | ❌ USB only | ✅ Emulated | 🚧 Partial (#302) |
| **USB Keyboard** | 🔌 External only | ✅ Internal | 🔌 External | ❌ No USB driver |
| **PS/2 Mouse** | ✅ Trackpad | ❌ USB only | ✅ Emulated | ❌ No driver (#143) |
| **USB Mouse** | 🔌 External | ✅ Trackpad | 🔌 External | ❌ No USB driver |
| **Framebuffer** | ✅ GMA X3100 | ✅ GMA/GeForce | ✅ Intel HD | ✅ Text mode, 🚧 Pixel (#301) |
| **Ethernet** | ❌ None | ❌ None | ✅ Intel GbE | ❌ No driver |
| **WiFi** | 🔌 Intel 3945 | 🔌 Broadcom | 🔌 Intel | ❌ No driver |
| **Audio** | 🔊 HD Audio | 🔊 HD Audio | 🔊 HD Audio | ❌ No driver (#33) |

**Legend:**
- ✅ = Hardware exists and driver works
- 🚧 = Hardware exists, driver partial
- ❌ = Hardware exists, no driver
- 🔌 = Hardware exists, low priority
- 🔊 = Hardware exists, medium priority

## 🚧 **Blockers & Challenges**

### **Critical Blockers**

1. **MacBook USB Requirement** 🔴 **CRITICAL**
   - **Problem**: MacBook has no PS/2 keyboard, only USB
   - **Impact**: Cannot use MacBook without USB drivers
   - **Solution**: Implement EHCI USB host + USB HID driver
   - **Estimated effort**: 12-16 weeks
   - **Workaround**: Start with ACER/Dell (PS/2 available)

2. **USB Installer Creation** 🔴 **CRITICAL**
   - **Problem**: No documented process for creating bootable USB
   - **Impact**: Cannot boot on real hardware at all
   - **Solution**: Document USB creation process, test on all laptops
   - **Estimated effort**: 1-2 weeks
   - **Priority**: Must do first

### **High-Priority Blockers**

3. **Real Hardware Debugging**
   - **Problem**: No serial port on modern laptops
   - **Impact**: Hard to debug boot failures
   - **Solution**: USB-to-serial adapter or early printk to framebuffer
   - **Workaround**: Use QEMU for initial development

4. **Firmware Quirks**
   - **Problem**: Real BIOS/UEFI may behave differently than QEMU
   - **Impact**: Boot failures, crashes, hangs
   - **Solution**: Extensive testing, handle edge cases
   - **Approach**: Start with one laptop, expand to others

### **Medium-Priority Challenges**

5. **Display Resolution**
   - **Problem**: Each laptop has different native resolution
   - **Current**: Limine sets up framebuffer, but kernel doesn't query properly
   - **Solution**: Implement EDID parsing, mode setting

6. **Power Management**
   - **Problem**: Laptops may enter sleep states unexpectedly
   - **Solution**: Implement ACPI power management
   - **Workaround**: Disable sleep in BIOS settings

## 🎓 **Why This Matters**

### **For meniOS Development**
- **Validation**: Proves the OS works beyond emulators
- **Performance**: Real hardware exposes bottlenecks and bugs
- **Compatibility**: Tests against diverse hardware configurations
- **Credibility**: Running on real hardware is a major milestone

### **For Educational Value**
- Demonstrates real-world OS deployment
- Shows bootloader integration and disk imaging
- Illustrates hardware driver development
- Proves cross-platform compatibility

### **For Community**
- **Accessibility**: Old laptops are cheap and available
- **Sustainability**: Repurposes e-waste hardware
- **Testing**: Community can help test on various hardware
- **Contributions**: More testers = more bug reports = better OS

## 🔍 **Success Metrics**

### **Phase 1 Success: Boot & Display** (2-4 weeks)
- ✅ Boots from USB on at least one laptop
- ✅ Serial console shows kernel messages
- ✅ Framebuffer console displays text
- ✅ Kernel reaches userland init

### **Phase 2 Success: Interactive Shell** (6-8 weeks)
- ✅ PS/2 keyboard input works (ACER or Dell)
- ✅ USB keyboard works (MacBook)
- ✅ Shell is usable and responsive
- ✅ Can navigate filesystem and run commands

### **Phase 3 Success: Storage Access** (8-10 weeks)
- ✅ Can read files from USB stick or internal drive
- ✅ Can write files to disk
- ✅ FAT32 filesystem stable on real hardware
- ✅ No data corruption or crashes

### **Phase 4 Success: Full Desktop** (12-16 weeks)
- ✅ Mouse/trackpad input functional
- ✅ Pixel-addressable framebuffer (Doom-ready)
- ✅ Network access (Ethernet or WiFi)
- ✅ Audio output working

## 📚 **References**

### **Hardware Documentation**
- [Intel Core 2 Duo Datasheets](https://www.intel.com/content/www/us/en/products/docs/processors/core/core-technical-resources.html)
- [Intel 965 Express Chipset Family Datasheet](https://www.intel.com/content/dam/doc/datasheet/965-express-chipset-family-datasheet.pdf)
- [Intel HD Graphics Programming Guide](https://www.intel.com/content/www/us/en/docs/graphics-for-linux/developer-reference/1-0/overview.html)
- [AHCI Specification](https://www.intel.com/content/www/us/en/io/serial-ata/serial-ata-ahci-spec-rev1-3-1.html)
- [USB 2.0 Specification (EHCI)](https://www.usb.org/document-library/ehci-specification-usb-20)

### **Bootloader**
- [Limine Protocol Specification](https://github.com/limine-bootloader/limine/blob/trunk/PROTOCOL.md)
- [Limine Configuration](https://github.com/limine-bootloader/limine/blob/trunk/CONFIG.md)

### **Similar Projects**
- **SerenityOS**: Boots on ThinkPad X200, T420
- **ToaruOS**: Boots on various laptops with QEMU-like setup
- **Sortix**: Real hardware support with USB drivers
- **Haiku**: Extensive laptop hardware support

### **Driver Development**
- [OSDev Wiki - USB](https://wiki.osdev.org/USB)
- [OSDev Wiki - EHCI](https://wiki.osdev.org/EHCI)
- [OSDev Wiki - Intel HD Graphics](https://wiki.osdev.org/Intel_HD_Graphics)
- [Linux kernel drivers](https://github.com/torvalds/linux/tree/master/drivers) - Reference implementation

## 🚀 **Getting Started**

### **For Contributors**

Priority order for maximum impact:

1. **🔴 CRITICAL - USB Installer**
   - Document USB creation process
   - Test Limine boot on physical hardware
   - Validate on all three target laptops

2. **🔴 CRITICAL - Serial Console**
   - Enable early serial debugging
   - Handle real UART hardware
   - Provide fallback for laptops without serial

3. **🟠 HIGH - USB Host Controller**
   - EHCI driver for MacBook support
   - USB HID keyboard/mouse
   - Critical for MacBook usability

4. **🟡 MEDIUM - Pixel Framebuffer** (#301)
   - mmap support for VRAM
   - Already tracked in Doom milestone
   - Enables graphical applications

5. **🟡 MEDIUM - Network Driver**
   - Intel Gigabit Ethernet for Dell
   - Enables remote testing and package downloads
   - Nice to have for development

### **Testing Strategy**

1. **Incremental Hardware**: Start with ACER (simplest), then Dell, finally MacBook (hardest)
2. **Serial Debugging**: Always use serial console for early boot debugging
3. **USB Boot**: Test boot from USB before attempting HDD installation
4. **Minimal Changes**: One driver at a time, validate on QEMU first
5. **Community Testing**: Encourage users to test on their own hardware

## 🎉 **Long-Term Vision**

When complete, meniOS will:

1. Boot natively on commodity x86-64 laptops from 2008-2012 era
2. Provide an interactive shell with keyboard input
3. Support storage access via SATA/AHCI
4. Display graphics via framebuffer
5. Eventually support modern features (USB 3.0, NVMe, etc.)

This transforms meniOS from a **virtual machine project** into a **real operating system** that can run on physical hardware, opening doors to:
- Community testing on diverse hardware
- Performance optimization for real workloads
- Driver development for modern peripherals
- Eventually: daily-driver potential for enthusiasts

---

**Last Updated**: 2025-10-17
**Next Steps**: Create USB installer, test first boot on ACER Extensa 5620
**Target Milestone**: First real hardware boot within 4-6 weeks

---

📝 **Note**: This roadmap focuses on laptops from 2008-2012 era as they represent a good balance of:
- **Availability**: Cheap and common in second-hand market
- **Simplicity**: Less complex than modern hardware
- **Capability**: Still powerful enough for development and testing
- **Documentation**: Well-documented hardware with Linux driver references
