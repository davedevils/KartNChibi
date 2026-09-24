// intercepts calls between KnC exe and EngineDLL dll build with cl LD

#include <windows.h>
#include <stdio.h>
#include <time.h>

static HMODULE g_hOrigDLL = nullptr;
static FILE* g_logFile = nullptr;

typedef int (__stdcall* NK_instantiateClassFn)(const char*, void**, int);
typedef int (__stdcall* NK_instantiateClassByHashedUUIDFn)(const void*, const void*, void**, int);

static NK_instantiateClassFn g_origInstantiateClass = nullptr;
static NK_instantiateClassByHashedUUIDFn g_origInstantiateByHash = nullptr;

void Log(const char* fmt, ...) {
    if (!g_logFile) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(g_logFile, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(g_logFile, fmt, args);
    va_end(args);
    
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
}

void LogHex(const char* name, const void* data, int size) {
    if (!g_logFile || !data) return;
    
    fprintf(g_logFile, "  %s: ", name);
    const unsigned char* bytes = (const unsigned char*)data;
    for (int i = 0; i < size; i++) {
        fprintf(g_logFile, "%02X ", bytes[i]);
    }
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
}

void LogVTable(const char* name, void* obj, int count) {
    if (!g_logFile || !obj) return;
    
    void** vt = *(void***)obj;
    fprintf(g_logFile, "  %s VTable @ %p:\n", name, vt);
    for (int i = 0; i < count; i++) {
        fprintf(g_logFile, "    [%2d] %p\n", i, vt[i]);
    }
    fflush(g_logFile);
}

void HookStreamVtable(void* streamObj);

extern "C" __declspec(dllexport) int __stdcall NK_instantiateClass(
    const char* className, void** ppObject, int flags)
{
    Log("NK_instantiateClass(\"%s\", %p, %d)", className ? className : "NULL", ppObject, flags);
    
    int result = g_origInstantiateClass(className, ppObject, flags);
    
    Log("  -> result=%d (0x%08X), *ppObject=%p", result, result, ppObject ? *ppObject : nullptr);
    
    if (result >= 0 && ppObject && *ppObject) {
        LogVTable("Object", *ppObject, 20);
    }
    
    return result;
}

// matches hash1 to detect a NiStream instantiation
static const unsigned char STREAM_HASH1[] = {
    0x4E, 0xB6, 0x55, 0x13, 0x59, 0xF7, 0x0F, 0x4A,
    0xA3, 0x79, 0x52, 0x43, 0xC8, 0xA1, 0x29, 0x23,
    0xDB, 0x60, 0x98, 0xB0
};

extern "C" __declspec(dllexport) int __stdcall NK_instantiateClassByHashedUUID(
    const void* hash1, const void* hash2, void** ppObject, int flags)
{
    Log("NK_instantiateClassByHashedUUID(%p, %p, %p, %d)", hash1, hash2, ppObject, flags);
    LogHex("hash1", hash1, 20);
    LogHex("hash2", hash2, 20);
    
    int result = g_origInstantiateByHash(hash1, hash2, ppObject, flags);
    
    Log("  -> result=%d (0x%08X), *ppObject=%p", result, result, ppObject ? *ppObject : nullptr);
    
    if (result >= 0 && ppObject && *ppObject) {
        LogVTable("Object", *ppObject, 30);
        
        fprintf(g_logFile, "  Object memory dump:\n");
        DWORD* mem = (DWORD*)*ppObject;
        for (int i = 0; i < 64; i += 4) {
            fprintf(g_logFile, "    +%03d: %08X %08X %08X %08X\n", 
                i*4, mem[i], mem[i+1], mem[i+2], mem[i+3]);
        }
        fflush(g_logFile);
        
        // checks if this is a NiStream and skips hooking it since that breaks rendering
        if (memcmp(hash1, STREAM_HASH1, 20) == 0) {
            Log("*** DETECTED NiStream CREATION ***");
        }
    }
    
    return result;
}

// index and original function pointer for one named vtable hook
struct VTableHook {
    void* originalFunc;
    int index;
    const char* name;
};

#define MAX_TRACKED_OBJECTS 100
static void* g_trackedObjects[MAX_TRACKED_OBJECTS];
static const char* g_trackedNames[MAX_TRACKED_OBJECTS];
static int g_numTracked = 0;

void TrackObject(void* obj, const char* name) {
    if (g_numTracked < MAX_TRACKED_OBJECTS) {
        g_trackedObjects[g_numTracked] = obj;
        g_trackedNames[g_numTracked] = name;
        g_numTracked++;
        Log("Tracking object %p as '%s'", obj, name);
    }
}

static void* g_streamObj = nullptr;
static void** g_streamVtable = nullptr;
static void* g_origStreamLoad = nullptr;

// hook for Stream Load vtable slot 1 args are this searchPath filename by observation
typedef bool (__stdcall* StreamLoadFn)(void* thisPtr, const char* searchPath, const char* filename);
bool __stdcall HookedStreamLoad(void* thisPtr, const char* searchPath, const char* filename) {
    Log(">>> NiStream::Load(searchPath=\"%s\", filename=\"%s\")", 
        searchPath ? searchPath : "NULL", filename ? filename : "NULL");
    
    bool result = ((StreamLoadFn)g_origStreamLoad)(thisPtr, searchPath, filename);
    
    Log(">>> NiStream::Load result: %s", result ? "SUCCESS" : "FAILED");
    
    if (result) {
        // dumps the full stream memory after load 200 dwords which is 800 bytes
        DWORD* mem = (DWORD*)thisPtr;
        Log(">>> Stream memory after load (extended dump):");
        for (int i = 0; i < 200; i += 4) {
            fprintf(g_logFile, "    +%03d: %08X %08X %08X %08X\n", 
                i*4, mem[i], mem[i+1], mem[i+2], mem[i+3]);
        }
        
        // looks for an object count of 13 since INTRO nif has 13 objects
        Log(">>> Looking for object count (13)...");
        for (int i = 0; i < 200; i++) {
            if (mem[i] == 13) {
                Log(">>> Found 13 at offset %d (0x%X)", i*4, i*4);
                // checks if the previous dword is a pointer to the object array
                if (i > 0) {
                    DWORD arrPtr = mem[i-1];
                    if (arrPtr > 0x02000000 && arrPtr < 0x70000000) {
                        Log(">>>   Potential array at offset %d: %08X", (i-1)*4, arrPtr);
                        __try {
                            DWORD* arr = (DWORD*)arrPtr;
                            Log(">>>   Array[0]=%08X Array[1]=%08X", arr[0], arr[1]);
                            DWORD firstObj = arr[0];
                            if (firstObj > 0x02000000 && firstObj < 0x70000000) {
                                DWORD vt = *(DWORD*)firstObj;
                                Log(">>>   First object vtable: %08X", vt);
                                if (vt == 0x101DAB70 || vt == 0x101DB6A8) {
                                    Log(">>> FOUND NIF OBJECTS! Array at offset %d", (i-1)*4);
                                }
                            }
                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                            Log(">>>   (exception reading array)");
                        }
                    }
                }
            }
        }
        
        // looks for vtable 0x101DAB70 or 0x101DB6A8 directly in the stream
        Log(">>> Looking for NiNode/NiTriStrips vtables...");
        for (int i = 0; i < 200; i++) {
            if (mem[i] == 0x101DAB70 || mem[i] == 0x101DB6A8) {
                Log(">>> Found vtable %08X at offset %d (inline object?)", mem[i], i*4);
            }
        }
        
        // checks these offsets for object arrays
        Log(">>> Checking specific offsets for objects...");
        int checkOffsets[] = {13, 14, 15, 16, 17}; // offsets 52 56 60 64 68
        for (int idx = 0; idx < 5; idx++) {
            int off = checkOffsets[idx];
            DWORD ptr = mem[off];
            if (ptr > 0x02000000 && ptr < 0x70000000) {
                Log(">>> Offset %d: ptr=%08X", off*4, ptr);
                __try {
                    DWORD* target = (DWORD*)ptr;
                    DWORD vt = target[0];
                    Log(">>>   -> first DWORD=%08X", vt);
                    
                    if (vt == 0x101DAB70 || vt == 0x101DB6A8) {
                        Log(">>>   *** FOUND NIF OBJECT at offset %d! vtable=%08X ***", off*4, vt);
                    }
                    
                    // checks if it is an array of pointers to objects
                    if (vt > 0x02000000 && vt < 0x70000000) {
                        DWORD firstObjVt = *(DWORD*)vt;
                        if (firstObjVt == 0x101DAB70 || firstObjVt == 0x101DB6A8) {
                            Log(">>>   *** FOUND OBJECT ARRAY at offset %d! first object vtable=%08X ***", off*4, firstObjVt);
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    Log(">>>   (exception reading ptr)");
                }
            }
        }
        
        fflush(g_logFile);
    }
    
    return result;
}

void HookStreamVtable(void* streamObj) {
    g_streamObj = streamObj;
    g_streamVtable = *(void***)streamObj;
    
    Log("Hooking Stream vtable @ %p", g_streamVtable);
    
    // saves the original then hooks the Load method at vtable slot 1
    g_origStreamLoad = g_streamVtable[1];
    
    DWORD oldProtect;
    VirtualProtect(&g_streamVtable[1], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
    g_streamVtable[1] = (void*)HookedStreamLoad;
    VirtualProtect(&g_streamVtable[1], sizeof(void*), oldProtect, &oldProtect);
    
    Log("Hooked Stream::Load @ %p -> %p", g_origStreamLoad, HookedStreamLoad);
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hInstance);
            
            g_logFile = fopen("EngineDLL_proxy.log", "w");
            if (g_logFile) {
                Log("=== EngineDLL Proxy Started ===");
                Log("Process: KnC.exe");
            }
            
            g_hOrigDLL = LoadLibraryA("EngineDLL_orig.dll");
            if (!g_hOrigDLL) {
                Log("ERROR: Failed to load EngineDLL_orig.dll (error %d)", GetLastError());
                return FALSE;
            }
            Log("Loaded EngineDLL_orig.dll @ %p", g_hOrigDLL);
            
            g_origInstantiateClass = (NK_instantiateClassFn)
                GetProcAddress(g_hOrigDLL, "NK_instantiateClass");
            g_origInstantiateByHash = (NK_instantiateClassByHashedUUIDFn)
                GetProcAddress(g_hOrigDLL, "NK_instantiateClassByHashedUUID");
            
            Log("NK_instantiateClass @ %p", g_origInstantiateClass);
            Log("NK_instantiateClassByHashedUUID @ %p", g_origInstantiateByHash);
            
            break;
        }
        
        case DLL_PROCESS_DETACH: {
            if (g_logFile) {
                Log("=== EngineDLL Proxy Shutting Down ===");
                Log("Total tracked objects: %d", g_numTracked);
                fclose(g_logFile);
                g_logFile = nullptr;
            }
            
            if (g_hOrigDLL) {
                FreeLibrary(g_hOrigDLL);
                g_hOrigDLL = nullptr;
            }
            break;
        }
    }
    
    return TRUE;
}


