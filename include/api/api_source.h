/**
 *  Source Engine Platform Interface Stubs
 *  =====================================
 *  GoldSrc/Half-Life filesystem and sys interfaces based on the GoldSrc SDK.
 *  These are NOT Steam API interfaces but are loaded via steamclient.dll.
 *
 *  Interface versions:
 *  - VFileSystem009 (GoldSrc default)
 *  - VSys001 (Sys interface)
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

// File handle types from FileSystem.h
typedef void* FileHandle_t;
typedef int FileFindHandle_t;

// FileSystemSeek_t from FileSystem.h
enum FileSystemSeek_t {
    FILESYSTEM_SEEK_HEAD = 0,
    FILESYSTEM_SEEK_CURRENT,
    FILESYSTEM_SEEK_TAIL
};

// IBaseInterface from interface.h
class IBaseInterface {
public:
    virtual ~IBaseInterface() {}
};

// Filesystem interface stub - matches IFileSystem from FileSystem.h
// GoldSrc uses VFileSystem009
class CFilesystemStub : public IBaseInterface
{
private:
    bool m_bInitialized;
    std::string m_BaseDir;
    
public:
    CFilesystemStub() : m_bInitialized(false) {}
    
    // Core lifecycle - Mount/Unmount from IFileSystem
    virtual void Mount(void) { 
        m_bInitialized = true; 
        UCOLOG("[UCOnline2-Source] Filesystem Mount");
    }
    virtual void Unmount(void) { 
        UCOLOG("[UCOnline2-Source] Filesystem Unmount");
        m_bInitialized = false; 
    }

    virtual void RemoveAllSearchPaths(void) {}
    virtual void AddSearchPath(const char* pPath, const char* pathID) {}
    virtual bool RemoveSearchPath(const char* pPath) { return false; }
    virtual void RemoveFile(const char* pRelativePath, const char* pathID) {}
    virtual void CreateDirHierarchy(const char* path, const char* pathID) {}

    // File I/O and info
    virtual bool FileExists(const char* pFileName) { 
        if (!pFileName || !m_bInitialized) return false;
        char fullPath[MAX_PATH];
        if (m_BaseDir.empty()) {
            strcpy_s(fullPath, MAX_PATH, pFileName);
        } else {
            _snprintf_s(fullPath, MAX_PATH, _TRUNCATE, "%s\\%s", m_BaseDir.c_str(), pFileName);
        }
        DWORD attrs = GetFileAttributesA(fullPath);
        return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
    }
    virtual bool IsDirectory(const char* pFileName) { return false; }

    // opens a file
    virtual FileHandle_t Open(const char* pFileName, const char* pOptions, const char* pathID = 0L) {
        if (!pFileName) return (FileHandle_t)0;
        char fullPath[MAX_PATH];
        if (m_BaseDir.empty()) {
            strcpy_s(fullPath, MAX_PATH, pFileName);
        } else {
            _snprintf_s(fullPath, MAX_PATH, _TRUNCATE, "%s\\%s", m_BaseDir.c_str(), pFileName);
        }
        FILE* f = nullptr;
        if (fopen_s(&f, fullPath, pOptions) != 0) return (FileHandle_t)0;
        return (FileHandle_t)f;
    }
    
    virtual void Close(FileHandle_t file) { 
        if (file) fclose((FILE*)file);
    }
    
    virtual void Seek(FileHandle_t file, int pos, FileSystemSeek_t seekType) { 
        if (file) fseek((FILE*)file, pos, (seekType == FILESYSTEM_SEEK_HEAD) ? SEEK_SET : 
            (seekType == FILESYSTEM_SEEK_CURRENT) ? SEEK_CUR : SEEK_END);
    }
    virtual unsigned int Tell(FileHandle_t file) { 
        return file ? ftell((FILE*)file) : 0;
    }
    virtual unsigned int Size(FileHandle_t file) { 
        if (!file) return 0;
        long pos = ftell((FILE*)file);
        fseek((FILE*)file, 0, SEEK_END);
        long size = ftell((FILE*)file);
        fseek((FILE*)file, pos, SEEK_SET);
        return (unsigned int)size;
    }
    virtual unsigned int Size(const char* pFileName) { return 0; }
    
    virtual long GetFileTime(const char* pFileName) { return 0; }
    virtual void FileTimeToString(char* pStrip, int maxCharsIncludingTerminator, long fileTime) {}
    virtual bool IsOk(FileHandle_t file) { return file != (FileHandle_t)0; }
    virtual void Flush(FileHandle_t file) { if (file) fflush((FILE*)file); }
    virtual bool EndOfFile(FileHandle_t file) { return file ? feof((FILE*)file) : true; }
    
    virtual int Read(void* pOutput, int size, FileHandle_t file) { 
        return file ? fread(pOutput, 1, size, (FILE*)file) : 0;
    }
    virtual int Write(void const* pInput, int size, FileHandle_t file) { 
        return file ? fwrite(pInput, 1, size, (FILE*)file) : 0;
    }
    virtual char* ReadLine(char* pOutput, int maxChars, FileHandle_t file) { 
        return file ? fgets(pOutput, maxChars, (FILE*)file) : nullptr;
    }
    virtual int FPrintf(FileHandle_t file, char* pFormat, ...) { return 0; }

    // FindFirst/FindNext - stubs
    virtual const char* FindFirst(const char* pWildCard, FileFindHandle_t* pHandle, const char* pathID = 0L) { return nullptr; }
    virtual const char* FindNext(FileFindHandle_t handle) { return nullptr; }
    virtual bool FindIsDirectory(FileFindHandle_t handle) { return false; }
    virtual void FindClose(FileFindHandle_t handle) {}
    
    // Stub implementations for remaining methods
    virtual void GetLocalCopy(const char* pFileName) {}
    virtual const char* GetLocalPath(const char* pFileName, char* pLocalPath, int localPathBufferSize) { return nullptr; }
    virtual char* ParseFile(char* pFileBytes, char* pToken, bool* pWasQuoted) { return nullptr; }
    virtual bool FullPathToRelativePath(const char* pFullpath, char* pRelative) { return false; }
    virtual bool GetCurrentDirectory(char* pDirectory, int maxlen) { return false; }
    virtual void PrintOpenedFiles(void) {}
    virtual void SetWarningFunc(void (*pfnWarning)(const char* fmt, ...)) {}
    virtual void SetWarningLevel(int level) {}
    virtual void LogLevelLoadStarted(const char* name) {}
    virtual void LogLevelLoadFinished(const char* name) {}
    virtual int HintResourceNeed(const char* hintlist, int forgetEverything) { return 0; }
    virtual int PauseResourcePreloading(void) { return 0; }
    virtual int ResumeResourcePreloading(void) { return 0; }
    virtual int SetVBuf(FileHandle_t stream, char* buffer, int mode, long size) { return 0; }
    virtual void GetInterfaceVersion(char* p, int maxlen) {}
    virtual bool IsFileImmediatelyAvailable(const char* pFileName) { return false; }
};

// Sys interface stub - command line handling
class CSysStub : public IBaseInterface
{
private:
    std::vector<std::string> m_Argv;
    
public:
    CSysStub() {}
    
    // Command line init
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
class CTier0Stub : public IBaseInterface
{
public:
    CTier0Stub() {}
    
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
}

// C linkage exports for CreateInterface
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