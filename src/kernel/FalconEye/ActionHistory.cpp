#include <excpt.h>
#include "stdafx.h"
#include "NtDefs.h"
#include "ActionHistory.h"

NtWVMEntry* NtWVMBuffer = NULL;
SIZE_T      freeNtWVMIdx = 0;
ERESOURCE   NtWVMLock;

NtUnMVSEntry* NtUnMVSBuffer = NULL;
SIZE_T      freeNtUnMVSIdx = 0;
ERESOURCE   NtUnMVSLock;

NtSTEntry*  NtSTBuffer = NULL;
SIZE_T      freeNtSTIdx = 0;
ERESOURCE   NtSTLock;

NtUserSWLPEntry* NtUserSWLPBuffer = NULL;
SIZE_T      freeNtUserSWLPIdx = 0;
ERESOURCE   NtUserSWLPLock;

NtUserSPEntry* NtUserSPBuffer = NULL;
SIZE_T      freeNtUserSPIdx = 0;
ERESOURCE   NtUserSPLock;

extern "C" {
    NTSTATUS ReadWVMData(PVOID localBuffer, ULONG bufferSize, PCHAR targetBuffer);
}

ULONG       ActionHistoryTag = 0xfffe;

BOOLEAN InitNtWVMHistory()
{
    NtWVMBuffer = (NtWVMEntry*)ExAllocatePoolWithTag(
        NonPagedPool,
        NTWVM_BUFFER_SIZE * sizeof(NtWVMEntry),
        ActionHistoryTag
    );
    if (NULL == NtWVMBuffer) {
        return FALSE;
    }
    ExInitializeResourceLite(&NtWVMLock);
    return TRUE;
}

BOOLEAN InitNtUnMVSHistory()
{
    NtUnMVSBuffer = (NtUnMVSEntry*)ExAllocatePoolWithTag(
        NonPagedPool,
        NTUNMVS_BUFFER_SIZE * sizeof(NtUnMVSEntry),
        ActionHistoryTag
    );
    if (NULL == NtUnMVSBuffer) {
        return FALSE;
    }
    ExInitializeResourceLite(&NtUnMVSLock);
    return TRUE;
}

BOOLEAN InitNtSTHistory()
{
    NtSTBuffer = (NtSTEntry*)ExAllocatePoolWithTag(
        NonPagedPool,
        NTST_BUFFER_SIZE * sizeof(NtSTEntry),
        ActionHistoryTag
    );
    if (NULL == NtSTBuffer) {
        return FALSE;
    }
    ExInitializeResourceLite(&NtSTLock);
    return TRUE;
}

BOOLEAN InitNtUserSWLPHistory()
{
    NtUserSWLPBuffer = (NtUserSWLPEntry*)ExAllocatePoolWithTag(
        NonPagedPool,
        NTUSERSWLP_BUFFER_SIZE * sizeof(NtUserSWLPEntry),
        ActionHistoryTag
    );
    if (NULL == NtUserSWLPBuffer) {
        return FALSE;
    }
    ExInitializeResourceLite(&NtUserSWLPLock);
    return TRUE;
}

BOOLEAN InitNtUserSPHistory()
{
    NtUserSPBuffer = (NtUserSPEntry*)ExAllocatePoolWithTag(
        NonPagedPool,
        NTUSERSWLP_BUFFER_SIZE * sizeof(NtUserSPEntry),
        ActionHistoryTag
    );
    if (NULL == NtUserSWLPBuffer) {
        return FALSE;
    }
    ExInitializeResourceLite(&NtUserSWLPLock);
    return TRUE;
}


BOOLEAN InitActionHistory()
{
    InitNtWVMHistory();
    InitNtUnMVSHistory();
    InitNtSTHistory();
    InitNtUserSWLPHistory();
    InitNtUserSPHistory();
    return TRUE;
}

BOOLEAN AddNtWriteVirtualMemoryEntry(
    ULONG   callerPid,
    ULONG   targetPid,
    PVOID   targetAddr,
    PVOID   localBuffer,
    ULONG   bufferSize
)
{
    CHAR targetBuffer[NTWVM_DATA_COPY_SIZE] = { 0 };
    ReadWVMData(localBuffer, bufferSize, targetBuffer);
    if (FALSE == ExAcquireResourceExclusiveLite(&NtWVMLock, TRUE)) {
        return FALSE;
    }
    NtWVMBuffer[freeNtWVMIdx] = { callerPid, targetPid, targetAddr, localBuffer, bufferSize };
    RtlCopyMemory(NtWVMBuffer[freeNtWVMIdx].initialData, targetBuffer, NTWVM_DATA_COPY_SIZE);
    freeNtWVMIdx = (freeNtWVMIdx + 1) % NTWVM_BUFFER_SIZE;
    ExReleaseResourceLite(&NtWVMLock);
    return TRUE;
}

// Caller must deallocate NtWVMEntry
NtWVMEntry* FindNtWriteVirtualMemoryEntry(ULONG callerPid, ULONG targetPid)
{
    NtWVMEntry* entry = NULL;
    if (FALSE == ExAcquireResourceExclusiveLite(&NtWVMLock, TRUE)) {
        return FALSE;
    }
    entry = (NtWVMEntry*)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(NtWVMEntry),
        ActionHistoryTag);
    if (NULL == entry) {
        ExReleaseResourceLite(&NtWVMLock);
        return FALSE;
    }
    for (auto i = 0; i < NTWVM_BUFFER_SIZE; i++) {
        if (callerPid == NtWVMBuffer[i].callerPid && targetPid == NtWVMBuffer[i].targetPid) {
            RtlCopyMemory(entry, &NtWVMBuffer[i], sizeof(NtWVMEntry));
            break;
        }
    }
    ExReleaseResourceLite(&NtWVMLock);
    return entry;
}

BOOLEAN AddNtUnmapViewOfSectionEntry(
    ULONG   callerPid,
    ULONG   targetPid,
    PVOID   baseAddr
)
{
    if (FALSE == ExAcquireResourceExclusiveLite(&NtUnMVSLock, TRUE)) {
        return FALSE;
    }
    NtUnMVSBuffer[freeNtUnMVSIdx] = { callerPid, targetPid, baseAddr };
    freeNtUnMVSIdx = (freeNtUnMVSIdx + 1) % NTWVM_BUFFER_SIZE;
    ExReleaseResourceLite(&NtUnMVSLock);
    return TRUE;
}

// Caller must deallocate NtWVMEntry
NtUnMVSEntry* FindNtUnmapViewOfSectionEntry(ULONG callerPid, ULONG targetPid)
{
    NtUnMVSEntry* entry = NULL;
    if (FALSE == ExAcquireResourceExclusiveLite(&NtUnMVSLock, TRUE)) {
        return FALSE;
    }
    entry = (NtUnMVSEntry*)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(NtUnMVSEntry),
        ActionHistoryTag);
    if (NULL == entry) {
        ExReleaseResourceLite(&NtUnMVSLock);
        return FALSE;
    }
    for (auto i = 0; i < NTUNMVS_BUFFER_SIZE; i++) {
        if (callerPid == NtUnMVSBuffer[i].callerPid && targetPid == NtUnMVSBuffer[i].targetPid) {
            RtlCopyMemory(entry, &NtUnMVSBuffer[i], sizeof(NtUnMVSEntry));
            break;
        }
    }
    ExReleaseResourceLite(&NtUnMVSLock);
    return entry;
}

BOOLEAN AddNtSuspendThreadEntry(
    ULONG   callerPid,
    ULONG   targetPid,
    ULONG   targetTid
)
{
    if (FALSE == ExAcquireResourceExclusiveLite(&NtSTLock, TRUE)) {
        return FALSE;
    }
    NtSTBuffer[freeNtSTIdx] = { callerPid, targetPid, targetTid };
    freeNtSTIdx = (freeNtSTIdx + 1) % NTST_BUFFER_SIZE;
    ExReleaseResourceLite(&NtSTLock);
    return TRUE;
}

// Caller must deallocate NtWVMEntry
NtSTEntry* FindNtSuspendThreadEntry(ULONG callerPid, ULONG targetPid)
{
    NtSTEntry* entry = NULL;
    if (FALSE == ExAcquireResourceExclusiveLite(&NtSTLock, TRUE)) {
        return FALSE;
    }
    entry = (NtSTEntry*)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(NtSTEntry),
        ActionHistoryTag);
    if (NULL == entry) {
        ExReleaseResourceLite(&NtSTLock);
        return FALSE;
    }
    for (auto i = 0; i < NTST_BUFFER_SIZE; i++) {
        if (callerPid == NtSTBuffer[i].callerPid && targetPid == NtSTBuffer[i].targetPid) {
            RtlCopyMemory(entry, &NtSTBuffer[i], sizeof(NtSTEntry));
            break;
        }
    }
    ExReleaseResourceLite(&NtSTLock);
    return entry;
}

BOOLEAN AddNtUserSetWindowLongPtrEntry(
    HWND hWnd,
    DWORD Index,
    LONG_PTR NewValue
)
{
    if (FALSE == ExAcquireResourceExclusiveLite(&NtUserSWLPLock, TRUE)) {
        return FALSE;
    }
    NtUserSWLPBuffer[freeNtUserSWLPIdx] = { hWnd, Index, NewValue };
    freeNtUserSWLPIdx = (freeNtUserSWLPIdx + 1) % NTUSERSWLP_BUFFER_SIZE;
    ExReleaseResourceLite(&NtUserSWLPLock);
    return TRUE;
}

// Caller must deallocate NtUserSWLPEntry
NtUserSWLPEntry* FindNtUserSetWindowLongPtrEntry(HWND hWnd)
{
    NtUserSWLPEntry* entry = NULL;
    if (FALSE == ExAcquireResourceExclusiveLite(&NtUserSWLPLock, TRUE)) {
        return FALSE;
    }
    entry = (NtUserSWLPEntry*)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(NtUserSWLPEntry),
        ActionHistoryTag);
    if (NULL == entry) {
        ExReleaseResourceLite(&NtUserSWLPLock);
        return FALSE;
    }
    for (auto i = 0; i < NTUSERSWLP_BUFFER_SIZE; i++) {
        if (hWnd == NtUserSWLPBuffer[i].hWnd) {
            RtlCopyMemory(entry, &NtUserSWLPBuffer[i], sizeof(NtUserSWLPEntry));
            break;
        }
    }
    ExReleaseResourceLite(&NtUserSWLPLock);
    return entry;
}

BOOLEAN AddNtUserSetPropEntry(
    HWND hWnd,
    ATOM Atom,
    HANDLE Data
)
{
    if (FALSE == ExAcquireResourceExclusiveLite(&NtUserSPLock, TRUE)) {
        return FALSE;
    }
    NtUserSPBuffer[freeNtUserSPIdx] = { hWnd, Atom, Data };
    freeNtUserSPIdx = (freeNtUserSPIdx + 1) % NTUSERSP_BUFFER_SIZE;
    ExReleaseResourceLite(&NtUserSPLock);
    return TRUE;
}

// Caller must deallocate NtUserSPEntry
NtUserSPEntry* FindNtSetWindowLongPtrEntry(HWND hWnd)
{
    NtUserSPEntry* entry = NULL;
    if (FALSE == ExAcquireResourceExclusiveLite(&NtUserSPLock, TRUE)) {
        return FALSE;
    }
    entry = (NtUserSPEntry*)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(NtUserSPEntry),
        ActionHistoryTag);
    if (NULL == entry) {
        ExReleaseResourceLite(&NtUserSPLock);
        return FALSE;
    }
    for (auto i = 0; i < NTUSERSP_BUFFER_SIZE; i++) {
        if (hWnd == NtUserSPBuffer[i].hWnd) {
            RtlCopyMemory(entry, &NtUserSPBuffer[i], sizeof(NtUserSPEntry));
            break;
        }
    }
    ExReleaseResourceLite(&NtUserSPLock);
    return entry;
}