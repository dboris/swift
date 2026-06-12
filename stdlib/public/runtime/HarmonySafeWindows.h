//===--- HarmonySafeWindows.h - BOOL-safe Windows SDK pre-parse -*- C -*-===//
//
// HARMONY (W3; WinCatalyst lesson #133): under the gnustep-3.0 ABI, ObjC
// BOOL is Apple-compatible signed char and OWNS the BOOL name, which
// hard-conflicts with minwindef.h's `typedef int BOOL` in any TU that sees
// both.  The fix is the chokepoint pattern (libobjc2 safewindows.h /
// WinCatalyst Foundation.h): pre-parse the <Windows.h> umbrella with BOOL
// renamed to the SDK's real 4-byte typedef `_WINBOOL`, making every later
// raw #include an include-guard no-op.  The interop flag list force-
// includes this header (/clang:-include) into every runtime C/C++/ObjC++
// TU on Windows -- runtime code that genuinely wants the Win32 type spells
// `_WINBOOL`, exactly as WinCatalyst's CG/CT sources do.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_HARMONY_SAFE_WINDOWS_H
#define SWIFT_HARMONY_SAFE_WINDOWS_H

#if defined(_WIN32)
#pragma push_macro("BOOL")
#undef BOOL
#define BOOL _WINBOOL
#include <Windows.h>
// SDK headers the runtime pulls OUTSIDE the <Windows.h> umbrella get the
// same treatment (the COMIncludes.h residual from lesson #133's record).
// The set is the grep over stdlib/public + include/swift/{Runtime,
// Threading} for raw `#include <um-header>` sites: shellapi
// (CommandLine.cpp), DbgHelp/psapi (SymbolInfo/ImageInspection),
// ShlWapi/Bcrypt (stubs), realtimeapiset (Threading).
#include <shellapi.h>
#include <DbgHelp.h>
#include <psapi.h>
#include <ShlWapi.h>
#include <Bcrypt.h>
#include <realtimeapiset.h>
#pragma pop_macro("BOOL")

// ShlWapi pulls the COM base headers, which leave `interface` defined as a
// macro for `struct` -- fatal to every later ObjC `@interface` (the disease
// WinCatalyst's COMIncludes.h/_End.h sandwich treats).  The runtime never
// declares COM interfaces, so simply drop it.
#undef interface
#endif

#endif // SWIFT_HARMONY_SAFE_WINDOWS_H
