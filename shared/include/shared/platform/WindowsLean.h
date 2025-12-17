/*******************************************************************************************
*   KnC Shared - Windows Platform Wrapper
*   Copyright (c) 2025 Kart N'Chibi Team
*   
*   Purpose: Centralized Windows.h inclusion with macro hygiene
*   Usage: Include this instead of <Windows.h> in .cpp files (NEVER in public headers)
*******************************************************************************************/

#pragma once

#ifdef _WIN32

// ============================================================================
// Step 1: Define hygiene macros BEFORE including Windows.h
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN  // Exclude rarely-used Windows APIs
#endif

#ifndef NOMINMAX
    #define NOMINMAX  // Prevent min/max macros
#endif

#ifndef VC_EXTRALEAN
    #define VC_EXTRALEAN  // Further reduce Windows.h bloat
#endif

#ifndef NOCOMM
    #define NOCOMM  // No serial communication APIs
#endif

#ifndef NOGDI
    #define NOGDI  // No GDI (we use OpenGL)
#endif

#ifndef NOUSER
    #define NOUSER  // No user APIs (we use GLFW)
#endif

// ============================================================================
// Step 2: Save existing macros (if any) - We'll restore them after cleanup
// ============================================================================

#pragma push_macro("time")
#pragma push_macro("min")
#pragma push_macro("max")

// Kill them BEFORE Windows.h inclusion so they don't pollute
#ifdef time
    #undef time
#endif
#ifdef min
    #undef min
#endif
#ifdef max
    #undef max
#endif

// ============================================================================
// Step 3: Include Windows.h (now it can't define time/min/max macros)
// ============================================================================

#include <Windows.h>

// ============================================================================
// Step 4: Kill ANY remaining toxic macros that leaked from Windows.h
// ============================================================================

#ifdef min
    #undef min
#endif

#ifdef max
    #undef max
#endif

#ifdef near
    #undef near
#endif

#ifdef far
    #undef far
#endif

#ifdef ERROR
    #undef ERROR
#endif

#ifdef DELETE
    #undef DELETE
#endif

#ifdef IGNORE
    #undef IGNORE
#endif

#ifdef CreateWindow
    #undef CreateWindow
#endif

#ifdef CreateDirectory
    #undef CreateDirectory
#endif

#ifdef DeleteFile
    #undef DeleteFile
#endif

#ifdef MoveFile
    #undef MoveFile
#endif

#ifdef CopyFile
    #undef CopyFile
#endif

#ifdef GetCurrentDirectory
    #undef GetCurrentDirectory
#endif

// THE BIG ONE: time() macro that breaks <ctime>
#ifdef time
    #undef time
#endif

// ============================================================================
// Step 5: Restore pushed macros (for legacy code compatibility)
// ============================================================================
// NOTE: We restore the macros so legacy code that expects them still works,
// but they're now "clean" and won't pollute STL headers like <ctime>

#pragma pop_macro("max")
#pragma pop_macro("min")
#pragma pop_macro("time")

#endif // _WIN32

