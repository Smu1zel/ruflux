/*
 * Ruflux: NT5/XP Compatibility Wrapper Header
 * Bridges Windows Vista -> XP API gaps for Windows XP / Server 2003
 * 
 * Copyright © 2026 Lynden Lewis <lyndenl25@yahoo.com> & EAZY BLACK
 */

#pragma once

#ifndef _WINSOCK2API_
#include <winsock2.h>
#endif
#ifndef _WS2TCPIP_H
#include <ws2tcpip.h>
#endif
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>

#ifdef __cplusplus
extern "C" {
#endif

/* kernel32 wrappers */
ULONGLONG WINAPI Ext_GetTickCount64(VOID);
LCID WINAPI Ext_LocaleNameToLCID(LPCWSTR lpName, DWORD dwFlags);
int WINAPI Ext_LCIDToLocaleName(LCID Locale, LPWSTR lpName, int cchName, DWORD dwFlags);
int WINAPI Ext_LCMapStringEx(LPCWSTR lpLocaleName, DWORD dwMapFlags, LPCWSTR lpSrcStr, int cchSrc,
                             LPWSTR lpDestStr, int cchDest, LPNLSVERSIONINFO lpVersionInformation,
                             LPVOID lpReserved, LPARAM lParam);
BOOL WINAPI Ext_GetNLSVersionEx(NLS_FUNCTION Function, LPCWSTR lpLocaleName, LPNLSVERSIONINFOEX lpVersionInformation);
BOOL WINAPI Ext_InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD dwFlags);
VOID WINAPI Ext_FlushProcessWriteBuffers(VOID);
BOOL WINAPI Ext_IsThreadAFiber(VOID);
BOOL WINAPI Ext_QueryThreadCycleTime(HANDLE ThreadHandle, PULONG64 CycleTime);
DWORD WINAPI Ext_GetDynamicTimeZoneInformation(PDYNAMIC_TIME_ZONE_INFORMATION pDynamicTimeZoneInformation);
VOID WINAPI Ext_InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
BOOL WINAPI Ext_SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds);
VOID WINAPI Ext_WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
VOID WINAPI Ext_WakeAllConditionVariable(PCONDITION_VARIABLE ConditionVariable);
LANGID WINAPI Ext_GetThreadUILanguage(VOID);
LANGID WINAPI Ext_SetThreadUILanguage(LANGID LangId);

#if defined(_M_IX86) || defined(__i386__)
DWORD WINAPI Ext_GetCurrentProcessorNumber(VOID);
BOOL WINAPI Ext_SetThreadStackGuarantee(PULONG StackSizeInBytes);
BOOL WINAPI Ext_SetFileCompletionNotificationModes(HANDLE FileHandle, UCHAR Flags);
BOOL WINAPI Ext_GetNLSVersion(NLS_FUNCTION Function, LCID Locale, LPNLSVERSIONINFO lpVersionInformation);
#endif

/* shell32 wrappers */
HRESULT WINAPI Ext_SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPath);

/* ws2_32 wrappers */
int WINAPI Ext_WSAPoll(WSAPOLLFD* fds, ULONG nfds, int timeout);

/* advapi32 wrappers */
LSTATUS WINAPI Ext_RegGetValueA(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);
LSTATUS WINAPI Ext_RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);

/* user32 wrappers */
#if defined(_M_IX86) || defined(__i386__)
BOOL WINAPI Ext_UpdateLayeredWindowIndirect(HWND hwnd, const UPDATELAYEREDWINDOWINFO *pULWInfo);
#endif

#ifdef __cplusplus
}
#endif

/* Global macro overrides to redirect calls transparently to wrappers */
#ifndef RUX_NO_NT5_WRAPPERS
#undef GetTickCount64
#define GetTickCount64 Ext_GetTickCount64

#undef SHGetKnownFolderPath
#define SHGetKnownFolderPath Ext_SHGetKnownFolderPath

#undef WSAPoll
#define WSAPoll Ext_WSAPoll

#undef RegGetValueA
#define RegGetValueA Ext_RegGetValueA

#undef RegGetValueW
#define RegGetValueW Ext_RegGetValueW

#undef LCIDToLocaleName
#define LCIDToLocaleName Ext_LCIDToLocaleName

#undef LocaleNameToLCID
#define LocaleNameToLCID Ext_LocaleNameToLCID

#undef LCMapStringEx
#define LCMapStringEx Ext_LCMapStringEx

#undef InitializeCriticalSectionEx
#define InitializeCriticalSectionEx Ext_InitializeCriticalSectionEx

#undef InitializeConditionVariable
#define InitializeConditionVariable Ext_InitializeConditionVariable

#undef SleepConditionVariableCS
#define SleepConditionVariableCS Ext_SleepConditionVariableCS

#undef WakeConditionVariable
#define WakeConditionVariable Ext_WakeConditionVariable

#undef WakeAllConditionVariable
#define WakeAllConditionVariable Ext_WakeAllConditionVariable

#undef GetThreadUILanguage
#define GetThreadUILanguage Ext_GetThreadUILanguage

#undef SetThreadUILanguage
#define SetThreadUILanguage Ext_SetThreadUILanguage
#endif
