//===--- HarmonyPE.h - PE flavors of POSIX runtime idioms -------*- C -*-===//
//
// HARMONY (W3): dlsym(RTLD_DEFAULT, ...) means "search every loaded
// image"; the PE flavor walks the loaded modules with GetProcAddress
// (the looked-up symbols -- overlay conformance descriptors, wincat CF
// entry points -- are DLL exports on this stack).  Shared by
// ErrorObject.mm and FoundationHelpers.mm.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_RUNTIME_HARMONY_PE_H
#define SWIFT_RUNTIME_HARMONY_PE_H

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <psapi.h>

static inline void *swiftHarmonyDlsymDefault(const char *name) {
  HMODULE modules[1024];
  DWORD bytes = 0;
  if (!K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules),
                             &bytes))
    return nullptr;
  DWORD count = bytes / sizeof(HMODULE);
  if (count > 1024)
    count = 1024;
  for (DWORD i = 0; i < count; ++i)
    if (void *p = (void *)GetProcAddress(modules[i], name))
      return p;
  return nullptr;
}
#define SWIFT_HARMONY_DLSYM_DEFAULT(name) swiftHarmonyDlsymDefault(name)

#else

#define SWIFT_HARMONY_DLSYM_DEFAULT(name) dlsym(RTLD_DEFAULT, name)

#endif // defined(_WIN32)

#endif // SWIFT_RUNTIME_HARMONY_PE_H
