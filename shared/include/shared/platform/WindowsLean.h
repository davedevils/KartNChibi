// centralized Windows h inclusion with macro hygiene include this instead of Windows h in cpp files never in public headers

#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#ifndef VC_EXTRALEAN
    #define VC_EXTRALEAN
#endif

#ifndef NOCOMM
    #define NOCOMM
#endif

#ifndef NOGDI
    #define NOGDI  // no GDI since OpenGL is used
#endif

#ifndef NOUSER
    #define NOUSER  // no user APIs since GLFW is used
#endif

#pragma push_macro("time")
#pragma push_macro("min")
#pragma push_macro("max")

// kills them before Windows h inclusion so they do not pollute
#ifdef time
    #undef time
#endif
#ifdef min
    #undef min
#endif
#ifdef max
    #undef max
#endif

#include <Windows.h>

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

// the big one the time macro that breaks ctime
#ifdef time
    #undef time
#endif

// restores the pushed macros so legacy code that expects them still works cleanly

#pragma pop_macro("max")
#pragma pop_macro("min")
#pragma pop_macro("time")

#endif // ends the WIN32 guard

