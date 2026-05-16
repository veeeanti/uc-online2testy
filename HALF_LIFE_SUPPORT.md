# Half-Life / GoldSrc Support Implementation

## Problem
Half-Life fails with errors:
```
Missing shutdown function for FileSystem_Init(basedir, (void *)filesystemFactory) : FileSystem_Shutdown()
Missing shutdown function for Sys_InitArgv( OrigCmd ) : Sys_ShutdownArgv()
```

## Root Cause
GoldSrc (Half-Life 1) requests interfaces that are NOT part of the Steam API:
- `VFileSystem009` (from `FILESYSTEM_INTERFACE_VERSION` macro) - filesystem access
- `VSys001` - command line handling
- `VTier001` - memory allocation

Half-Life's `filesystem_stdio.dll` calls `interfaceFactory("VFileSystem009", NULL)` directly via GetProcAddress.

## Solution

### Interface Version Strings (FIXED)
The key insight from GoldSrc SDK:
```cpp
#define FILESYSTEM_INTERFACE_VERSION "VFileSystem009"  // Expands to "VFileSystem009"
```

We now match the **expanded macro value**, not the macro name itself.

### 1. `include/api/api_factory.h`
Added interface interception for GoldSrc versions:
- `VFileSystem009`, `VFileSystem008`, ... `VFileSystem001`
- `VSys001`, `VSys002`
- `VTier001`

Exports:
- `CreateInterface()` - Direct export for Source modules
- `interfaceFactory()` - Primary entry point for `filesystem_stdio.dll`

### 2. `include/api/api_source.h`
Revised stubs matching `IFileSystem` vtable from GoldSrc SDK:
- `CFilesystemStub` - Full `IFileSystem` method implementations
- `CSysStub` - Command line handling
- `CTier0Stub` - Memory allocation

## How to Test
1. Build: `msbuild uc_online2.vcxproj /p:Configuration=Release /p:Platform=x64`
2. Copy `build\x64\steam_api64.dll` to Half-Life directory
3. Run Half-Life - check `%TEMP%\uc_online2.log` for:
```
[UCOnline2] interfaceFactory -> VFileSystem009
[UCOnline2] Intercepting FILESYSTEM interface -> VFileSystem009
```