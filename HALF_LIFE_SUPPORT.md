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

These interfaces were being passed through to the real `steamclient.dll`, which doesn't provide them.

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
Added `TrySourceEngineInterface()` to intercept Source engine interface requests BEFORE passing to real steamclient.dll:

```cpp
static void* TrySourceEngineInterface(const char* ver) {
    if (strcmp(ver, "FILESYSTEM_INTERFACE_VERSION") == 0 || ...) {
        return GetSourceFilesystemStub();
    }
    if (strcmp(ver, "SYS_INTERFACE_VERSION") == 0) {
        return GetSourceSysStub();
    }
    if (strcmp(ver, "TIER0_INTERFACE_VERSION") == 0) {
        return GetSourceTier0Stub();
    }
    return nullptr;
}
```

### 3. `dllmain.cpp` (MODIFIED)
Added include for the new header:
```cpp
#include "include/api/api_source.h"
```

## How to Test
1. Build the project
2. Copy `steam_api.dll`/`steam_api64.dll` to Half-Life directory
3. Run Half-Life - errors should be resolved
4. Check `%TEMP%\uc_online2.log` for interface interception logs:
```
[UCOnline2] Intercepting FILESYSTEM interface -> FILESYSTEM_INTERFACE_VERSION
[UCOnline2] Intercepting SYS interface -> SYS_INTERFACE_VERSION
```

## Notes
- These are minimal stubs - full implementation would require replicating Valve's filesystem layer
- The stubs prevent crashes but game-specific features (save synchronization) may not work
- Goldberg Emu has more complete implementations if you need full Source engine compatibility