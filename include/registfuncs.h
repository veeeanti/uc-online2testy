/**
 *  This may cause antiviruses to shit huge ass cinderblocks, but oh well.
 *  It is genuinely only to find where Steam is installed to get to the
 *  steamclient(64).dll file, since when you install it, it always adds
 *  the path to your registry. I wish it would just add it to PATH or do
 *  that too so I could get around that possible malware flag, but hey.
 *  Life ain't fair, now is it? lol.
 *
 *  ~veeλnti<3 2026
 */
#pragma once

#include <Windows.h>

LSTATUS GetRegistryDWORD(const char* cszPath, const char* cszKey, DWORD& dwValue)
{
    HKEY hKeyNode = nullptr;
    DWORD dwType = REG_DWORD;
    DWORD dwSize = sizeof(DWORD);

    LSTATUS GetRegKey = ERROR_SUCCESS;

    // Try to open the key in the local machine registry (where Steam is actually installed)
    GetRegKey = RegOpenKeyExA(HKEY_LOCAL_MACHINE, cszPath, 0, KEY_READ | KEY_WOW64_32KEY, &hKeyNode);
    if (GetRegKey != ERROR_SUCCESS)
    {
        // Try the 64-bit view
        GetRegKey = RegOpenKeyExA(HKEY_LOCAL_MACHINE, cszPath, 0, KEY_READ | KEY_WOW64_64KEY, &hKeyNode);
        if (GetRegKey != ERROR_SUCCESS)
        {
            return GetRegKey;
        }
    }

    GetRegKey = RegQueryValueExA(hKeyNode, cszKey, nullptr, &dwType, (LPBYTE)&dwValue, &dwSize);

    RegCloseKey(hKeyNode);

    return GetRegKey;
}

LSTATUS GetRegistryString(const char* cszPath, const char* cszKey, char* szString, unsigned int uMaxBuf)
{
    DWORD dwType = REG_SZ;
    DWORD dwSize = uMaxBuf;

    LSTATUS GetRegKey = ERROR_SUCCESS;
    HKEY hKeyNode = nullptr;

    // Try to open the key in the local machine registry (where Steam is actually installed)
    GetRegKey = RegOpenKeyExA(HKEY_LOCAL_MACHINE, cszPath, 0, KEY_READ | KEY_WOW64_32KEY, &hKeyNode);
    if (GetRegKey != ERROR_SUCCESS)
    {
        // Try the 64-bit view
        GetRegKey = RegOpenKeyExA(HKEY_LOCAL_MACHINE, cszPath, 0, KEY_READ | KEY_WOW64_64KEY, &hKeyNode);
        if (GetRegKey != ERROR_SUCCESS)
        {
            return GetRegKey;
        }
    }

    if (GetRegKey == ERROR_SUCCESS)
    {
        GetRegKey = RegQueryValueExA(hKeyNode, cszKey, nullptr, &dwType, (LPBYTE)szString, &dwSize);
        RegCloseKey(hKeyNode);
    }

    return GetRegKey;
}
