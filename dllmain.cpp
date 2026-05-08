#include <Windows.h>
#include <Shlwapi.h>
#include <new.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define STEAM_API_EXPORTS

#include "include/sdk/steam_api.h"
#include "include/sdk/steamclientpublic.h"
#include "include/sdk/steam_gameserver.h"

#include "include/registfuncs.h"
#include "include/callback_dispatcher.h"
#include "include/globals.h"
#include "include/uc_loader.h"
#include "include/dump_handler.h"

// Compiler does NOT like it that these are at the end, way after where it wants to see them. So we are making it aware of the existence of these functions. This is just to shut it the fuck up and compile lol.
void SetAppIDEnv();
void WriteAppIDFile();

#include "include/api/api_callbacks.h"
#include "include/api/api_client.h"
#include "include/api/api_interfaces.h"
#include "include/api/api_interfaces_v.h"
#include "include/api/api_gameserver.h"
#include "include/api/api_breakpad.h"
#include "include/api/api_shutdown.h"
#include "include/api/api_factory.h"
#include "include/api/api_flat.h"

// ============================================================
// Global variable definitions
// ============================================================

HMODULE g_CoreModule = nullptr;
HMODULE g_ClientModule = nullptr;
HSteamPipe g_ClientPipe = 0;
HSteamUser g_ClientUser = 0;
ISteamClient* g_pSteamClient = nullptr;
ISteamClient* g_pSteamClientSafe = nullptr;
ISteamUtils* g_pUtilsForCallbacks = nullptr;
ISteamController* g_pControllerForCallbacks = nullptr;
ISteamInput* g_pInputForCallbacks = nullptr;
CSteamAPIContext g_ClientCtx;
bool g_bClientReady = false;
SRWLOCK g_CtxLock;

HMODULE g_ServerModule = nullptr;
HSteamPipe g_ServerPipe = 0;
HSteamUser g_ServerUser = 0;
ISteamClient* g_ServerClient = nullptr;
ISteamClient* g_pServerClient = nullptr;
ISteamClient* g_pSteamClientGameServer = nullptr;
ISteamClient* g_pSteamClientGameServer_Latest = nullptr;
ISteamGameServer* g_pGameServer = nullptr;
ISteamUtils* g_pServerUtils = nullptr;
CSteamGameServerAPIContext g_ServerCtx;
bool g_bServerReady = false;
EServerMode g_ServerMode = eServerModeInvalid;

bool g_bUsingBreakpad = false;
bool g_bFullDumps = false;
void* g_BreakpadCtx = nullptr;
PFNPreMinidumpCallback g_BreakpadCallback = nullptr;
char g_BreakpadVer[64] = { 0 };
char g_BreakpadTimestamp[64] = { 0 };
uint32 g_BreakpadAppID = 0;
uint64 g_BreakpadSteamID = 0;

bool g_bTryCatch = false;
int g_DispatchMode = 0;
char g_InstallPath[MAX_PATH] = { 0 };
bool g_bHaveInstallPath = false;
SRWLOCK g_CallbackLock;
uint32 g_ForcedAppId = 480;
uint32 g_OriginalAppId = 0;

Fn_CreateInterface g_pfnCreateInterface = nullptr;
Fn_ReleaseThreadLocal g_pfnReleaseThreadLocal = nullptr;
Fn_IsKnownInterface g_pfnIsKnownInterface = nullptr;
Fn_NotifyMissing g_pfnNotifyMissing = nullptr;
Fn_BreakpadInit g_pfnBreakpadInit = nullptr;
Fn_BreakpadSetAppID g_pfnBreakpadSetAppID = nullptr;
Fn_BreakpadSetSteamID g_pfnBreakpadSetSteamID = nullptr;
Fn_BreakpadSetComment g_pfnBreakpadSetComment = nullptr;
Fn_BreakpadWriteDump g_pfnBreakpadWriteDump = nullptr;

uintp g_CtxCounter = 0;

static UC_Core_Init_t g_pfnCoreInit = nullptr;
static UC_Core_Shutdown_t g_pfnCoreShutdown = nullptr;

// ============================================================
// SteamInternal_ContextInit
// ============================================================

S_API void* S_CALLTYPE SteamInternal_ContextInit(void* pData)
{
    UCOLOG("[UCOnline2] SteamInternal_ContextInit");
    if (!pData) return nullptr;
    void** pArr = (void**)pData;
    uintp* pCounter = (uintp*)&pArr[1];
    char* pBase = (char*)pData;
    #if defined(_M_IX86)
        if (*pCounter == g_CtxCounter) return pBase + 8;
        AcquireSRWLockExclusive(&g_CtxLock);
        if (*pCounter != g_CtxCounter) { void(*pFn)(void*) = (void(*)(void*))pArr[0]; pFn(pBase + 8); *pCounter = g_CtxCounter; }
        ReleaseSRWLockExclusive(&g_CtxLock);
        return pBase + 8;
    #elif defined(_M_AMD64)
        if (*pCounter == g_CtxCounter) return pBase + 16;
        AcquireSRWLockExclusive(&g_CtxLock);
        if (*pCounter != g_CtxCounter) { void(*pFn)(void*) = (void(*)(void*))pArr[0]; pFn(pBase + 16); *pCounter = g_CtxCounter; }
        ReleaseSRWLockExclusive(&g_CtxLock);
        return pBase + 16;
    #endif
}

void UCOLOG(const char* fmt, ...)
{
#ifdef _DEBUG
    if (!fmt) return;
    va_list args;
    va_start(args, fmt);
    
    char msg[2048] = { 0 };
    _vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, args);
    
    char logPath[MAX_PATH] = { 0 };
    DWORD len = GetTempPathA(MAX_PATH, logPath);
    if (len == 0 || len > (MAX_PATH - 25)) { va_end(args); return; }
    
    if (!PathAppendA(logPath, "uc_online2.log")) { va_end(args); return; }
    
    FILE* f = nullptr;
    if (fopen_s(&f, logPath, "ab") != 0 || !f) { va_end(args); return; }
    
    SYSTEMTIME st = { 0 };
    GetLocalTime(&st);
    
    fprintf(f, "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
    
    size_t msgLen = strlen(msg);
    if (msgLen == 0 || msg[msgLen - 1] != '\n')
        fputs("\r\n", f);
    
    fclose(f);
    va_end(args);
#endif
}

void UCOColor(WORD color, const char* text)
{
    (void)color;
#ifdef _DEBUG
    if (text && text[0])
        UCOLOG("%s", text);
#endif
}

void* InitSteamClient(HMODULE* phMod, bool bLocal, const char* iface)
{
    UCOLOG("[UCOnline2] InitSteamClient -> %s", iface);
    
    const char* steamClientPath = "steamclient.dll";
#if defined(_M_AMD64)
    steamClientPath = "steamclient64.dll";
#endif
    
    HMODULE hMod = LoadLibraryA(steamClientPath);
    if (!hMod)
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] Failed to load steamclient.dll\r\n");
        return nullptr;
    }
    
    if (phMod)
        *phMod = hMod;
    
    Fn_CreateInterface pfnCreateInterface = (Fn_CreateInterface)GetProcAddress(hMod, "CreateInterface");
    if (!pfnCreateInterface)
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] Failed to get CreateInterface\r\n");
        return nullptr;
    }
    
    return pfnCreateInterface(iface, nullptr);
}

// ============================================================
// LoadGameOverlay
// ============================================================

static void LoadGameOverlay()
{
#if defined(_WIN32)
    uint32_t forcedAppId = 480;
    if (g_pfnCoreInit)
    {
        forcedAppId = g_ForcedAppId;
    }
    
    #if defined(_M_IX86)
        HMODULE hOverlay = GetModuleHandleW(L"GameOverlayRenderer.dll");
    #elif defined(_M_AMD64)
        HMODULE hOverlay = GetModuleHandleW(L"GameOverlayRenderer64.dll");
    #endif

    if (forcedAppId != 769 && !hOverlay)
    {
        const char* installPath = SteamAPI_GetSteamInstallPath();
        if (_stricmp(installPath, "UCOnline2_InvalidPath") != 0)
        {
            char overlayPath[MAX_PATH] = { 0 };
            #if defined(_M_IX86)
                _snprintf_s(overlayPath, MAX_PATH, _TRUNCATE, "%s\\GameOverlayRenderer.dll", installPath);
            #elif defined(_M_AMD64)
                _snprintf_s(overlayPath, MAX_PATH, _TRUNCATE, "%s\\GameOverlayRenderer64.dll", installPath);
            #endif
            HMODULE hLoaded = LoadLibraryExA(overlayPath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (hLoaded)
            {
                UCOLOG("[UCOnline2] Loaded game overlay: %s", overlayPath);
            }
            else
            {
                UCOLOG("[UCOnline2] Failed to load game overlay: %s (error %lu)", overlayPath, GetLastError());
            }
        }
    }
#endif
}

// ============================================================
// DllMain
// ============================================================

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        UCOLOG("[UCOnline2] DllMain -> DLL_PROCESS_ATTACH");

        InitializeSRWLock(&g_CtxLock);
        InitializeSRWLock(&g_CallbackLock);

        // Load the core DLL
        char corePath[MAX_PATH] = { 0 };
        DWORD len = GetModuleFileNameA(hModule, corePath, sizeof(corePath));
        if (len == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            return FALSE;

        if (!PathRemoveFileSpecA(corePath))
            return FALSE;

        #if defined(_M_IX86)
            _snprintf_s(corePath, MAX_PATH, _TRUNCATE, "%s\\uc_online2_core.dll", corePath);
        #elif defined(_M_AMD64)
            _snprintf_s(corePath, MAX_PATH, _TRUNCATE, "%s\\uc_online2_core.dll", corePath);
        #endif

        g_CoreModule = LoadLibraryExA(corePath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (g_CoreModule)
        {
            g_pfnCoreInit = (UC_Core_Init_t)GetProcAddress(g_CoreModule, "UC_Core_Init");
            g_pfnCoreShutdown = (UC_Core_Shutdown_t)GetProcAddress(g_CoreModule, "UC_Core_Shutdown");
            UC_Core_GetAppId_t pfnGetAppId = (UC_Core_GetAppId_t)GetProcAddress(g_CoreModule, "UC_Core_GetAppId");
            UC_Core_GetOgAppId_t pfnGetOgAppId = (UC_Core_GetOgAppId_t)GetProcAddress(g_CoreModule, "UC_Core_GetOgAppId");

            // Set defaults before core init
            g_ForcedAppId = 480;
            g_OriginalAppId = 0;

            if (g_pfnCoreInit)
                g_pfnCoreInit();

            // Get the actual values from core after init
            if (pfnGetAppId)
                g_ForcedAppId = pfnGetAppId();
            if (pfnGetOgAppId)
                g_OriginalAppId = pfnGetOgAppId();
        }

        #ifdef _DEBUG
        g_ForcedAppId = 480;
        g_OriginalAppId = 0;
        #endif

        UCOLOG("[UCOnline2] PID: %lu", GetCurrentProcessId());
        UCOLOG("[UCOnline2] Thread: %lu", GetCurrentThreadId());

        LoadGameOverlay();
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        if (g_pfnCoreShutdown)
            g_pfnCoreShutdown();

        if (g_CoreModule)
            FreeLibrary(g_CoreModule);
    }

    return TRUE;
}

// ============================================================
// CCallbackDispatcher
// ============================================================

static bool s_bDispatcherReady = false;

CCallbackDispatcher::CCallbackDispatcher()
{
    UCOColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "[UCOnline2] CCallbackDispatcher constructed\r\n");

    m_pfnBGetCallback = nullptr;
    m_pfnFreeLastCallback = nullptr;
    m_pfnGetAPICallResult = nullptr;
    m_CurrentUser = 0;
    m_ManualCbId = 0;
    m_ManualCbSize = 0;
    m_bProcessing = false;
    m_CallbackMap.clear();
    m_CallResultMap.clear();
    m_BufferMap.clear();
    s_bDispatcherReady = true;
}

CCallbackDispatcher::~CCallbackDispatcher()
{
    UCOColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "[UCOnline2] CCallbackDispatcher destroyed\r\n");

    s_bDispatcherReady = false;
    m_pfnBGetCallback = nullptr;
    m_pfnFreeLastCallback = nullptr;
    m_pfnGetAPICallResult = nullptr;
    m_CurrentUser = 0;
    m_ManualCbId = 0;
    m_ManualCbSize = 0;
    m_bProcessing = false;
    m_CallbackMap.clear();
    m_CallResultMap.clear();
    m_BufferMap.clear();
}

void CCallbackDispatcher::Shutdown()
{
    UCOColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "[UCOnline2] CCallbackDispatcher shutdown\r\n");

    s_bDispatcherReady = false;
    m_pfnBGetCallback = nullptr;
    m_pfnFreeLastCallback = nullptr;
    m_pfnGetAPICallResult = nullptr;
    m_CurrentUser = 0;
    m_ManualCbId = 0;
    m_ManualCbSize = 0;
    m_bProcessing = false;
    m_CallbackMap.clear();
    m_CallResultMap.clear();
    m_BufferMap.clear();
}

void CCallbackDispatcher::ExecuteCallResult(HSteamPipe hPipe, SteamAPICall_t hCall, CCallbackBase* pCb)
{
    UCOLOG("[UCOnline2] ExecuteCallResult -> call=%lld cb=%d size=%d\r\n", hCall, pCb->GetICallback(), pCb->GetCallbackSizeBytes());

    BYTE* pBuffer = new BYTE[pCb->GetCallbackSizeBytes()]();
    bool bFailed = false;

    bool bResult = m_pfnGetAPICallResult(hPipe, hCall, pBuffer, pCb->GetCallbackSizeBytes(), pCb->GetICallback(), &bFailed);

    if (bResult && !bFailed)
    {
        size_t countBefore = m_CallbackMap.size();

        pCb->Run(pBuffer, bFailed, hCall);

        if (countBefore != m_CallbackMap.size())
        {
            UCOColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "[UCOnline2] Callback map changed during CallResult execution\r\n");

            m_CallbackMap.erase(LobbyEnter_t::k_iCallback);
            pCb->Run(pBuffer);
        }

        m_BufferMap.insert(std::make_pair(hCall, pBuffer));
    }
    else
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] GetAPICallResult failed\r\n");
        delete[] pBuffer;
    }
}

void CCallbackDispatcher::DispatchFrame(HSteamPipe hPipe, bool bServer)
{
    if (!m_pfnBGetCallback || !m_pfnFreeLastCallback || !m_pfnGetAPICallResult)
        return;

    CallbackMsg_t msg = { 0 };
    if (!m_pfnBGetCallback(hPipe, &msg))
        return;

    UCOLOG("[UCOnline2] Callback received -> %d\r\n", msg.m_iCallback);
    m_CurrentUser = msg.m_hSteamUser;

    if (msg.m_iCallback == SteamAPICallCompleted_t::k_iCallback && msg.m_cubParam == sizeof(SteamAPICallCompleted_t))
    {
        SteamAPICallCompleted_t* pCompleted = (SteamAPICallCompleted_t*)msg.m_pubParam;

        if (!m_CallResultMap.empty())
        {
            for (auto it = m_CallResultMap.begin(); it != m_CallResultMap.end(); ++it)
            {
                if (pCompleted->m_hAsyncCall == it->first &&
                    pCompleted->m_iCallback == it->second->GetICallback() &&
                    pCompleted->m_cubParam == (uint32)it->second->GetCallbackSizeBytes())
                {
                    ExecuteCallResult(hPipe, it->first, it->second);
                }
            }
        }
    }
    else
    {
        if (!m_CallbackMap.empty())
        {
            for (auto it = m_CallbackMap.begin(); it != m_CallbackMap.end(); ++it)
            {
                CCallbackBase* pCb = it->second;

                if (it->first == msg.m_iCallback && (pCb->m_nCallbackFlags & pCb->k_ECallbackFlagsRegistered))
                {
                    if (msg.m_hSteamUser == g_ServerUser && (pCb->m_nCallbackFlags & pCb->k_ECallbackFlagsGameServer) && bServer)
                    {
                        UCOLOG("[UCOnline2] Server callback -> %d flags=%d\r\n", msg.m_iCallback, pCb->m_nCallbackFlags);
                        pCb->Run(msg.m_pubParam);
                        break;
                    }
                    else if (msg.m_hSteamUser == g_ClientUser && !(pCb->m_nCallbackFlags & pCb->k_ECallbackFlagsGameServer) && !bServer)
                    {
                        UCOLOG("[UCOnline2] Client callback -> %d flags=%d\r\n", msg.m_iCallback, pCb->m_nCallbackFlags);

                        bool bSkip = false;

                        if (msg.m_iCallback == HTML_NeedsPaint_t::k_iCallback && msg.m_cubParam == sizeof(HTML_NeedsPaint_t))
                        {
                            HTML_NeedsPaint_t* pPaint = (HTML_NeedsPaint_t*)msg.m_pubParam;
                            if (pPaint->unWide == 1 || pPaint->unTall == 1)
                                bSkip = true;
                        }

                        if (!bSkip)
                            pCb->Run(msg.m_pubParam);

                        break;
                    }
                }
            }
        }
    }

    UCOLOG("[UCOnline2] Freeing callback -> %d\r\n", msg.m_iCallback);
    SecureZeroMemory(msg.m_pubParam, msg.m_cubParam);
    m_pfnFreeLastCallback(hPipe);
}

// ... rest of callback dispatcher methods would be similar ...

void CCallbackDispatcher::DispatchFrameSafe(HSteamPipe hPipe, bool bServer)
{
    try
    {
        if (!m_pfnBGetCallback || !m_pfnFreeLastCallback || !m_pfnGetAPICallResult)
            return;

        CallbackMsg_t msg = { 0 };
        if (!m_pfnBGetCallback(hPipe, &msg))
            return;

        UCOLOG("[UCOnline2] Callback (safe) -> %d\r\n", msg.m_iCallback);
        m_CurrentUser = msg.m_hSteamUser;

        if (msg.m_iCallback == SteamAPICallCompleted_t::k_iCallback && msg.m_cubParam == sizeof(SteamAPICallCompleted_t))
        {
            SteamAPICallCompleted_t* pCompleted = (SteamAPICallCompleted_t*)msg.m_pubParam;

            if (!m_CallResultMap.empty())
            {
                for (auto it = m_CallResultMap.begin(); it != m_CallResultMap.end(); ++it)
                {
                    if (pCompleted->m_hAsyncCall == it->first &&
                        pCompleted->m_iCallback == it->second->GetICallback() &&
                        pCompleted->m_cubParam == (uint32)it->second->GetCallbackSizeBytes())
                    {
                        ExecuteCallResult(hPipe, it->first, it->second);
                    }
                }
            }
        }
        else
        {
            if (!m_CallbackMap.empty())
            {
                for (auto it = m_CallbackMap.begin(); it != m_CallbackMap.end(); ++it)
                {
                    CCallbackBase* pCb = it->second;

                    if (it->first == msg.m_iCallback && (pCb->m_nCallbackFlags & pCb->k_ECallbackFlagsRegistered))
                    {
                        if (msg.m_hSteamUser == g_ServerUser && (pCb->m_nCallbackFlags & pCb->k_ECallbackFlagsGameServer) && bServer)
                        {
                            UCOLOG("[UCOnline2] Server callback (safe) -> %d flags=%d\r\n", msg.m_iCallback, pCb->m_nCallbackFlags);
                            pCb->Run(msg.m_pubParam);
                            break;
                        }
                        else if (msg.m_hSteamUser == g_ClientUser && !(pCb->m_nCallbackFlags & pCb->k_ECallbackFlagsGameServer) && !bServer)
                        {
                            UCOLOG("[UCOnline2] Client callback (safe) -> %d flags=%d\r\n", msg.m_iCallback, pCb->m_nCallbackFlags);
                            pCb->Run(msg.m_pubParam);
                            break;
                        }
                    }
                }
            }
        }

        UCOLOG("[UCOnline2] Freeing callback (safe) -> %d\r\n", msg.m_iCallback);
        SecureZeroMemory(msg.m_pubParam, msg.m_cubParam);
        m_pfnFreeLastCallback(hPipe);
    }
    catch (...)
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] Exception in callback dispatch\r\n");
    }
}

void CCallbackDispatcher::Add(CCallbackBase* pCb, int iCallback)
{
    if (!pCb)
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] Add: null callback ptr\r\n");
        return;
    }

    if (pCb->GetCallbackSizeBytes() == 0)
        return;

    UCOLOG("[UCOnline2] Register callback -> %d size=%d flags=%d\r\n", iCallback, pCb->GetCallbackSizeBytes(), pCb->m_nCallbackFlags);

    pCb->m_nCallbackFlags |= pCb->k_ECallbackFlagsRegistered;
    pCb->m_iCallback = iCallback;
    m_CallbackMap.insert(std::make_pair(iCallback, pCb));
}

void CCallbackDispatcher::AddCallResult(CCallbackBase* pCb, SteamAPICall_t hCall)
{
    if (!pCb || hCall == k_uAPICallInvalid)
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] AddCallResult: invalid args\r\n");
        return;
    }

    if (pCb->GetICallback() == 0 || pCb->GetCallbackSizeBytes() == 0)
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] AddCallResult: zero callback\r\n");
        return;
    }

    UCOLOG("[UCOnline2] Register call result -> %lld cb=%d size=%d\r\n", hCall, pCb->GetICallback(), pCb->GetCallbackSizeBytes());

    pCb->m_nCallbackFlags |= pCb->k_ECallbackFlagsRegistered;

    auto existing = m_CallResultMap.find(hCall);
    if (existing == m_CallResultMap.end())
    {
        m_CallResultMap.insert(std::make_pair(hCall, pCb));
    }
    else
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] AddCallResult: already registered\r\n");
        existing->second = pCb;
    }
}

void CCallbackDispatcher::Remove(CCallbackBase* pCb)
{
    if (!pCb)
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] Remove: null callback ptr\r\n");
        return;
    }

    if (!(pCb->m_nCallbackFlags & pCb->k_ECallbackFlagsRegistered))
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] Remove: not registered\r\n");
        return;
    }

    if (m_CallbackMap.empty())
        return;

    UCOLOG("[UCOnline2] Unregister callback -> %d flags=%d\r\n", pCb->GetICallback(), pCb->m_nCallbackFlags);
    pCb->m_nCallbackFlags &= ~pCb->k_ECallbackFlagsRegistered;

    for (auto it = m_CallbackMap.begin(); it != m_CallbackMap.end(); ++it)
    {
        if (it->second == pCb)
        {
            m_CallbackMap.erase(it);
            break;
        }
    }
}

void CCallbackDispatcher::RemoveCallResult(CCallbackBase* pCb, SteamAPICall_t hCall)
{
    if (!pCb || hCall == k_uAPICallInvalid)
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] RemoveCallResult: invalid args\r\n");
        return;
    }

    if (!(pCb->m_nCallbackFlags & pCb->k_ECallbackFlagsRegistered))
    {
        UCOColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "[UCOnline2] RemoveCallResult: not registered\r\n");
        return;
    }

    UCOLOG("[UCOnline2] Unregister call result -> %lld cb=%d flags=%d\r\n", hCall, pCb->GetICallback(), pCb->m_nCallbackFlags);
    pCb->m_nCallbackFlags &= ~pCb->k_ECallbackFlagsRegistered;

    auto bufIt = m_BufferMap.find(hCall);
    if (bufIt != m_BufferMap.end())
    {
        UCOColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "[UCOnline2] Freeing call result buffer\r\n");
        delete[] bufIt->second;
        m_BufferMap.erase(bufIt);
    }
}

CCallbackDispatcher* GetDispatcher()
{
    static CCallbackDispatcher instance;
    return &instance;
}

// ============================================================
// SetAppIDEnv / WriteAppIDFile (called by SteamAPI functions)
// ============================================================

void SetAppIDEnv()
{
    char szApp[16] = { 0 };
    char szGame[32] = { 0 };
    char szOverlayGame[32] = { 0 };

    _snprintf_s(szApp, sizeof(szApp), _TRUNCATE, "%u", g_ForcedAppId);
    _snprintf_s(szGame, sizeof(szGame), _TRUNCATE, "%llu", CGameID(g_ForcedAppId).ToUint64());

    uint32_t overlayAppId = (g_OriginalAppId != 0) ? g_OriginalAppId : g_ForcedAppId;
    _snprintf_s(szOverlayGame, sizeof(szOverlayGame), _TRUNCATE, "%llu", CGameID(overlayAppId).ToUint64());

    SetEnvironmentVariableA("SteamAppId", szApp);
    SetEnvironmentVariableA("SteamGameId", szGame);
    SetEnvironmentVariableA("SteamOverlayGameId", szOverlayGame);
}

void WriteAppIDFile()
{
    char buf[16] = { 0 };
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%u\n", g_ForcedAppId);

    FILE* f = nullptr;
    if (fopen_s(&f, "steam_appid.txt", "wb") == 0 && f)
    {
        fwrite(buf, 1, strlen(buf), f);
        fclose(f);
    }

    char exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) != 0)
    {
        if (PathRemoveFileSpecA(exePath))
        {
            char fullPath[MAX_PATH] = { 0 };
            _snprintf_s(fullPath, sizeof(fullPath), _TRUNCATE, "%s\\steam_appid.txt", exePath);

            f = nullptr;
            if (fopen_s(&f, fullPath, "wb") == 0 && f)
            {
                fwrite(buf, 1, strlen(buf), f);
                fclose(f);
            }
        }
    }
}