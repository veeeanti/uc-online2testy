#pragma once

#include "api_source.h"
#include <string.h>

// Source engine platform interfaces - must be handled BEFORE steamclient lookup
// These are NOT Steam API interfaces but Source engine internal interfaces
// that games like Half-Life expect to get from steamclient.dll
static void* TrySourceEngineInterface(const char* ver)
{
	if (!ver) return nullptr;

	// Filesystem interface - required for Half-Life save games and config
	// Note: Half-Life uses FILESYSTEM_INTERFACE_VERSION, later Source uses VFileSystem009 etc
	if (strcmp(ver, "FILESYSTEM_INTERFACE_VERSION") == 0 ||
		strcmp(ver, "FILESYSTEM_INIT") == 0 ||
		strcmp(ver, "FileSystem_Startup") == 0)
	{
		UCOLOG("[UCOnline2] Intercepting FILESYSTEM interface -> %s", ver);
		return GetSourceFilesystemStub();
	}

	// Sys interface - required for Half-Life command line handling
	if (strcmp(ver, "SYS_INTERFACE_VERSION") == 0)
	{
		UCOLOG("[UCOnline2] Intercepting SYS interface -> %s", ver);
		return GetSourceSysStub();
	}

	// Tier0 interface - memory allocation etc
	if (strcmp(ver, "TIER0_INTERFACE_VERSION") == 0)
	{
		UCOLOG("[UCOnline2] Intercepting TIER0 interface");
		return GetSourceTier0Stub();
	}

	return nullptr;
}

S_API void* S_CALLTYPE SteamInternal_CreateInterface(const char* ver)
{
	if (ver)
	{
		UCOLOG("[UCOnline2] SteamInternal_CreateInterface -> %s\r\n", ver);

		// First check if this is a Source engine platform interface
		void* srcIface = TrySourceEngineInterface(ver);
		if (srcIface)
			return srcIface;

		HMODULE hMod = g_ClientModule;
		if (g_ServerModule) hMod = g_ServerModule;

		if (hMod)
		{
			g_pfnCreateInterface = (Fn_CreateInterface)GetProcAddress(hMod, "CreateInterface");
			if (g_pfnCreateInterface)
				return g_pfnCreateInterface(ver, nullptr);
		}
	}

	UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] CreateInterface: returning null\r\n");
	return nullptr;
}

S_API void* S_CALLTYPE SteamGameServerInternal_CreateInterface(const char* iface)
{
	if (iface)
	{
		UCOLOG("[UCOnline2] SteamGameServerInternal_CreateInterface -> %s\r\n", iface);

		// Also check for Source engine interfaces (filesystem_stdio may use this path too)
		void* srcIface = TrySourceEngineInterface(iface);
		if (srcIface)
			return srcIface;

		if (g_ServerModule)
		{
			g_pfnCreateInterface = (Fn_CreateInterface)GetProcAddress(g_ServerModule, "CreateInterface");
			if (g_pfnCreateInterface)
				return g_pfnCreateInterface(iface, nullptr);
		}
	}

	UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] GameServerCreateInterface: returning null\r\n");
	return nullptr;
}

S_API void* S_CALLTYPE SteamInternal_FindOrCreateUserInterface(HSteamUser hUser, const char* ver)
{
	if (ver)
	{
		UCOLOG("[UCOnline2] FindOrCreateUserInterface -> %s\r\n", ver);

		if (g_pSteamClient && g_ClientPipe != 0)
		{
			void* pIface = g_pSteamClient->GetISteamGenericInterface(hUser, g_ClientPipe, ver);
			if (!pIface)
				WarnMissingInterface(g_ClientPipe, ver);

			return pIface;
		}

		char msg[MAX_PATH] = { 0 };
		_snprintf_s(msg, MAX_PATH, _TRUNCATE, "[UCOnline2] Tried to access %s before SteamAPI_Init\r\n", ver);
		UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, msg);
	}

	UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] FindOrCreateUserInterface: returning null\r\n");
	return nullptr;
}

S_API void* S_CALLTYPE SteamInternal_FindOrCreateGameServerInterface(HSteamUser hUser, const char* ver)
{
	if (ver)
	{
		UCOLOG("[UCOnline2] FindOrCreateGameServerInterface -> %s\r\n", ver);

		if (g_ServerClient && g_ServerPipe != 0)
		{
			void* pIface = g_ServerClient->GetISteamGenericInterface(hUser, g_ServerPipe, ver);
			if (!pIface)
				WarnMissingInterface(g_ServerPipe, ver);

			return pIface;
		}

		char msg[MAX_PATH] = { 0 };
		_snprintf_s(msg, MAX_PATH, _TRUNCATE, "[UCOnline2] Tried to access %s before GameServer_Init\r\n", ver);
		UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, msg);
	}

	UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] FindOrCreateGameServerInterface: returning null\r\n");
	return nullptr;
}

// Direct CreateInterface export - called by filesystem_stdio.dll and other Source modules
// This is the main entry point that filesystem_stdio.dll uses via GetProcAddress
S_API void* S_CALLTYPE CreateInterface(const char* ver, int* pReturnCode)
{
	if (ver)
	{
		UCOLOG("[UCOnline2] CreateInterface -> %s\r\n", ver);

		// First check if this is a Source engine platform interface
		void* srcIface = TrySourceEngineInterface(ver);
		if (srcIface)
		{
			if (pReturnCode)
				*pReturnCode = 0; // S_OK
			return srcIface;
		}

		HMODULE hMod = g_ClientModule;
		if (g_ServerModule) hMod = g_ServerModule;

		if (hMod)
		{
			g_pfnCreateInterface = (Fn_CreateInterface)GetProcAddress(hMod, "CreateInterface");
			if (g_pfnCreateInterface)
				return g_pfnCreateInterface(ver, pReturnCode);
		}
	}

	UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] CreateInterface (direct): returning null\r\n");
	if (pReturnCode)
		*pReturnCode = 1; // E_FAIL
	return nullptr;
}

// interfaceFactory - alias for filesystem_stdio.dll compatibility (GoldSrc Source)
// Half-Life calls: interfaceFactory( FILESYSTEM_INTERFACE_VERSION, NULL )
S_API void* S_CALLTYPE interfaceFactory(const char* ver, void* pFactory)
{
	UCOLOG("[UCOnline2] interfaceFactory -> %s\r\n", ver ? ver : "(null)");

	// First check if this is a Source engine platform interface
	void* srcIface = TrySourceEngineInterface(ver);
	if (srcIface)
		return srcIface;

	// Fall through to CreateInterface
	return CreateInterface(ver, nullptr);
}