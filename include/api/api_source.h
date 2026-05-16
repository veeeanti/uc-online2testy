/**
 *  Source Engine Platform Interface Stubs
 *  =====================================
 *  Half-Life and Source engine games require filesystem, sys, and tier0
 *  interfaces that are NOT part of Steam API but are loaded via steamclient.dll.
 *
 *  These stubs prevent "Missing shutdown function" errors.
 *
 *  ~veeλnti<3 2026
 */

#pragma once

#include <Windows.h>
#include <string>
#include <vector>

// Declare UCOLOG before we use it
void UCOLOG(const char* fmt, ...);

namespace SourceEngine {

// Forward declarations for shutdown registration
typedef void (*ShutdownFunc_t)();

struct ShutdownEntry {
    const char* name;
    ShutdownFunc_t func;
};

// Filesystem interface stub
// Half-Life expects this for save games, configs, and content mounting
class CFilesystemStub
{
private:
    bool m_bInitialized;
    std::string m_BaseDir;
    
public:
    CFilesystemStub() : m_bInitialized(false) {}
    
    // Core lifecycle - REQUIRED
    virtual bool Init(const char* basedir, void* filesystemFactory) {
        m_BaseDir = basedir ? basedir : "";
        m_bInitialized = true;
        UCOLOG("[UCOnline2-Source] Filesystem Init: %s", m_BaseDir.c_str());
        return true;
    }
    
    virtual void Shutdown() {
        UCOLOG("[UCOnline2-Source] Filesystem Shutdown");
        m_bInitialized = false;
    }
    
    // File operations - commonly called
    virtual bool FileExists(const char* path, const char* pathID = nullptr) {
        if (!path || !m_bInitialized) return false;
        
        char fullPath[MAX_PATH];
        if (m_BaseDir.empty()) {
            strcpy_s(fullPath, MAX_PATH, path);
        } else {
            _snprintf_s(fullPath, MAX_PATH, _TRUNCATE, "%s\\%s", m_BaseDir.c_str(), path);
        }
        
        DWORD attrs = GetFileAttributesA(fullPath);
        return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
    }
    
    virtual void* Open(const char* fileName, const char* options, const char* pathID = nullptr) {
        if (!fileName) return nullptr;
        
        char fullPath[MAX_PATH];
        if (m_BaseDir.empty()) {
            strcpy_s(fullPath, MAX_PATH, fileName);
        } else {
            _snprintf_s(fullPath, MAX_PATH, _TRUNCATE, "%s\\%s", m_BaseDir.c_str(), fileName);
        }
        
        FILE* f = nullptr;
        if (fopen_s(&f, fullPath, options) != 0) {
            return nullptr;
        }
        return (void*)f;
    }
    
    virtual int Read(void* pOutput, int size, void* handle) {
        if (!pOutput || !handle) return 0;
        return fread(pOutput, 1, size, (FILE*)handle);
    }
    
    virtual int Write(const void* pInput, int size, void* handle) {
        if (!pInput || !handle) return 0;
        return fwrite(pInput, 1, size, (FILE*)handle);
    }
    
    virtual void Close(void* handle) {
        if (handle) fclose((FILE*)handle);
    }
    
    virtual bool IsInitialized() const { return m_bInitialized; }
    virtual const char* GetBaseDir() const { return m_BaseDir.c_str(); }
};

// Sys interface stub
// Provides command line and system-level operations
class CSysStub
{
private:
    std::vector<std::string> m_Argv;
    
public:
    CSysStub() {}
    
    virtual bool InitArgv(const char* cmdLine) {
        m_Argv.clear();
        std::string line = cmdLine ? cmdLine : "";
        
        size_t start = 0;
        while (start < line.length()) {
            while (start < line.length() && isspace((unsigned char)line[start])) start++;
            if (start >= line.length()) break;
            size_t end = start;
            while (end < line.length() && !isspace((unsigned char)line[end])) end++;
            m_Argv.push_back(line.substr(start, end - start));
            start = end;
        }
        UCOLOG("[UCOnline2-Source] Sys InitArgv: %d args", (int)m_Argv.size());
        return true;
    }
    
    virtual void ShutdownArgv() {
        UCOLOG("[UCOnline2-Source] Sys ShutdownArgv");
        m_Argv.clear();
    }
    
    virtual int GetArgc() const { return (int)m_Argv.size(); }
    virtual const char* GetArgv(int index) const {
        if (index < 0 || index >= (int)m_Argv.size()) return nullptr;
        return m_Argv[index].c_str();
    }
};

// Tier0 memory interface stub
class CTier0Stub
{
public:
    CTier0Stub() {}
    
    // Memory allocation stubs - map to standard heap
    virtual void* MemAlloc(size_t size) { return malloc(size); }
    virtual void* MemAlloc(size_t size, const char* pFileName, int nLine) { return malloc(size); }
    virtual void MemFree(void* pMem) { free(pMem); }
};

} // namespace SourceEngine

// Global instance definitions - these must be declared before the inline functions
namespace SourceEngine {
    inline CFilesystemStub& GetFilesystemStub() { static CFilesystemStub stub; return stub; }
    inline CSysStub& GetSysStub() { static CSysStub stub; return stub; }
    inline CTier0Stub& GetTier0Stub() { static CTier0Stub stub; return stub; }
    inline std::vector<ShutdownEntry>& GetShutdownHandlers() { static std::vector<ShutdownEntry> handlers; return handlers; }
}

// C linkage exports for SteamInternal_CreateInterface
extern "C" {

S_API void* S_CALLTYPE GetSourceFilesystemStub() {
    return &SourceEngine::GetFilesystemStub();
}

S_API void* S_CALLTYPE GetSourceSysStub() {
    return &SourceEngine::GetSysStub();
}

S_API void* S_CALLTYPE GetSourceTier0Stub() {
    return &SourceEngine::GetTier0Stub();
}

}