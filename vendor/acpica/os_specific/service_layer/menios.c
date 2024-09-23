/******************************************************************************
 *
 * Module Name: oszephyr - Zephyr OSL
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2024, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code. No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include "acpi.h"
#include "aclocal.h"
#include "acapps.h"
#include "accommon.h"
#include "actypes.h"
#include <boot/limine.h>
#include <kernel/acpi.h>
#include <kernel/proc.h>
#include <kernel/kernel.h>
#include <kernel/serial.h>
#include <stdlib.h>

static BOOLEAN EnDbgPrint;

static volatile struct limine_rsdp_request rsdp_request = {
  .id = LIMINE_RSDP_REQUEST,
  .revision = 0
};

ACPI_STATUS AcpiOsInitialize() { // 559
  serial_printf("ACPI: Initializing...\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsTerminate() { // 557
  serial_printf("ACPI: Terminating...\n");
  return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
  if(rsdp_request.response == NULL) {
    printf(">>> Error loading device table.\n");
    serial_printf("AcpiOsGetRootPointer: Error loading device table.\n");
    hcf();
  }

  serial_printf("AcpiOsGetRootPointer: %p\n", rsdp_request.response->address);
  ACPI_PHYSICAL_ADDRESS ret = (ACPI_PHYSICAL_ADDRESS)rsdp_request.response->address;
	AcpiFindRootPointer(&ret);
	return ret;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *predefined_object, ACPI_STRING *new_value) {
  serial_printf("AcpiOsPredefinedOverride: Not implemented.\n");
  new_value = NULL;
  return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *existing_table, ACPI_TABLE_HEADER **new_table) {
  serial_printf("AcpiOsTableOverride: Not implemented.\n");
  *new_table = NULL;
  return AE_OK;
}

void* AcpiOsAllocate(ACPI_SIZE size) {
  serial_printf("AcpiOsAllocate: %d\n", size);
  return malloc(size);
}

void AcpiOsFree(void *addr) {
  serial_printf("AcpiOsFree: %p\n", addr);
  free(addr);
}

/******************************************************************************
 *
 * FUNCTION:    AcpiEnableDbgPrint
 *
 * PARAMETERS:  en, 	            - Enable/Disable debug print
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output
 *
 *****************************************************************************/

void AcpiEnableDbgPrint (bool enable) {
  serial_printf("AcpiEnableDbgPrint: %d\n", enable);
  EnDbgPrint = enable;
}

/******************************************************************************
 *
 * FUNCTION:    AcpiOsPrintf
 *
 * PARAMETERS:  Fmt, ...            - Standard printf format
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted output
 *
 *****************************************************************************/
void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf (const char *fmt, ...) {

  serial_printf("AcpiOsPrintf: %s\n", fmt);

  va_list args;
  va_start(args, fmt);

  if(EnDbgPrint) {
    vprintf(fmt, args);
  }

  va_end(args);
}

void AcpiOsVprintf(const char *fmt, va_list args) {
  
  serial_printf("AcpiOsVprintf: %s\n", fmt);

  if(EnDbgPrint) {
    vprintf(fmt, args);
  }
}

ACPI_THREAD_ID AcpiOsGetThreadId() {
  serial_printf("AcpiOsGetThreadId: Not implemented.\n");
  return current->pid;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK function, void *context) {
  serial_printf("AcpiOsExecute: Not implemented.\n");
  
  function(context);
  return AE_OK;
}

void AcpiOsSleep(UINT64 Milliseconds) {
  serial_printf("AcpiOsSleep: Not implemented.\n");
}

void AcpiOsStall(UINT32 Microseconds) {
  serial_printf("AcpiOsStall: Not implemented.\n");
}

#if(ACPI_MUTEX_TYPE != ACPI_BINARY_SEMAPHORE)
ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *OutHandle) {
  serial_printf("AcpiOsCreateMutex: Not implemented.\n");
  return AE_OK;
}

void AcpiOsDeleteMutex(ACPI_MUTEX Handle) {
  serial_printf("AcpiOsDeleteMutex: Not implemented.\n");
}

ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX handle, UINT16 timeout) {
  serial_printf("AcpiOsAcquireMutex: Not implemented.\n");
  return AE_OK;
}

void AcpiOsReleaseMutex(ACPI_MUTEX Handle) {
  serial_printf("AcpiOsReleaseMutex: Not implemented.\n");
}
#endif

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 max_units, UINT32 initial_units, ACPI_SEMAPHORE *out_handle) {
  serial_printf("AcpiOsCreateSemaphore: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle){
  serial_printf("AcpiOsDeleteSemaphore: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE handle, UINT32 units, UINT16 timeout) {
  serial_printf("AcpiOsWaitSemaphore: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE handle, UINT32 units) {
  serial_printf("AcpiOsSignalSemaphore: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *out_handle) {
  serial_printf("AcpiOsCreateLock: Not implemented.\n");
  return AE_OK;
}

void AcpiOsDeleteLock(ACPI_HANDLE handle) {
  serial_printf("AcpiOsDeleteLock: Not implemented.\n");
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) {
  serial_printf("AcpiOsAcquireLock: Not implemented.\n");
  return AE_OK;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK handle, ACPI_CPU_FLAGS flags) {
  serial_printf("AcpiOsReleaseLock: Not implemented.\n");
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 sleep_state, UINT32 rega_value, UINT32 regb_value) {
  serial_printf("AcpiOsEnterSleep: Not implemented.\n");
  return AE_OK;
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length) {
  serial_printf("AcpiOsMapMemory: Not implemented.\n");
  return NULL;
}

void AcpiOsUnmapMemory(void *where, ACPI_SIZE length) {
  serial_printf("AcpiOsUnmapMemory: Not implemented.\n");
}

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void *Context) {
  serial_printf("AcpiOsInstallInterruptHandler: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler) {
  serial_printf("AcpiOsRemoveInterruptHandler: Not implemented.\n");
  return AE_OK;
}

void AcpiOsWaitEventsComplete (void) {
  serial_printf("AcpiOsWaitEventsComplete: Not implemented.\n");
}

UINT64 AcpiOsGetTimer(void) {
  serial_printf("AcpiOsGetTimer: Not implemented.\n");
  return 0;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
  serial_printf("AcpiOsWritePort: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width) {
  serial_printf("AcpiOsReadMemory: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width) {
  serial_printf("AcpiOsWriteMemory: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width) {
  serial_printf("AcpiOsReadPort: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info){
  serial_printf("AcpiOsSignal: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, 
                                        ACPI_PHYSICAL_ADDRESS *NewAddress, 
                                        UINT32 *NewTableLength) {
  serial_printf("AcpiOsPhysicalTableOverride: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value, UINT32 Width) {
  serial_printf("AcpiOsWritePciConfiguration: Not implemented.\n");
  return AE_OK;
}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value, UINT32 Width) {
  serial_printf("AcpiOsReadPciConfiguration: Not implemented.\n");
  return AE_OK;
}