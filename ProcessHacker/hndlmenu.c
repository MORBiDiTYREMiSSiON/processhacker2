/*
 * Process Hacker -
 *   common handle menu items
 *
 * Copyright (C) 2016 wj32
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <phapp.h>
#include <hndlmenu.h>

#include <emenu.h>
#include <hndlinfo.h>
#include <kphuser.h>

#include <mainwnd.h>
#include <procprp.h>
#include <procprv.h>

VOID PhInsertHandleObjectPropertiesEMenuItems(
    _In_ struct _PH_EMENU_ITEM *Menu,
    _In_ ULONG InsertBeforeId,
    _In_ BOOLEAN EnableShortcut,
    _In_ PPH_HANDLE_ITEM_INFO Info
    )
{
    PPH_EMENU_ITEM parentItem;
    ULONG indexInParent;

    if (!PhFindEMenuItemEx(Menu, 0, NULL, InsertBeforeId, &parentItem, &indexInParent))
        return;

    if (PhEqualString2(Info->TypeName, L"File", TRUE) || PhEqualString2(Info->TypeName, L"DLL", TRUE) ||
        PhEqualString2(Info->TypeName, L"Mapped file", TRUE) || PhEqualString2(Info->TypeName, L"Mapped image", TRUE))
    {
        if (PhEqualString2(Info->TypeName, L"File", TRUE))
            PhInsertEMenuItem(parentItem, PhCreateEMenuItem(0, ID_HANDLE_OBJECTPROPERTIES2, L"File properties", NULL, NULL), indexInParent);

        PhInsertEMenuItem(parentItem, PhCreateEMenuItem(0, ID_HANDLE_OBJECTPROPERTIES1, PhaAppendCtrlEnter(L"Open &file location", EnableShortcut), NULL, NULL), indexInParent);
    }
    else if (PhEqualString2(Info->TypeName, L"Key", TRUE))
    {
        PhInsertEMenuItem(parentItem, PhCreateEMenuItem(0, ID_HANDLE_OBJECTPROPERTIES1, PhaAppendCtrlEnter(L"Open key", EnableShortcut), NULL, NULL), indexInParent);
    }
    else if (PhEqualString2(Info->TypeName, L"Process", TRUE))
    {
        PhInsertEMenuItem(parentItem, PhCreateEMenuItem(0, ID_HANDLE_OBJECTPROPERTIES1, PhaAppendCtrlEnter(L"Process properties", EnableShortcut), NULL, NULL), indexInParent);
    }
    else if (PhEqualString2(Info->TypeName, L"Section", TRUE))
    {
        PhInsertEMenuItem(parentItem, PhCreateEMenuItem(0, ID_HANDLE_OBJECTPROPERTIES1, PhaAppendCtrlEnter(L"Read/Write memory", EnableShortcut), NULL, NULL), indexInParent);
    }
    else if (PhEqualString2(Info->TypeName, L"Thread", TRUE))
    {
        PhInsertEMenuItem(parentItem, PhCreateEMenuItem(0, ID_HANDLE_OBJECTPROPERTIES1, PhaAppendCtrlEnter(L"Go to thread", EnableShortcut), NULL, NULL), indexInParent);
    }
}

static NTSTATUS PhpDuplicateHandleFromProcessItem(
    _Out_ PHANDLE NewHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ HANDLE ProcessId,
    _In_ HANDLE Handle
    )
{
    NTSTATUS status;
    HANDLE processHandle;

    if (!NT_SUCCESS(status = PhOpenProcess(
        &processHandle,
        PROCESS_DUP_HANDLE,
        ProcessId
        )))
        return status;

    status = NtDuplicateObject(
        processHandle,
        Handle,
        NtCurrentProcess(),
        NewHandle,
        DesiredAccess,
        0,
        0
        );
    NtClose(processHandle);

    return status;
}

static VOID PhpShowProcessPropContext(
    _In_ PVOID Parameter
    )
{
    PhShowProcessProperties(Parameter);
    PhDereferenceObject(Parameter);
}

VOID PhShowHandleObjectProperties1(
    _In_ HWND hWnd,
    _In_ PPH_HANDLE_ITEM_INFO Info
    )
{
    if (PhEqualString2(Info->TypeName, L"File", TRUE) || PhEqualString2(Info->TypeName, L"DLL", TRUE) ||
        PhEqualString2(Info->TypeName, L"Mapped file", TRUE) || PhEqualString2(Info->TypeName, L"Mapped image", TRUE))
    {
        if (Info->BestObjectName)
            PhShellExploreFile(hWnd, Info->BestObjectName->Buffer);
        else
            PhShowError(hWnd, L"Unable to open file location because the object is unnamed.");
    }
    else if (PhEqualString2(Info->TypeName, L"Key", TRUE))
    {
        if (Info->BestObjectName)
            PhShellOpenKey2(hWnd, Info->BestObjectName);
        else
            PhShowError(hWnd, L"Unable to open key because the object is unnamed.");
    }
    else if (PhEqualString2(Info->TypeName, L"Process", TRUE))
    {
        HANDLE processHandle;
        HANDLE processId;
        PPH_PROCESS_ITEM targetProcessItem;

        processId = NULL;

        if (KphIsConnected())
        {
            if (NT_SUCCESS(PhOpenProcess(
                &processHandle,
                ProcessQueryAccess,
                Info->ProcessId
                )))
            {
                PROCESS_BASIC_INFORMATION basicInfo;

                if (NT_SUCCESS(KphQueryInformationObject(
                    processHandle,
                    Info->Handle,
                    KphObjectProcessBasicInformation,
                    &basicInfo,
                    sizeof(PROCESS_BASIC_INFORMATION),
                    NULL
                    )))
                {
                    processId = basicInfo.UniqueProcessId;
                }

                NtClose(processHandle);
            }
        }
        else
        {
            HANDLE handle;
            PROCESS_BASIC_INFORMATION basicInfo;

            if (NT_SUCCESS(PhpDuplicateHandleFromProcessItem(
                &handle,
                ProcessQueryAccess,
                Info->ProcessId,
                Info->Handle
                )))
            {
                if (NT_SUCCESS(PhGetProcessBasicInformation(handle, &basicInfo)))
                    processId = basicInfo.UniqueProcessId;

                NtClose(handle);
            }
        }

        if (processId)
        {
            targetProcessItem = PhReferenceProcessItem(processId);

            if (targetProcessItem)
            {
                ProcessHacker_ShowProcessProperties(PhMainWndHandle, targetProcessItem);
                PhDereferenceObject(targetProcessItem);
            }
            else
            {
                PhShowError(hWnd, L"The process does not exist.");
            }
        }
    }
    else if (PhEqualString2(Info->TypeName, L"Section", TRUE))
    {
        HANDLE handle = NULL;
        BOOLEAN readOnly = FALSE;

        if (!NT_SUCCESS(PhpDuplicateHandleFromProcessItem(
            &handle,
            SECTION_QUERY | SECTION_MAP_READ | SECTION_MAP_WRITE,
            Info->ProcessId,
            Info->Handle
            )))
        {
            PhpDuplicateHandleFromProcessItem(
                &handle,
                SECTION_QUERY | SECTION_MAP_READ,
                Info->ProcessId,
                Info->Handle
                );
            readOnly = TRUE;
        }

        if (handle)
        {
            NTSTATUS status;
            PPH_STRING sectionName = NULL;
            SECTION_BASIC_INFORMATION basicInfo;
            SIZE_T viewSize = PH_MAX_SECTION_EDIT_SIZE;
            PVOID viewBase = NULL;
            BOOLEAN tooBig = FALSE;

            PhGetHandleInformation(NtCurrentProcess(), handle, -1, NULL, NULL, NULL, &sectionName);

            if (NT_SUCCESS(PhGetSectionBasicInformation(handle, &basicInfo)))
            {
                if (basicInfo.MaximumSize.QuadPart <= PH_MAX_SECTION_EDIT_SIZE)
                    viewSize = (SIZE_T)basicInfo.MaximumSize.QuadPart;
                else
                    tooBig = TRUE;

                status = NtMapViewOfSection(
                    handle,
                    NtCurrentProcess(),
                    &viewBase,
                    0,
                    0,
                    NULL,
                    &viewSize,
                    ViewShare,
                    0,
                    readOnly ? PAGE_READONLY : PAGE_READWRITE
                    );

                if (status == STATUS_SECTION_PROTECTION && !readOnly)
                {
                    status = NtMapViewOfSection(
                        handle,
                        NtCurrentProcess(),
                        &viewBase,
                        0,
                        0,
                        NULL,
                        &viewSize,
                        ViewShare,
                        0,
                        PAGE_READONLY
                        );
                }

                if (NT_SUCCESS(status))
                {
                    PPH_SHOW_MEMORY_EDITOR showMemoryEditor = PhAllocate(sizeof(PH_SHOW_MEMORY_EDITOR));

                    if (tooBig)
                        PhShowWarning(hWnd, L"The section size is greater than 32 MB. Only the first 32 MB will be available for editing.");

                    memset(showMemoryEditor, 0, sizeof(PH_SHOW_MEMORY_EDITOR));
                    showMemoryEditor->ProcessId = NtCurrentProcessId();
                    showMemoryEditor->BaseAddress = viewBase;
                    showMemoryEditor->RegionSize = viewSize;
                    showMemoryEditor->SelectOffset = -1;
                    showMemoryEditor->SelectLength = 0;
                    showMemoryEditor->Title = sectionName ? PhConcatStrings2(L"Section - ", sectionName->Buffer) : PhCreateString(L"Section");
                    showMemoryEditor->Flags = PH_MEMORY_EDITOR_UNMAP_VIEW_OF_SECTION;
                    ProcessHacker_ShowMemoryEditor(PhMainWndHandle, showMemoryEditor);
                }
                else
                {
                    PhShowStatus(hWnd, L"Unable to map a view of the section", status, 0);
                }
            }

            PhClearReference(&sectionName);

            NtClose(handle);
        }
    }
    else if (PhEqualString2(Info->TypeName, L"Thread", TRUE))
    {
        HANDLE processHandle;
        CLIENT_ID clientId;
        PPH_PROCESS_ITEM targetProcessItem;
        PPH_PROCESS_PROPCONTEXT propContext;

        clientId.UniqueProcess = NULL;
        clientId.UniqueThread = NULL;

        if (KphIsConnected())
        {
            if (NT_SUCCESS(PhOpenProcess(
                &processHandle,
                ProcessQueryAccess,
                Info->ProcessId
                )))
            {
                THREAD_BASIC_INFORMATION basicInfo;

                if (NT_SUCCESS(KphQueryInformationObject(
                    processHandle,
                    Info->Handle,
                    KphObjectThreadBasicInformation,
                    &basicInfo,
                    sizeof(THREAD_BASIC_INFORMATION),
                    NULL
                    )))
                {
                    clientId = basicInfo.ClientId;
                }

                NtClose(processHandle);
            }
        }
        else
        {
            HANDLE handle;
            THREAD_BASIC_INFORMATION basicInfo;

            if (NT_SUCCESS(PhpDuplicateHandleFromProcessItem(
                &handle,
                ThreadQueryAccess,
                Info->ProcessId,
                Info->Handle
                )))
            {
                if (NT_SUCCESS(PhGetThreadBasicInformation(handle, &basicInfo)))
                    clientId = basicInfo.ClientId;

                NtClose(handle);
            }
        }

        if (clientId.UniqueProcess)
        {
            targetProcessItem = PhReferenceProcessItem(clientId.UniqueProcess);

            if (targetProcessItem)
            {
                propContext = PhCreateProcessPropContext(PhMainWndHandle, targetProcessItem);
                PhDereferenceObject(targetProcessItem);
                PhSetSelectThreadIdProcessPropContext(propContext, clientId.UniqueThread);
                ProcessHacker_Invoke(PhMainWndHandle, PhpShowProcessPropContext, propContext);
            }
            else
            {
                PhShowError(hWnd, L"The process does not exist.");
            }
        }
    }
}

VOID PhShowHandleObjectProperties2(
    _In_ HWND hWnd,
    _In_ PPH_HANDLE_ITEM_INFO Info
    )
{
    if (PhEqualString2(Info->TypeName, L"File", TRUE) || PhEqualString2(Info->TypeName, L"DLL", TRUE) ||
        PhEqualString2(Info->TypeName, L"Mapped file", TRUE) || PhEqualString2(Info->TypeName, L"Mapped image", TRUE))
    {
        if (Info->BestObjectName)
            PhShellProperties(hWnd, Info->BestObjectName->Buffer);
        else
            PhShowError(hWnd, L"Unable to open file properties because the object is unnamed.");
    }
}
