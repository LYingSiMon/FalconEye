/*
*	Module Name:
*		entry.cpp
*
*	Abstract:
*		Sample driver that implements infinity hook to detour
*		system calls.
*
*	Authors:
*		Nick Peterson <everdox@gmail.com> | http://everdox.net/
*
*	Special thanks to Nemanja (Nemi) Mulasmajic <nm@triplefault.io>
*	for his help with the POC.
*
*/

#include "stdafx.h"
#include "entry.h"
#include "..\libinfinityhook\infinityhook.h"
#include "Syscalls.h"
#include "FloatingCodeDetect.h"

static wchar_t IfhMagicFileName[] = L"ifh--";

static UNICODE_STRING StringNtCreateFile = RTL_CONSTANT_STRING(L"NtCreateFile");
static UNICODE_STRING StringNtNtWriteVirtualMemory = RTL_CONSTANT_STRING(L"NtWriteVirtualMemory");

static NtCreateFile_t OriginalNtCreateFile = NULL;
static NtWriteVirtualMemory_t OriginalNtNtWriteVirtualMemory = NULL;

BOOLEAN g_DbgPrintSyscall = FALSE;
/*
*	The entry point of the driver. Initializes infinity hook and
*	sets up the driver's unload routine so that it can be gracefully 
*	turned off.
*/
extern "C" NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject, 
	_In_ PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	// Figure out when we built this last for debugging purposes.
	kprintf("[+] infinityhook: Loaded.\n");
	
	// Let the driver be unloaded gracefully. This also turns off 
	// infinity hook.
	DriverObject->DriverUnload = DriverUnload;

	OriginalNtCreateFile = (NtCreateFile_t)MmGetSystemRoutineAddress(&StringNtCreateFile);
	if (!OriginalNtCreateFile)
	{
		kprintf("[-] infinityhook: Failed to locate export: %wZ.\n", StringNtCreateFile);
		return STATUS_ENTRYPOINT_NOT_FOUND;
	}
	OriginalNtNtWriteVirtualMemory = (NtWriteVirtualMemory_t)MmGetSystemRoutineAddress(&StringNtNtWriteVirtualMemory);

	TestMemImageByAddress();
	FindNtBase(OriginalNtCreateFile);
	GetSyscallAddresses();

	// Initialize infinity hook. Each system call will be redirected
	// to our syscall stub.
	NTSTATUS Status = IfhInitialize(SyscallStub);
	if (!NT_SUCCESS(Status))
	{
		kprintf("[-] infinityhook: Failed to initialize with status: 0x%lx.\n", Status);
	}

	return Status;
}

/*
*	Turns off infinity hook.
*/
void DriverUnload(
	_In_ PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);

	// Unload infinity hook gracefully.
	IfhRelease();

	kprintf("\n[!] infinityhook: Unloading... BYE!\n");
}

/*
*	For each usermode syscall, this stub will be invoked.
*/
void __fastcall SyscallStub(
	_In_ unsigned int SystemCallIndex, 
	_Inout_ void** SystemCallFunction)
{
	if (g_DbgPrintSyscall) {
		kprintf("[+] infinityhook: SYSCALL %lu: 0x%p [stack: 0x%p].\n", SystemCallIndex, *SystemCallFunction, SystemCallFunction);
	}

	void** DetourAddress = (void **)UpdateSystemCallFunction(*SystemCallFunction);
	if (DetourAddress) {
		*SystemCallFunction = DetourAddress;
	}
}

/*
*	This function is invoked instead of nt!NtCreateFile. It will 
*	attempt to filter a file by the "magic" file name.
*/
NTSTATUS DetourNtCreateFile(
	_Out_ PHANDLE FileHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_In_opt_ PLARGE_INTEGER AllocationSize,
	_In_ ULONG FileAttributes,
	_In_ ULONG ShareAccess,
	_In_ ULONG CreateDisposition,
	_In_ ULONG CreateOptions,
	_In_reads_bytes_opt_(EaLength) PVOID EaBuffer,
	_In_ ULONG EaLength)
{
	// We're going to filter for our "magic" file name.
	if (ObjectAttributes &&
		ObjectAttributes->ObjectName && 
		ObjectAttributes->ObjectName->Buffer)
	{
		// Unicode strings aren't guaranteed to be NULL terminated so
		// we allocate a copy that is.
		PWCHAR ObjectName = (PWCHAR)ExAllocatePool(NonPagedPool, ObjectAttributes->ObjectName->Length + sizeof(wchar_t));
		if (ObjectName)
		{
			memset(ObjectName, 0, ObjectAttributes->ObjectName->Length + sizeof(wchar_t));
			memcpy(ObjectName, ObjectAttributes->ObjectName->Buffer, ObjectAttributes->ObjectName->Length);
		
			// Does it contain our special file name?
			if (wcsstr(ObjectName, IfhMagicFileName))
			{
				kprintf("[+] infinityhook: Denying access to file: %wZ.\n", ObjectAttributes->ObjectName);
				ExFreePool(ObjectName);
				return STATUS_ACCESS_DENIED;
			}

			ExFreePool(ObjectName);
		}
	}

	return OriginalNtCreateFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);
}
