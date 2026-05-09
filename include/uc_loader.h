/**
 *  This is the replacement for dll_loader.h, which was no longer named correctly
 *  and was a mess of different features that I just threw in there without thinking
 *  about anything. This is the header for uc_online2_core, which is the 'client' dll
 *  that now handles everything that was written in the old dll_loader header and then
 *  some. All that's here now is the declarations for the core DLL's exported functions, 
 *  the actual code for plugin injection/loading, steamstubbed, some appid stuff, are
 *  all located in the uc_online2_core.cpp and dllmain.cpp files. 
 *
 *  ~veeλnti<3 2026
 */

#pragma once

#include <Windows.h>
#include <cstdint>

#ifdef UC_CORE_EXPORTS
#define UC_CORE_API __declspec(dllexport)
#else
#define UC_CORE_API __declspec(dllimport)
#endif

extern "C" {

typedef void (WINAPI* UC_Core_Init_t)();
typedef void (WINAPI* UC_Core_Shutdown_t)();
typedef uint32_t (WINAPI* UC_Core_GetAppId_t)();
typedef uint32_t (WINAPI* UC_Core_GetOgAppId_t)();

UC_CORE_API void UC_Core_Init();
UC_CORE_API void UC_Core_Shutdown();

UC_CORE_API uint32_t UC_Core_GetAppId();
UC_CORE_API uint32_t UC_Core_GetOgAppId();
UC_CORE_API bool UC_Core_GetSteamStubEnabled();

UC_CORE_API void UC_Core_SetAppIDEnv();
UC_CORE_API void UC_Core_WriteAppIDFile();

} // extern "C"