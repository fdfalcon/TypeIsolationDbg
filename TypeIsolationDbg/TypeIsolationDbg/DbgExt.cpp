/*
##########################################################
# This file is based on the sync.cpp file from ret-sync. #
##########################################################

Copyright (C) 2016, Alexandre Gazet.

Copyright (C) 2012-2015, Quarkslab.


ret-sync is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "DbgExt.h"
#include <shlwapi.h>

#define MAX_NAME 1024


#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "dbgeng.lib")


PDEBUG_CLIENT4              g_ExtClient;
PDEBUG_CONTROL              g_ExtControl;
PDEBUG_SYMBOLS3             g_ExtSymbols;
PDEBUG_REGISTERS            g_ExtRegisters;
WINDBG_EXTENSION_APIS   ExtensionApis;

DWORD_PTR TypeIsolationHead;

// Queries for all debugger interfaces.
extern "C" HRESULT ExtQuery(PDEBUG_CLIENT4 Client)
{
    HRESULT hRes = S_OK;

    if (g_ExtClient != NULL){
        return S_OK;
    }

    if (FAILED(hRes = Client->QueryInterface(__uuidof(IDebugControl), (void **)&g_ExtControl))){
        goto Fail;
    }
    if (FAILED(hRes = Client->QueryInterface(__uuidof(IDebugSymbols3), (void **)&g_ExtSymbols))){
        goto Fail;
    }


    if (FAILED(hRes = Client->QueryInterface(__uuidof(IDebugRegisters), (void **)&g_ExtRegisters))){
        goto Fail;
    }

    g_ExtClient = Client;
    return S_OK;

Fail:
    ExtRelease();
    return hRes;
}


// Cleans up all debugger interfaces.
void ExtRelease(void)
{
    g_ExtClient = NULL;
    EXT_RELEASE(g_ExtControl);
    EXT_RELEASE(g_ExtSymbols);
    EXT_RELEASE(g_ExtRegisters);
}




// plugin initialization
extern "C" HRESULT CALLBACK DebugExtensionInitialize(PULONG Version, PULONG Flags)
{
    HRESULT hRes = S_OK;
    IDebugClient *DebugClient;
    PDEBUG_CONTROL DebugControl;

    *Version = DEBUG_EXTENSION_VERSION(EXT_MAJOR_VER, EXT_MINOR_VER);
    *Flags = 0;

    if (FAILED(hRes = DebugCreate(__uuidof(IDebugClient), (void **)&DebugClient))){
        return hRes;
    }

    if (SUCCEEDED(hRes = DebugClient->QueryInterface(__uuidof(IDebugControl), (void **)&DebugControl)))
    {
        // Get the windbg-style extension APIS
        ExtensionApis.nSize = sizeof(ExtensionApis);
        hRes = DebugControl->GetWindbgExtensionApis64(&ExtensionApis);
        DebugControl->Release();
        dprintf("[TypeIsolationDbg] DebugExtensionInitialize, ExtensionApis loaded\n");
    }

    DebugClient->Release();
    g_ExtClient = NULL;

    return hRes;
}


// notification callback
extern "C" void CALLBACK DebugExtensionNotify(ULONG Notify, ULONG64 Argument)
{
    UNREFERENCED_PARAMETER(Argument);

   
    return;
}


extern "C" void CALLBACK DebugExtensionUninitialize(void)
{
    EXIT_API();
    return;
}



HRESULT CALLBACK typeisolation(PDEBUG_CLIENT4 Client, PCSTR Args)
{
	HRESULT hRes;
	DWORD_PTR  Address = 0;
	CTYPEISOLATION TypeIsolation;
	ULONG Bytes;
	INIT_API();

	// just the structure description if no address is supplied
	if (!Args || !*Args) {
		hRes = g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL,
			"NSInstrumentation::CTypeIsolation (size = 0x20)\n"
			"      +0x000 CSECTIONENTRY *next;\n"
			"      +0x008 CSECTIONENTRY *previous;\n"
			"      +0x010 _EX_PUSH_LOCK *pushlock;\n"
			"      +0x018 ULONG64 size;\n");
		return hRes;

	}

	Address = GetExpression(Args);
	if (!Address) {
		dprintf("[TypeIsolationDbg] could not parse the address.\n");
		return E_FAIL;
	}

	ReadMemory(Address, &TypeIsolation, sizeof(TypeIsolation), &Bytes);
	if (Bytes != sizeof(TypeIsolation)) {
		dprintf("[TypeIsolationDbg] could not read from address 0x%p.\n", Address);
		return E_FAIL;
	}

	hRes |= g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL,
		"NSInstrumentation::CTypeIsolation\n"
		"      +0x000 next                               : <exec cmd=\"!sectionentry %p\">0x%p</exec>\n"
		"      +0x008 previous                           : <exec cmd=\"!sectionentry %p\">0x%p</exec>\n"
		"      +0x010 pushlock                           : <exec cmd=\"dt nt!_EX_PUSH_LOCK %p\">0x%p</exec>\n"
		"      +0x018 size                               : 0x%X [number of section entries: 0x%x]\n",
		TypeIsolation.next, TypeIsolation.next,
		TypeIsolation.previous, TypeIsolation.previous,
		TypeIsolation.pushlock, TypeIsolation.pushlock,
		TypeIsolation.size, TypeIsolation.size / 0xf0);

	return hRes;
}


HRESULT CALLBACK sectionentry(PDEBUG_CLIENT4 Client, PCSTR Args)
{
	HRESULT hRes;
	DWORD_PTR  Address = 0;
	CSECTIONENTRY SectionEntry;
	ULONG Bytes;
	INIT_API();
	PCSTR next_command, previous_command, next_hint, previous_hint;

	// just the structure description if no address is supplied
	if (!Args || !*Args) {
		hRes = g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL,
			"NSInstrumentation::CSectionEntry (size = 0x28)\n"
			"      +0x000 CSECTIONENTRY *next;\n"
			"      +0x008 CSECTIONENTRY *previous;\n"
			"      +0x010 SECTION *section;\n"
			"      +0x018 PVOID view;\n"
			"      +0x020 CSECTIONBITMAPALLOCATOR *bitmap_allocator;\n");
		return hRes;

	}

	Address = GetExpression(Args);
	if (!Address) {
		dprintf("[TypeIsolationDbg] could not parse the address.\n");
		return E_FAIL;
	}

	ReadMemory(Address, &SectionEntry, sizeof(SectionEntry), &Bytes);
	if (Bytes != sizeof(SectionEntry)) {
		dprintf("[TypeIsolationDbg] could not read from address 0x%p.\n", Address);
		return E_FAIL;
	}

	if ((DWORD_PTR)SectionEntry.next == TypeIsolationHead) {
		next_command = "typeisolation";
		next_hint = " [head]";
	}
	else {
		next_command = "sectionentry";
		next_hint = "";
	}

	if ((DWORD_PTR)SectionEntry.previous == TypeIsolationHead) {
		previous_command = "typeisolation";
		previous_hint = " [head]";
	}
	else {
		previous_command = "sectionentry";
		previous_hint = "";
	}

	hRes |= g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL,
		"NSInstrumentation::CSectionEntry\n"
		"      +0x000 next                                : <exec cmd=\"!%s %p\">0x%p</exec>%s\n"
		"      +0x008 previous                            : <exec cmd=\"!%s %p\">0x%p</exec>%s\n"
		"      +0x010 section                             : 0x%p\n"
		"      +0x018 view                                : 0x%p\n"
		"      +0x020 bitmap_allocator                    : <exec cmd=\"!sectionbitmapallocator %p\">0x%p</exec>\n",
		next_command, SectionEntry.next, SectionEntry.next, next_hint,
		previous_command, SectionEntry.previous, SectionEntry.previous, previous_hint,
		SectionEntry.section,
		SectionEntry.view,
		SectionEntry.bitmap_allocator, SectionEntry.bitmap_allocator);

	return hRes;
}


HRESULT CALLBACK sectionbitmapallocator(PDEBUG_CLIENT4 Client, PCSTR Args)
{
	HRESULT hRes;
	DWORD_PTR  Address = 0;
	CSECTIONBITMAPALLOCATOR BitmapAllocator;
	ULONG Bytes;
	ULONG64 decoded_view, decoded_rtl_bitmap;
	INIT_API();

	// just the structure description if no address is supplied
	if (!Args || !*Args) {
		hRes = g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL,
			"NSInstrumentation::CSectionBitmapAllocator (size = 0x28)\n"
			"      +0x000 _EX_PUSH_LOCK *pushlock;\n"
			"      +0x008 ULONG64 xored_view;\n"
			"      +0x010 ULONG64 xor_key;\n"
			"      +0x018 ULONG64 xored_rtl_bitmap;\n"
			"      +0x020 ULONG bitmap_hint_index;\n"
			"      +0x024 ULONG num_committed_views;\n");
		return hRes;

	}

	Address = GetExpression(Args);
	if (!Address) {
		dprintf("[TypeIsolationDbg] could not parse the address.\n");
		return E_FAIL;
	}

	ReadMemory(Address, &BitmapAllocator, sizeof(BitmapAllocator), &Bytes);
	if (Bytes != sizeof(BitmapAllocator)) {
		dprintf("[TypeIsolationDbg] could not read from address 0x%p.\n", Address);
		return E_FAIL;
	}

	decoded_view   = BitmapAllocator.xored_view ^ BitmapAllocator.xor_key;
	decoded_rtl_bitmap = BitmapAllocator.xored_rtl_bitmap ^ BitmapAllocator.xor_key;

	hRes |= g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL,
		"NSInstrumentation::CSectionBitmapAllocator\n"
		"      +0x000 pushlock                            : <exec cmd=\"dt nt!_EX_PUSH_LOCK %p\">0x%p</exec>\n"
		"      +0x008 xored_view                          : 0x%p [decoded: <exec cmd=\"dc %p\">0x%p</exec>]\n"
		"      +0x010 xor_key                             : 0x%p\n"
		"      +0x018 xored_rtl_bitmap                    : 0x%p [decoded: <exec cmd=\"!rtlbitmap %p\">0x%p</exec>]\n"
		"      +0x020 bitmap_hint_index                   : 0x%X\n"
		"      +0x024 num_committed_views                 : 0x%X\n",
		BitmapAllocator.pushlock, BitmapAllocator.pushlock,
		BitmapAllocator.xored_view, decoded_view,
		BitmapAllocator.xor_key,
		BitmapAllocator.xored_rtl_bitmap, decoded_rtl_bitmap, decoded_rtl_bitmap,
		BitmapAllocator.bitmap_hint_index,
		BitmapAllocator.num_committed_views);

	return hRes;
}


HRESULT CALLBACK rtlbitmap(PDEBUG_CLIENT4 Client, PCSTR Args)
{
	HRESULT hRes;
	DWORD_PTR  Address = 0;
	RTL_BITMAP RtlBitmap;
	ULONG Bytes;
	INIT_API();

	// just the structure description if no address is supplied
	if (!Args || !*Args) {
		hRes = g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL,
			"RTL_BITMAP (size = 0x10)\n"
			"      +0x000 ULONG64 size;\n"
			"      +0x008 PVOID bitmap_buffer;\n");
		return hRes;

	}

	Address = GetExpression(Args);
	if (!Address) {
		dprintf("[TypeIsolationDbg] could not parse the address.\n");
		return E_FAIL;
	}

	ReadMemory(Address, &RtlBitmap, sizeof(RtlBitmap), &Bytes);
	if (Bytes != sizeof(RtlBitmap)) {
		dprintf("[TypeIsolationDbg] could not read from address 0x%p.\n", Address);
		return E_FAIL;
	}

	hRes |= g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL,
		"RTL_BITMAP\n"
		"      +0x000 size                                : 0x%X\n"
		"      +0x008 bitmap_buffer                       : <exec cmd=\"dyb %p L20\">0x%p</exec>\n",
		RtlBitmap.size,
		RtlBitmap.bitmap_buffer, RtlBitmap.bitmap_buffer);

	return hRes;
}



// get the root TypeIsolation address from the user input or from the symbol resolution and print its structure
HRESULT CALLBACK gptypeisolation(PDEBUG_CLIENT4 Client, PCSTR Args)
{
	HRESULT hRes;
	DWORD_PTR Address1 = 0, Address2 = 0, gpTypeIsolation = 0;
	ULONG Bytes;
	CTYPEISOLATION TypeIsolation;
	INIT_API();

	if (!Args || !*Args) {
		gpTypeIsolation = GetExpression("win32kbase!gpTypeIsolation");
	}
	else {
		gpTypeIsolation = GetExpression(Args);
	}
	
	
	if (!gpTypeIsolation) {
		dprintf("[TypeIsolationDbg] could not find win32kbase!gpTypeIsolation address.\n");
		return E_FAIL;
	}
	hRes = g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL, "win32kbase!gpTypeIsolation is at address 0x%p.\n", gpTypeIsolation);

	//Dereference TWICE win32kbase!gpTypeIsolation, so we get a a pointer to a CTypeIsolation struct
	ReadMemory(gpTypeIsolation, &Address1, sizeof(Address1), &Bytes);
	if (Bytes != sizeof(Address1)) {
		dprintf("[TypeIsolationDbg] could not read memory at address 0x%p. "
			"Try switching to a user-mode process with at least one kernel GUI thread, such as notepad.exe!\n"
			"E.g: !process 0 0; .process /i <PROCESS>; g\n", gpTypeIsolation);

		return E_FAIL;
	}
	hRes = g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL, "*win32kbase!gpTypeIsolation: 0x%p.\n", Address1);

	ReadMemory(Address1, &Address2, sizeof(Address2), &Bytes);
	if (Bytes != sizeof(Address2)) {
		dprintf("[TypeIsolationDbg] could not read memory at address 0x%p.\n", Address1);
		return E_FAIL;
	}
	hRes = g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL, "**win32kbase!gpTypeIsolation: 0x%p.\n", Address2);

	// Cache the address of the CTypeIsolation structure
	TypeIsolationHead = Address2;

	//Read the CTypeIsolation structure referenced by the pointer mentioned above
	ReadMemory(Address2, &TypeIsolation, sizeof(TypeIsolation), &Bytes);
	if (Bytes != sizeof(TypeIsolation)) {
		dprintf("[TypeIsolationDbg] could not read memory at address 0x%p.\n", Address2);
		return E_FAIL;
	}

	
	hRes |= g_ExtControl->ControlledOutput(DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL, 
		"<?dml?>"
		"NSInstrumentation::CTypeIsolation\n"
		"      +0x000 next                                : <exec cmd=\"!sectionentry %p\">0x%p</exec>\n"
		"      +0x008 previous                            : <exec cmd=\"!sectionentry %p\">0x%p</exec>\n"
		"      +0x010 pushlock                            : <exec cmd=\"dt nt!_EX_PUSH_LOCK %p\">0x%p</exec>\n"
		"      +0x018 size                                : 0x%X [number of section entries: 0x%x]\n", 
		TypeIsolation.next, TypeIsolation.next,
		TypeIsolation.previous, TypeIsolation.previous,
		TypeIsolation.pushlock, TypeIsolation.pushlock,
		TypeIsolation.size, TypeIsolation.size / 0xf0);

	return hRes;
}



HRESULT CALLBACK typeisolationhelp(PDEBUG_CLIENT4 Client, PCSTR Args)
{
    INIT_API();
    UNREFERENCED_PARAMETER(Args);

	dprintf("[TypeIsolationDbg] extension commands help:\n"
		" > !gptypeisolation [address]          = prints the top-level CTypeIsolation structure (default address: win32kbase!gpTypeIsolation)\n"
		" > !typeisolation [address]            = prints a NSInstrumentation::CTypeIsolation structure\n"
		" > !sectionentry [address]             = prints a NSInstrumentation::CSectionEntry structure\n"
		" > !sectionbitmapallocator [address]   = prints a NSInstrumentation::CSectionBitmapAllocator structure\n"
		" > !rtlbitmap [address]                = prints a RTL_BITMAP structure\n"
		" > !typeisolationhelp                  = meh... >_>'\n\n");

    return S_OK;
}

