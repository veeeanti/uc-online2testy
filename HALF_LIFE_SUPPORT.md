# Half-Life / Source Engine Support Implementation

## Problem
Half-Life fails with errors:
```
Missing shutdown function for FileSystem_Init(basedir, (void *)filesystemFactory) : FileSystem_Shutdown()
Missing shutdown function for Sys_InitArgv( OrigCmd ) : Sys_ShutdownArgv()
```

## Root Cause
Source engine games (Half-Life, CS, etc.) request interfaces that are NOT part of the Steam API:
- `FILESYSTEM_INTERFACE_VERSION` - filesystem access for save games/configs
- `SYS_INTERFACE_VERSION` - system/command line handling  
- `TIER0_INTERFACE_VERSION` - memory allocation

Half-Life's `filesystem_stdio.dll` calls `interfaceFactory()` directly via GetProcAddress, expecting these interfaces from `steamclient.dll`.

## Solution

### 1. `include/api/api_source.h` (NEW)
Provides stub implementations for Source engine platform interfaces:

```cpp
// CFilesystemStub - Half-Life filesystem interface
class CFilesystemStub {
    bool Init(const char* basedir, void* filesystemFactory);
    void Shutdown();
    bool FileExists(const char* path, const char* pathID = nullptr);
    void* Open(const char* fileName, const char* options, const char* pathID = nullptr);
    int Read(void* pOutput, int size, void* handle);
    void Close(void* handle);
};

// CSysStub - Command line handling
class CSysStub {
    bool InitArgv(const char* cmdLine);
    void ShutdownArgv();
    int GetArgc() const;
    const char* GetArgv(int index) const;
};

// CTier0Stub - Memory allocation
class CTier0Stub {
    void* MemAlloc(size_t size);
    void MemFree(void* pMem);
};
```

### 2. `include/api/api_factory.h` (MODIFIED)
Added two exports for interface interception:

**`interfaceFactory()`** - Called by Half-Life's `filesystem_stdio.dll`:
```cpp
S_API void* S_CALLTYPE interfaceFactory(const char* ver, void* pFactory)
{
    // First check if this is a Source engine platform interface
    void* srcIface = TrySourceEngineInterface(ver);
    if (srcIface)
        return srcIface;
    return CreateInterface(ver, nullptr);
}
```

**`CreateInterface()`** - Direct export for Source engine modules:
```cpp
S_API void* S_CALLTYPE CreateInterface(const char* ver, int* pReturnCode)
{
    void* srcIface = TrySourceEngineInterface(ver);
    if (srcIface) {
        if (pReturnCode) *pReturnCode = 0; // S_OK
        return srcIface;
    }
    // Fall through to real steamclient...
}
```

### 3. `dllmain.cpp` (MODIFIED)
Added include for the new header:
```cpp
#include "include/api/api_source.h"
```

## How to Test
1. Build the project (Visual Studio: `msbuild uc_online2.vcxproj /p:Configuration=Release /p:Platform=x64`)
2. Copy `build\x64\steam_api64.dll` to Half-Life directory
3. Run Half-Life - errors should be resolved

## Expected Log Output
Check `%TEMP%\uc_online2.log`:
```
[UCOnline2] interfaceFactory -> FILESYSTEM_INTERFACE_VERSION
[UCOnline2] Intercepting FILESYSTEM interface -> FILESYSTEM_INTERFACE_VERSION
[UCOnline2] Intercepting SYS interface -> SYS_INTERFACE_VERSION
```

## Known Limitations
- These are minimal stubs - full implementation would require replicating Valve's filesystem layer
- Save synchronization features may not work
- Goldberg Emu has more complete implementations for full Source engine compatibility