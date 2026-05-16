# Half-Life Fix Analysis

## Root Cause

The error messages:
```
Missing shutdown function for FileSystem_Init(basedir, (void *)filesystemFactory) : FileSystem_Shutdown()
Missing shutdown function for Sys_InitArgv( OrigCmd ) : Sys_ShutdownArgv()
```

Are coming from **`filesystem_stdio.dll`** (bundled with Half-Life), NOT from `steam_api.dll`.

## Execution Flow

1. Half-Life (`hl.exe`) loads our `steam_api.dll` proxy
2. Half-Life loads `filesystem_stdio.dll` (Valve's Steam implementation)
3. `filesystem_stdio.dll` **directly loads `steamclient.dll`** via `LoadLibrary`
4. `filesystem_stdio.dll` calls `CreateInterface` on `steamclient.dll` to get:
   - `FILESYSTEM_INTERFACE_VERSION`
   - `SYS_INTERFACE_VERSION`
   - `TIER0_INTERFACE_VERSION`
5. Since `steamclient.dll` doesn't provide these Source engine interfaces (they're only in Steam with Source SDK integration), the calls fail
6. The tier system in `filesystem_stdio.dll` reports "Missing shutdown function" because the interfaces never initialized

## Solution Options

### Option 1: Hook filesystem_stdio.dll (Recommended)
The core DLL needs to hook `filesystem_stdio.dll`'s calls to `CreateInterface` on `steamclient.dll` and intercept them.

### Option 2: Provide filesystem_stdio.dll replacement
Some implementations (Goldberg Emu) include their own `filesystem_stdio.dll` that provides these interfaces.

### Option 3: Hook steamclient.dll CreateInterface
Intercept ALL `CreateInterface` calls regardless of calling module, providing our stubs.

## Current Implementation Status

- ✅ `steam_api.dll` intercepts `SteamInternal_CreateInterface`
- ✅ `steam_api.dll` exports `CreateInterface` directly  
- ❌ `filesystem_stdio.dll` bypasses `steam_api.dll` and calls `steamclient.dll` directly
- ❌ Need to hook `filesystem_stdio.dll` or replace it

## Next Steps

1. Add a hook in `uc_online2_core.cpp` to monitor `filesystem_stdio.dll`'s `CreateInterface` calls
2. OR bundle a modified `filesystem_stdio.dll` that doesn't require these interfaces
3. OR use MinHook to detour `steamclient.dll`'s `CreateInterface`