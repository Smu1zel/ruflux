#define RUX_NO_NT5_WRAPPERS 1
#include <windows.h>
#include <winternl.h>

typedef struct _TP_TIMER {
    PTP_TIMER_CALLBACK callback;
    PVOID context;
    HANDLE hTimer;
} TP_TIMER, *PTP_TIMER, TP_TIMER_WRAPPER;

typedef struct {
    LPCWSTR TargetName;
    LCID ResultLcid;
} LOCALE_SEARCH;

// nt5.1 sucks i have to do this shit 
#if defined(_M_IX86) || defined(__i386__)
#include <intrin.h>

typedef BOOL (WINAPI *PFN_GETNLSVERSION)(NLS_FUNCTION, LCID, LPNLSVERSIONINFO);
typedef DWORD (WINAPI *PFN_GETCURRENTPROCESSORNUMBER)(VOID);
typedef BOOL (WINAPI *PFN_SETTHREADSTACKGUARANTEE)(PULONG StackSizeInBytes);
typedef BOOL (WINAPI *PFN_SETFILECOMPLETIONNOTIFICATIONMODES)(HANDLE FileHandle, UCHAR Flags);

static PFN_GETNLSVERSION             g_pfnGetNLSVersion = NULL;
static PFN_GETCURRENTPROCESSORNUMBER   g_pfnGetCurrentProcessorNumber = NULL;
static PFN_SETTHREADSTACKGUARANTEE     g_pfnSetThreadStackGuarantee = NULL;
static PFN_SETFILECOMPLETIONNOTIFICATIONMODES g_pfnSetFileCompletionNotificationModes = NULL;
static volatile LONG                 g_bK32Initialized = 0;

#define FileIoCompletionNotificationInformation 41

typedef struct _FILE_IO_COMPLETION_NOTIFICATION_INFORMATION {
    ULONG Flags;
} FILE_IO_COMPLETION_NOTIFICATION_INFORMATION, *PFILE_IO_COMPLETION_NOTIFICATION_INFORMATION;

static void InitK32Functions(void) {
    if (InterlockedCompareExchange(&g_bK32Initialized, 1, 0) == 0) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (hKernel32) {
            g_pfnGetNLSVersion             = (PFN_GETNLSVERSION)GetProcAddress(hKernel32, "GetNLSVersion");
            g_pfnGetCurrentProcessorNumber = (PFN_GETCURRENTPROCESSORNUMBER)GetProcAddress(hKernel32, "GetCurrentProcessorNumber");
            g_pfnSetThreadStackGuarantee   = (PFN_SETTHREADSTACKGUARANTEE)GetProcAddress(hKernel32, "SetThreadStackGuarantee");
            g_pfnSetFileCompletionNotificationModes = (PFN_SETFILECOMPLETIONNOTIFICATIONMODES)GetProcAddress(hKernel32, "SetFileCompletionNotificationModes");
        }
    }
}

#ifdef _USRDLL
#pragma comment(linker, "/export:GetCurrentProcessorNumber=_Ext_GetCurrentProcessorNumber@0")
#pragma comment(linker, "/export:SetThreadStackGuarantee=_Ext_SetThreadStackGuarantee@4")
#pragma comment(linker, "/export:SetFileCompletionNotificationModes=_Ext_SetFileCompletionNotificationModes@8")
#pragma comment(linker, "/export:GetNLSVersion=_Ext_GetNLSVersion@12")
#endif
#else
#ifdef _USRDLL
#pragma comment(linker, "/export:GetCurrentProcessorNumber=kernel32.GetCurrentProcessorNumber")
#pragma comment(linker, "/export:SetThreadStackGuarantee=kernel32.SetThreadStackGuarantee")
#pragma comment(linker, "/export:SetFileCompletionNotificationModes=kernel32.SetFileCompletionNotificationModes")
#pragma comment(linker, "/export:GetNLSVersion=kernel32.GetNLSVersion")
#endif
#endif

#ifdef _USRDLL
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
#if defined(_M_IX86) || defined(__i386__)
    if (fdwReason == DLL_PROCESS_ATTACH) {
        InitK32Functions();
    }
#endif
    return TRUE;
}
#endif

ULONGLONG WINAPI Ext_GetTickCount64(VOID) {
    typedef ULONGLONG (WINAPI *PFN_GETTICKCOUNT64)(VOID);
    static PFN_GETTICKCOUNT64 s_pfnGetTickCount64 = (PFN_GETTICKCOUNT64)-1;

    if (s_pfnGetTickCount64 == (PFN_GETTICKCOUNT64)-1) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        s_pfnGetTickCount64 = hKernel32 ? (PFN_GETTICKCOUNT64)GetProcAddress(hKernel32, "GetTickCount64") : NULL;
    }

    if (s_pfnGetTickCount64 != NULL) {
        return s_pfnGetTickCount64();
    }

    static DWORD s_LastTick = 0;
    static DWORD s_WrapCount = 0;
    DWORD currentTick;
    DWORD lastTick;
    ULONGLONG result;

    currentTick = GetTickCount();
    lastTick = (DWORD)InterlockedExchangeAdd((LPLONG)&s_LastTick, 0);

    if (currentTick < lastTick) {
        InterlockedIncrement((LPLONG)&s_WrapCount);
    }
    InterlockedExchange((LPLONG)&s_LastTick, currentTick);
    result = (ULONGLONG)s_WrapCount << 32;
    result |= currentTick;

    return result;
}

static BOOL CALLBACK EnumLocalesProc(LPWSTR lpLocaleString) {
    LCID lcid;
    LOCALE_SEARCH* search;
    wchar_t lang[10];
    wchar_t country[10];
    wchar_t full[32];

    lcid = (LCID)wcstoul(lpLocaleString, NULL, 16);
    search = (LOCALE_SEARCH*)GetPropW(GetDesktopWindow(), L"LocSearch");

    if (!search) return FALSE;

    if (GetLocaleInfoW(lcid, LOCALE_SISO639LANGNAME, lang, 10)) {
        if (GetLocaleInfoW(lcid, LOCALE_SISO3166CTRYNAME, country, 10)) {
            wsprintfW(full, L"%s-%s", lang, country);
            if (CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, 
                               full, -1, search->TargetName, -1) == 2) {
                search->ResultLcid = lcid;
                return FALSE; 
            }
        }
    }

    return TRUE; 
}
LCID WINAPI Ext_LocaleNameToLCID(LPCWSTR lpName, DWORD dwFlags) {
    LOCALE_SEARCH search;

    if (!lpName) return 0;

    if (lpName[0] == L'\0' || lstrcmpiW(lpName, L"LOCALE_NAME_USER_DEFAULT") == 0) {
        return GetUserDefaultLCID();
    }
    if (lstrcmpiW(lpName, L"LOCALE_NAME_SYSTEM_DEFAULT") == 0) {
        return GetSystemDefaultLCID();
    }

    search.TargetName = lpName;
    search.ResultLcid = 0;

    SetPropW(GetDesktopWindow(), L"LocSearch", (HANDLE)&search);
    EnumSystemLocalesW(EnumLocalesProc, LCID_INSTALLED);
    RemovePropW(GetDesktopWindow(), L"LocSearch");

    if (search.ResultLcid == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    return search.ResultLcid;
}

VOID WINAPI Ext_FlushProcessWriteBuffers(VOID) {
    static DWORD volatile dummy = 0;
    DWORD oldProtect;
    VirtualProtect((LPVOID)&dummy, sizeof(DWORD), PAGE_READONLY, &oldProtect);
    VirtualProtect((LPVOID)&dummy, sizeof(DWORD), PAGE_READWRITE, &oldProtect);
}

BOOL WINAPI Ext_IsThreadAFiber(VOID) {
    return (GetCurrentFiber() != NULL && GetCurrentFiber() != (PVOID)0x1e00);
}

// inline helper func
static VOID CALLBACK XPTimerCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired) {
    PTP_TIMER pti = (PTP_TIMER)lpParameter;
    if (pti && pti->callback) {
        pti->callback(NULL, pti->context, pti);
    }
}

PTP_TIMER WINAPI Ext_CreateThreadpoolTimer(PTP_TIMER_CALLBACK pfnti, PVOID pv, PVOID pcbe) {
    PTP_TIMER pti = (PTP_TIMER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TP_TIMER_WRAPPER));
    if (pti) {
        pti->callback = pfnti;
        pti->context = pv;
        pti->hTimer = NULL;
    }
    return pti;
}

VOID WINAPI Ext_SetThreadpoolTimer(PTP_TIMER pti, PFILETIME pftDueTime, DWORD msPeriod, DWORD msWindowLength) {
    if (!pti) return;

    if (pti->hTimer) {
        DeleteTimerQueueTimer(NULL, pti->hTimer, INVALID_HANDLE_VALUE);
        pti->hTimer = NULL;
    }
    if (pftDueTime) {
        ULARGE_INTEGER now, due;
        DWORD dueMs = 0;

        GetSystemTimeAsFileTime((LPFILETIME)&now);
        due.LowPart = pftDueTime->dwLowDateTime;
        due.HighPart = pftDueTime->dwHighDateTime;

        if (due.QuadPart == 0) {
            dueMs = 0;
        } else if ((LONGLONG)due.QuadPart < 0) {
            dueMs = (DWORD)((-(LONGLONG)due.QuadPart) / 10000);
        } else {
            if (due.QuadPart > now.QuadPart) {
                dueMs = (DWORD)((due.QuadPart - now.QuadPart) / 10000);
            } else {
                dueMs = 0;
            }
        }

        if (!CreateTimerQueueTimer(&pti->hTimer, NULL, XPTimerCallback, pti, dueMs, msPeriod, WT_EXECUTEDEFAULT)) {
            pti->hTimer = NULL;
        }
    }
}

VOID WINAPI Ext_CloseThreadpoolTimer(PTP_TIMER pti) {
    if (!pti) return;

    if (pti->hTimer) {
        DeleteTimerQueueTimer(NULL, pti->hTimer, INVALID_HANDLE_VALUE);
    }
    
    HeapFree(GetProcessHeap(), 0, pti);
}

BOOL WINAPI Ext_GetNLSVersionEx(NLS_FUNCTION Function, LPCWSTR lpLocaleName, LPNLSVERSIONINFOEX lpVersionInformation) {
    LCID lcid;

    if (!lpVersionInformation) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    #if defined(_M_IX86)
        if (g_pfnGetNLSVersion == NULL) {
            return TRUE;
        }
    #endif

    lcid = Ext_LocaleNameToLCID(lpLocaleName, 0);
    if (lcid == 0) {
        return FALSE;
    }
    #if defined(_M_IX86)
    return g_pfnGetNLSVersion(Function, lcid, (LPNLSVERSIONINFO)lpVersionInformation);
    #else
    return GetNLSVersion(Function, lcid, (LPNLSVERSIONINFO)lpVersionInformation);
    #endif
}

BOOL WINAPI Ext_QueryThreadCycleTime(HANDLE ThreadHandle, PULONG64 CycleTime) {
    FILETIME CreationTime, ExitTime, KernelTime, UserTime;
    ULARGE_INTEGER K, U;

    if (!CycleTime) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (GetThreadTimes(ThreadHandle, &CreationTime, &ExitTime, &KernelTime, &UserTime)) {
        K.LowPart = KernelTime.dwLowDateTime;
        K.HighPart = KernelTime.dwHighDateTime;

        U.LowPart = UserTime.dwLowDateTime;
        U.HighPart = UserTime.dwHighDateTime;
        *CycleTime = K.QuadPart + U.QuadPart;
        return TRUE;
    }

    return FALSE;
}

BOOL WINAPI Ext_InitializeCriticalSectionEx(
    LPCRITICAL_SECTION lpCriticalSection,
    DWORD dwSpinCount,
    DWORD dwFlags
) {
    return InitializeCriticalSectionAndSpinCount(lpCriticalSection, dwSpinCount);
}

int WINAPI Ext_LCMapStringEx(
    LPCWSTR lpLocaleName,
    DWORD dwMapFlags,
    LPCWSTR lpSrcStr,
    int cchSrc,
    LPWSTR lpDestStr,
    int cchDest,
    LPNLSVERSIONINFO lpVersionInformation,
    LPVOID lpReserved,
    LPARAM lParam)
{
    LCID lcid;
    lcid = Ext_LocaleNameToLCID(lpLocaleName, 0);

    if (lcid == 0) {
        return 0;
    }
    return LCMapStringW(lcid, dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest);
}

DWORD WINAPI Ext_GetDynamicTimeZoneInformation(PDYNAMIC_TIME_ZONE_INFORMATION pDynamicTimeZoneInformation) {
    TIME_ZONE_INFORMATION tzi;
    DWORD dwResult;
    HKEY hKey;
    LONG lRet;
    DWORD dwType, cbData;
    DWORD dwDisabled = 0;

    if (!pDynamicTimeZoneInformation) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return TIME_ZONE_ID_UNKNOWN;
    }

    ZeroMemory(pDynamicTimeZoneInformation, sizeof(DYNAMIC_TIME_ZONE_INFORMATION));

    dwResult = GetTimeZoneInformation(&tzi);
    if (dwResult == TIME_ZONE_ID_INVALID) {
        return TIME_ZONE_ID_INVALID;
    }

    pDynamicTimeZoneInformation->Bias = tzi.Bias;
    lstrcpyW(pDynamicTimeZoneInformation->StandardName, tzi.StandardName);
    pDynamicTimeZoneInformation->StandardDate = tzi.StandardDate;
    pDynamicTimeZoneInformation->StandardBias = tzi.StandardBias;
    lstrcpyW(pDynamicTimeZoneInformation->DaylightName, tzi.DaylightName);
    pDynamicTimeZoneInformation->DaylightDate = tzi.DaylightDate;
    pDynamicTimeZoneInformation->DaylightBias = tzi.DaylightBias;

    lRet = RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                         L"SYSTEM\\CurrentControlSet\\Control\\TimeZoneInformation", 
                         0, KEY_READ, &hKey);
                         
    if (lRet == ERROR_SUCCESS) {
        cbData = sizeof(pDynamicTimeZoneInformation->TimeZoneKeyName);
        lRet = RegQueryValueExW(hKey, L"TimeZoneKeyName", NULL, &dwType, 
                                (LPBYTE)pDynamicTimeZoneInformation->TimeZoneKeyName, &cbData);
                                
        if (lRet != ERROR_SUCCESS) {
            cbData = sizeof(pDynamicTimeZoneInformation->TimeZoneKeyName);
            lRet = RegQueryValueExW(hKey, L"StandardName", NULL, &dwType, 
                                    (LPBYTE)pDynamicTimeZoneInformation->TimeZoneKeyName, &cbData);
        }
        
        cbData = sizeof(DWORD);
        lRet = RegQueryValueExW(hKey, L"DynamicDaylightTimeDisabled", NULL, &dwType, 
                                (LPBYTE)&dwDisabled, &cbData);
        if (lRet == ERROR_SUCCESS) {
            pDynamicTimeZoneInformation->DynamicDaylightTimeDisabled = (dwDisabled != 0);
        } else {
            pDynamicTimeZoneInformation->DynamicDaylightTimeDisabled = FALSE;
        }

        RegCloseKey(hKey);
    }

    if (pDynamicTimeZoneInformation->TimeZoneKeyName[0] == L'\0') {
        lstrcpynW(pDynamicTimeZoneInformation->TimeZoneKeyName, tzi.StandardName, 128);
    }

    return dwResult;
}

FARPROC WINAPI Ext_GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    if (lpProcName && ((ULONG_PTR)lpProcName > 0xFFFF)) {
        if (lpProcName[0] == 'G' && lpProcName[3] == 'D' && lpProcName[10] == 'T' && lpProcName[29] == '\0') {
            if (lstrcmpA(lpProcName, "GetDynamicTimeZoneInformation") == 0) {
                return (FARPROC)Ext_GetDynamicTimeZoneInformation;
            }
        }
    }
    return GetProcAddress(hModule, lpProcName);
}

// NT 5.1 extensions
#if defined(_M_IX86) || defined(__i386__)
DWORD WINAPI Ext_GetCurrentProcessorNumber(VOID) {
    SYSTEM_INFO si;

    InitK32Functions();
    if (g_pfnGetCurrentProcessorNumber != NULL) {
        return g_pfnGetCurrentProcessorNumber();
    }
    
    GetSystemInfo(&si);
    if (si.dwNumberOfProcessors > 1) {
        return (DWORD)(GetCurrentThreadId() % si.dwNumberOfProcessors);
    }
    return 0;
}
BOOL WINAPI Ext_SetThreadStackGuarantee(PULONG StackSizeInBytes) {
    static ULONG s_FakeGuaranteedBytes = 0; 
    ULONG AllocationSize;
    ULONG PreviousSize;

    if (!StackSizeInBytes) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    InitK32Functions();
    if (g_pfnSetThreadStackGuarantee != NULL) {
        return g_pfnSetThreadStackGuarantee(StackSizeInBytes);
    }

    AllocationSize = *StackSizeInBytes;
    PreviousSize = s_FakeGuaranteedBytes;
    *StackSizeInBytes = PreviousSize;
    if ((AllocationSize == 0) || (AllocationSize <= PreviousSize)) {
        return TRUE;
    }
    s_FakeGuaranteedBytes = (AllocationSize + 4095) & ~4095;
    return TRUE;
}

BOOL WINAPI Ext_SetFileCompletionNotificationModes(HANDLE FileHandle, UCHAR Flags) {
    NTSTATUS Status;
    FILE_IO_COMPLETION_NOTIFICATION_INFORMATION FileInformation;
    IO_STATUS_BLOCK IoStatusBlock;

    InitK32Functions();
    if (g_pfnSetFileCompletionNotificationModes != NULL) {
        return g_pfnSetFileCompletionNotificationModes(FileHandle, Flags);
    }

    if (Flags & ~(FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    FileInformation.Flags = Flags;

    Status = NtSetInformationFile(
        FileHandle,
        &IoStatusBlock,
        &FileInformation,
        sizeof(FileInformation),
        FileIoCompletionNotificationInformation
    );

    if (!NT_SUCCESS(Status)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return TRUE;
}

BOOL WINAPI Ext_GetNLSVersion(NLS_FUNCTION Function, LCID Locale, LPNLSVERSIONINFO lpVersionInformation) {
    if (!lpVersionInformation) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    InitK32Functions();
    if (g_pfnGetNLSVersion != NULL) {
        return g_pfnGetNLSVersion(Function, Locale, lpVersionInformation);
    }

    return TRUE;
}

#endif

int WINAPI Ext_LCIDToLocaleName(LCID Locale, LPWSTR lpName, int cchName, DWORD dwFlags) {
    typedef int (WINAPI *PFN_LCIDToLocaleName)(LCID, LPWSTR, int, DWORD);
    static PFN_LCIDToLocaleName s_pfnLCIDToLocaleName = (PFN_LCIDToLocaleName)-1;

    if (s_pfnLCIDToLocaleName == (PFN_LCIDToLocaleName)-1) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        s_pfnLCIDToLocaleName = hKernel32 ? (PFN_LCIDToLocaleName)GetProcAddress(hKernel32, "LCIDToLocaleName") : NULL;
    }

    if (s_pfnLCIDToLocaleName != NULL) {
        return s_pfnLCIDToLocaleName(Locale, lpName, cchName, dwFlags);
    }

    wchar_t lang[10] = {0};
    wchar_t country[10] = {0};
    if (GetLocaleInfoW(Locale, LOCALE_SISO639LANGNAME, lang, 10)) {
        if (GetLocaleInfoW(Locale, LOCALE_SISO3166CTRYNAME, country, 10) && country[0] != L'\0') {
            return wsprintfW(lpName, L"%s-%s", lang, country);
        }
        return wsprintfW(lpName, L"%s", lang);
    }
    return 0;
}

typedef VOID (WINAPI *PFN_InitializeConditionVariable)(PCONDITION_VARIABLE);
typedef BOOL (WINAPI *PFN_SleepConditionVariableCS)(PCONDITION_VARIABLE, PCRITICAL_SECTION, DWORD);
typedef VOID (WINAPI *PFN_WakeConditionVariable)(PCONDITION_VARIABLE);
typedef VOID (WINAPI *PFN_WakeAllConditionVariable)(PCONDITION_VARIABLE);

static PFN_InitializeConditionVariable s_pfnInitCV = (PFN_InitializeConditionVariable)-1;
static PFN_SleepConditionVariableCS s_pfnSleepCV = NULL;
static PFN_WakeConditionVariable s_pfnWakeCV = NULL;
static PFN_WakeAllConditionVariable s_pfnWakeAllCV = NULL;

static void InitConditionVariablePointers(void) {
    if (s_pfnInitCV == (PFN_InitializeConditionVariable)-1) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (hKernel32) {
            s_pfnInitCV = (PFN_InitializeConditionVariable)GetProcAddress(hKernel32, "InitializeConditionVariable");
            s_pfnSleepCV = (PFN_SleepConditionVariableCS)GetProcAddress(hKernel32, "SleepConditionVariableCS");
            s_pfnWakeCV = (PFN_WakeConditionVariable)GetProcAddress(hKernel32, "WakeConditionVariable");
            s_pfnWakeAllCV = (PFN_WakeAllConditionVariable)GetProcAddress(hKernel32, "WakeAllConditionVariable");
        } else {
            s_pfnInitCV = NULL;
        }
    }
}

VOID WINAPI Ext_InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable) {
    InitConditionVariablePointers();
    if (s_pfnInitCV) {
        s_pfnInitCV(ConditionVariable);
        return;
    }
    if (ConditionVariable)
        ConditionVariable->Ptr = CreateEventW(NULL, FALSE, FALSE, NULL);
}

BOOL WINAPI Ext_SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds) {
    InitConditionVariablePointers();
    if (s_pfnSleepCV)
        return s_pfnSleepCV(ConditionVariable, CriticalSection, dwMilliseconds);

    if (!ConditionVariable || !ConditionVariable->Ptr)
        return FALSE;

    HANDLE hEvent = (HANDLE)ConditionVariable->Ptr;
    LeaveCriticalSection(CriticalSection);
    DWORD dwWait = WaitForSingleObject(hEvent, dwMilliseconds);
    EnterCriticalSection(CriticalSection);

    if (dwWait == WAIT_OBJECT_0)
        return TRUE;
    if (dwWait == WAIT_TIMEOUT)
        SetLastError(ERROR_TIMEOUT);
    return FALSE;
}

VOID WINAPI Ext_WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable) {
    InitConditionVariablePointers();
    if (s_pfnWakeCV) {
        s_pfnWakeCV(ConditionVariable);
        return;
    }
    if (ConditionVariable && ConditionVariable->Ptr)
        SetEvent((HANDLE)ConditionVariable->Ptr);
}

VOID WINAPI Ext_WakeAllConditionVariable(PCONDITION_VARIABLE ConditionVariable) {
    InitConditionVariablePointers();
    if (s_pfnWakeAllCV) {
        s_pfnWakeAllCV(ConditionVariable);
        return;
    }
    if (ConditionVariable && ConditionVariable->Ptr)
        PulseEvent((HANDLE)ConditionVariable->Ptr);
}

LANGID WINAPI Ext_GetThreadUILanguage(VOID) {
    typedef LANGID (WINAPI *PFN_GetThreadUILanguage)(VOID);
    static PFN_GetThreadUILanguage s_pfnGetThreadUILang = (PFN_GetThreadUILanguage)-1;

    if (s_pfnGetThreadUILang == (PFN_GetThreadUILanguage)-1) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        s_pfnGetThreadUILang = hKernel32 ? (PFN_GetThreadUILanguage)GetProcAddress(hKernel32, "GetThreadUILanguage") : NULL;
    }

    if (s_pfnGetThreadUILang != NULL)
        return s_pfnGetThreadUILang();

    return GetUserDefaultUILanguage();
}

LANGID WINAPI Ext_SetThreadUILanguage(LANGID LangId) {
    typedef LANGID (WINAPI *PFN_SetThreadUILanguage)(LANGID);
    static PFN_SetThreadUILanguage s_pfnSetThreadUILang = (PFN_SetThreadUILanguage)-1;

    if (s_pfnSetThreadUILang == (PFN_SetThreadUILanguage)-1) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        s_pfnSetThreadUILang = hKernel32 ? (PFN_SetThreadUILanguage)GetProcAddress(hKernel32, "SetThreadUILanguage") : NULL;
    }

    if (s_pfnSetThreadUILang != NULL)
        return s_pfnSetThreadUILang(LangId);

    SetThreadLocale(MAKELCID(LangId, SORT_DEFAULT));
    return LangId;
}