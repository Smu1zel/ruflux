#define RUX_NO_NT5_WRAPPERS 1
#include <windows.h>
#include <evntprov.h>

#ifdef _USRDLL
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    return TRUE;
}
#endif

ULONG WINAPI Ext_EventRegister(
    LPCGUID ProviderId,
    PENABLECALLBACK EnableCallback,
    PVOID CallbackContext,
    PREGHANDLE RegHandle
)
{
    if (RegHandle)
        *RegHandle = 0;
    
    return ERROR_NOT_SUPPORTED;
}

ULONG WINAPI Ext_EventUnregister(
    REGHANDLE RegHandle
)
{
    return ERROR_SUCCESS;
}

ULONG WINAPI Ext_EventWrite(
    REGHANDLE RegHandle,
    PCEVENT_DESCRIPTOR EventDescriptor,
    ULONG UserDataCount,
    PEVENT_DATA_DESCRIPTOR UserData
)
{
    return ERROR_NOT_SUPPORTED;
}

ULONG WINAPI Ext_EventWriteTransfer(
    REGHANDLE RegHandle,
    PCEVENT_DESCRIPTOR EventDescriptor,
    LPCGUID ActivityId,
    LPCGUID RelatedActivityId,
    ULONG UserDataCount,
    PEVENT_DATA_DESCRIPTOR UserData
)
{
    return ERROR_NOT_SUPPORTED;
}

/* RegGetValue implementation, added manually */
typedef LSTATUS (WINAPI *PFN_RegGetValueA)(HKEY, LPCSTR, LPCSTR, DWORD, LPDWORD, PVOID, LPDWORD);
typedef LSTATUS (WINAPI *PFN_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);

static PFN_RegGetValueA g_pfnRegGetValueA = NULL;
static PFN_RegGetValueW g_pfnRegGetValueW = NULL;
static BOOL g_bAdvInit = FALSE;

static void InitAdvapiFunctions(void) {
    if (!g_bAdvInit) {
        HMODULE hAdvapi = GetModuleHandleA("advapi32.dll");
        if (hAdvapi) {
            g_pfnRegGetValueA = (PFN_RegGetValueA)GetProcAddress(hAdvapi, "RegGetValueA");
            g_pfnRegGetValueW = (PFN_RegGetValueW)GetProcAddress(hAdvapi, "RegGetValueW");
        }
        g_bAdvInit = TRUE;
    }
}

static BOOL CheckRegTypeFlags(DWORD dwType, DWORD dwFlags) {
    if (dwFlags == 0 || (dwFlags & 0x0000ffff) == 0x0000ffff) /* RRF_RT_ANY */
        return TRUE;
    switch (dwType) {
        case REG_NONE:      return (dwFlags & 0x00000001) != 0; /* RRF_RT_REG_NONE */
        case REG_SZ:        return (dwFlags & 0x00000002) != 0; /* RRF_RT_REG_SZ */
        case REG_EXPAND_SZ: return (dwFlags & 0x00000004) != 0; /* RRF_RT_REG_EXPAND_SZ */
        case REG_BINARY:    return (dwFlags & 0x00000008) != 0; /* RRF_RT_REG_BINARY */
        case REG_DWORD:     return (dwFlags & 0x00000010) != 0; /* RRF_RT_REG_DWORD */
        case REG_MULTI_SZ:  return (dwFlags & 0x00000020) != 0; /* RRF_RT_REG_MULTI_SZ */
        case REG_QWORD:     return (dwFlags & 0x00000040) != 0; /* RRF_RT_REG_QWORD */
        default:            return TRUE;
    }
}

LSTATUS WINAPI Ext_RegGetValueA(
    HKEY hkey,
    LPCSTR lpSubKey,
    LPCSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData
)
{
    HKEY hTargetKey = hkey;
    HKEY hSubKey = NULL;
    DWORD dwType = REG_NONE;
    LSTATUS status;

    InitAdvapiFunctions();
    if (g_pfnRegGetValueA != NULL)
        return g_pfnRegGetValueA(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);

    if (lpSubKey && *lpSubKey) {
        status = RegOpenKeyExA(hkey, lpSubKey, 0, KEY_QUERY_VALUE, &hSubKey);
        if (status != ERROR_SUCCESS)
            return status;
        hTargetKey = hSubKey;
    }

    status = RegQueryValueExA(hTargetKey, lpValue, NULL, &dwType, (LPBYTE)pvData, pcbData);
    if (hSubKey)
        RegCloseKey(hSubKey);

    if (status == ERROR_SUCCESS) {
        if (!CheckRegTypeFlags(dwType, dwFlags))
            return ERROR_DATATYPE_MISMATCH;
        if (pdwType)
            *pdwType = dwType;
    }

    return status;
}

LSTATUS WINAPI Ext_RegGetValueW(
    HKEY hkey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData
)
{
    HKEY hTargetKey = hkey;
    HKEY hSubKey = NULL;
    DWORD dwType = REG_NONE;
    LSTATUS status;

    InitAdvapiFunctions();
    if (g_pfnRegGetValueW != NULL)
        return g_pfnRegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);

    if (lpSubKey && *lpSubKey) {
        status = RegOpenKeyExW(hkey, lpSubKey, 0, KEY_QUERY_VALUE, &hSubKey);
        if (status != ERROR_SUCCESS)
            return status;
        hTargetKey = hSubKey;
    }

    status = RegQueryValueExW(hTargetKey, lpValue, NULL, &dwType, (LPBYTE)pvData, pcbData);
    if (hSubKey)
        RegCloseKey(hSubKey);

    if (status == ERROR_SUCCESS) {
        if (!CheckRegTypeFlags(dwType, dwFlags))
            return ERROR_DATATYPE_MISMATCH;
        if (pdwType)
            *pdwType = dwType;
    }

    return status;
}