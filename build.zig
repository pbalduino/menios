const std = @import("std");
const Target = std.Target;
const CrossTarget = std.zig.CrossTarget;

pub fn build(b: *std.Build) void {
    // const nasm = b.addSystemCommand(&.{"nasm"});
    // nasm.addArgs(&.{ "-Wall", "-f elf64" });
    // nasm.addFileArg(b.path("./src/kernel/lgdt.s"));

    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_arch = .x86_64,
            .os_tag = .freestanding,
            .abi = Target.Abi.none,
        },
    });
    const optimize = b.standardOptimizeOption(.{});

    const kernel = b.addExecutable(.{
        .name = "menios",
        .root_source_file = b.path("src/kernel/main.zig"),
        .target = target,
        .optimize = optimize,
        .pic = false,
        .link_libc = false,
        .version = .{ .major = 0, .minor = 0, .patch = 3 },
    });

    const cfiles = &[_][]const u8{ "src/kernel/console.c", "src/kernel/file.c", "src/kernel/framebuffer.c", "src/kernel/gdt.c", "src/kernel/halt.c", "src/kernel/idt.c", "src/kernel/panic.c", "src/kernel/serial.c", "src/kernel/acpi/acpi.c", "src/kernel/acpi/acpica.c", "src/kernel/mem/mem_utils.c", "src/kernel/mem/kmalloc.c", "src/kernel/mem/mem.c", "src/kernel/mem/pmm.c", "src/kernel/mem/sbrk.c", "src/kernel/proc/proc.c", "src/libc/ctype.c", "src/libc/sprintf.c", "src/libc/stdlib.c", "src/libc/string.c", "vendor/acpica/tables/tbutils.c", "vendor/acpica/tables/tbxface.c", "vendor/acpica/tables/tbinstal.c", "vendor/acpica/tables/tbfadt.c", "vendor/acpica/tables/tbxfload.c", "vendor/acpica/tables/tbdata.c", "vendor/acpica/tables/tbfind.c", "vendor/acpica/tables/tbprint.c", "vendor/acpica/tables/tbxfroot.c", "vendor/acpica/hardware/hwtimer.c", "vendor/acpica/hardware/hwvalid.c", "vendor/acpica/hardware/hwregs.c", "vendor/acpica/hardware/hwpci.c", "vendor/acpica/hardware/hwgpe.c", "vendor/acpica/hardware/hwsleep.c", "vendor/acpica/hardware/hwesleep.c", "vendor/acpica/hardware/hwxface.c", "vendor/acpica/hardware/hwxfsleep.c", "vendor/acpica/hardware/hwacpi.c", "vendor/acpica/dispatcher/dsobject.c", "vendor/acpica/dispatcher/dsargs.c", "vendor/acpica/dispatcher/dsdebug.c", "vendor/acpica/dispatcher/dsutils.c", "vendor/acpica/dispatcher/dswload.c", "vendor/acpica/dispatcher/dsinit.c", "vendor/acpica/dispatcher/dsfield.c", "vendor/acpica/dispatcher/dscontrol.c", "vendor/acpica/dispatcher/dsopcode.c", "vendor/acpica/dispatcher/dswload2.c", "vendor/acpica/dispatcher/dsmthdat.c", "vendor/acpica/dispatcher/dswstate.c", "vendor/acpica/dispatcher/dswscope.c", "vendor/acpica/dispatcher/dswexec.c", "vendor/acpica/dispatcher/dspkginit.c", "vendor/acpica/dispatcher/dsmethod.c", "vendor/acpica/namespace/nsxfobj.c", "vendor/acpica/namespace/nsinit.c", "vendor/acpica/namespace/nsaccess.c", "vendor/acpica/namespace/nsparse.c", "vendor/acpica/namespace/nsarguments.c", "vendor/acpica/namespace/nsobject.c", "vendor/acpica/namespace/nsxfname.c", "vendor/acpica/namespace/nsdumpdv.c", "vendor/acpica/namespace/nsrepair2.c", "vendor/acpica/namespace/nseval.c", "vendor/acpica/namespace/nssearch.c", "vendor/acpica/namespace/nspredef.c", "vendor/acpica/namespace/nsutils.c", "vendor/acpica/namespace/nsload.c", "vendor/acpica/namespace/nsrepair.c", "vendor/acpica/namespace/nsxfeval.c", "vendor/acpica/namespace/nsalloc.c", "vendor/acpica/namespace/nsnames.c", "vendor/acpica/namespace/nswalk.c", "vendor/acpica/namespace/nsprepkg.c", "vendor/acpica/namespace/nsdump.c", "vendor/acpica/namespace/nsconvert.c", "vendor/acpica/parser/psargs.c", "vendor/acpica/parser/psutils.c", "vendor/acpica/parser/psloop.c", "vendor/acpica/parser/psobject.c", "vendor/acpica/parser/psopcode.c", "vendor/acpica/parser/psxface.c", "vendor/acpica/parser/psparse.c", "vendor/acpica/parser/pswalk.c", "vendor/acpica/parser/psscope.c", "vendor/acpica/parser/psopinfo.c", "vendor/acpica/parser/pstree.c", "vendor/acpica/utilities/utids.c", "vendor/acpica/utilities/utdebug.c", "vendor/acpica/utilities/uterror.c", "vendor/acpica/utilities/utstring.c", "vendor/acpica/utilities/utdecode.c", "vendor/acpica/utilities/utpredef.c", "vendor/acpica/utilities/utuuid.c", "vendor/acpica/utilities/utmisc.c", "vendor/acpica/utilities/utxfmutex.c", "vendor/acpica/utilities/utcache.c", "vendor/acpica/utilities/utxface.c", "vendor/acpica/utilities/utlock.c", "vendor/acpica/utilities/utbuffer.c", "vendor/acpica/utilities/utnonansi.c", "vendor/acpica/utilities/utobject.c", "vendor/acpica/utilities/utresrc.c", "vendor/acpica/utilities/utcksum.c", "vendor/acpica/utilities/utdelete.c", "vendor/acpica/utilities/utxferror.c", "vendor/acpica/utilities/utascii.c", "vendor/acpica/utilities/uteval.c", "vendor/acpica/utilities/utxfinit.c", "vendor/acpica/utilities/utglobal.c", "vendor/acpica/utilities/uttrack.c", "vendor/acpica/utilities/utstrtoul64.c", "vendor/acpica/utilities/utprint.c", "vendor/acpica/utilities/utownerid.c", "vendor/acpica/utilities/utresdecode.c", "vendor/acpica/utilities/utclib.c", "vendor/acpica/utilities/utexcep.c", "vendor/acpica/utilities/utmath.c", "vendor/acpica/utilities/utstate.c", "vendor/acpica/utilities/utmutex.c", "vendor/acpica/utilities/utinit.c", "vendor/acpica/utilities/utaddress.c", "vendor/acpica/utilities/utosi.c", "vendor/acpica/utilities/utstrsuppt.c", "vendor/acpica/utilities/utalloc.c", "vendor/acpica/utilities/uthex.c", "vendor/acpica/utilities/utcopy.c", "vendor/acpica/os_specific/service_layer/menios.c", "vendor/acpica/executer/exregion.c", "vendor/acpica/executer/exsystem.c", "vendor/acpica/executer/exprep.c", "vendor/acpica/executer/exstore.c", "vendor/acpica/executer/exmutex.c", "vendor/acpica/executer/excreate.c", "vendor/acpica/executer/exresnte.c", "vendor/acpica/executer/exoparg3.c", "vendor/acpica/executer/exserial.c", "vendor/acpica/executer/exresop.c", "vendor/acpica/executer/exnames.c", "vendor/acpica/executer/exstoren.c", "vendor/acpica/executer/extrace.c", "vendor/acpica/executer/exoparg2.c", "vendor/acpica/executer/exdump.c", "vendor/acpica/executer/exoparg6.c", "vendor/acpica/executer/exfield.c", "vendor/acpica/executer/exstorob.c", "vendor/acpica/executer/exconcat.c", "vendor/acpica/executer/exoparg1.c", "vendor/acpica/executer/exconvrt.c", "vendor/acpica/executer/exmisc.c", "vendor/acpica/executer/exresolv.c", "vendor/acpica/executer/exutils.c", "vendor/acpica/executer/exfldio.c", "vendor/acpica/executer/exdebug.c", "vendor/acpica/executer/exconfig.c", "vendor/acpica/events/evglock.c", "vendor/acpica/events/evxfevnt.c", "vendor/acpica/events/evgpe.c", "vendor/acpica/events/evhandler.c", "vendor/acpica/events/evmisc.c", "vendor/acpica/events/evrgnini.c", "vendor/acpica/events/evxface.c", "vendor/acpica/events/evregion.c", "vendor/acpica/events/evgpeutil.c", "vendor/acpica/events/evgpeblk.c", "vendor/acpica/events/evxfgpe.c", "vendor/acpica/events/evevent.c", "vendor/acpica/events/evsci.c", "vendor/acpica/events/evgpeinit.c", "vendor/acpica/events/evxfregn.c" };
    const cflags = &[_][]const u8{ "-Wall", "-Wextra", "-Winline", "-Wfatal-errors", "-Wno-unused-parameter", "-static", "-std=gnu11", "-nostdinc", "-nostdlib", "-ffreestanding", "-fno-lto", "-fno-stack-check", "-fno-stack-protector", "-m64", "-march=x86-64", "-mno-80387", "-mno-mmx", "-mno-red-zone", "-mno-sse", "-mno-sse2", "-fPIC", "-DMENIOS_KERNEL" };

    kernel.addIncludePath(b.path("include"));

    kernel.addCSourceFiles(.{ .files = cfiles, .flags = cflags });

    kernel.addAssemblyFile(b.path("./src/kernel/lgdt.s"));
    kernel.addAssemblyFile(b.path("./src/kernel/lidt.s"));

    kernel.setLinkerScriptPath(b.path("./linker.ld"));

    b.installArtifact(kernel);
}
