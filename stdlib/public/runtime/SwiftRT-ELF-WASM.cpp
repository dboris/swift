//===--- SwiftRT-ELF-WASM.cpp ---------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "ImageInspectionCommon.h"
#include "swift/shims/MetadataSections.h"
#include "swift/Runtime/Backtrace.h"
#include "swift/Runtime/Config.h"

#include <cstddef>
#include <new>

#if defined(__ELF__)
extern "C" const char __ehdr_start[] __attribute__((__weak__));
#endif

#if SWIFT_ENABLE_BACKTRACING
// Drag in a symbol from the backtracer, to force the static linker to include
// the code.
static const void *__backtraceRef __attribute__((used, retain))
  = (const void *)swift::runtime::backtrace::_swift_backtrace_isThunkFunction;
#endif

// Create empty sections to ensure that the start/stop symbols are synthesized
// by the linker.  Otherwise, we may end up with undefined symbol references as
// the linker table section was never constructed.
#if defined(__ELF__)
# define DECLARE_EMPTY_METADATA_SECTION(name, attrs) __asm__("\t.section " #name ",\"" attrs "\"\n");
#elif defined(__wasm__)
# define DECLARE_EMPTY_METADATA_SECTION(name, attrs) __asm__("\t.section " #name ",\"R\",@\n");
#endif

#define BOUNDS_VISIBILITY __attribute__((__visibility__("hidden"), \
                                         __aligned__(1)))

#define DECLARE_BOUNDS(name)                            \
  BOUNDS_VISIBILITY extern const char __start_##name;   \
  BOUNDS_VISIBILITY extern const char __stop_##name;

#define DECLARE_SWIFT_SECTION(name)             \
  DECLARE_EMPTY_METADATA_SECTION(name, "aR")    \
  DECLARE_BOUNDS(name)

// These may or may not be present, depending on compiler switches; it's
// worth calling them out as a result.
#define DECLARE_SWIFT_REFLECTION_SECTION(name)  \
  DECLARE_SWIFT_SECTION(name)

extern "C" {
DECLARE_SWIFT_SECTION(swift5_protocols)
DECLARE_SWIFT_SECTION(swift5_protocol_conformances)
DECLARE_SWIFT_SECTION(swift5_type_metadata)

DECLARE_SWIFT_REFLECTION_SECTION(swift5_fieldmd)
DECLARE_SWIFT_REFLECTION_SECTION(swift5_builtin)
DECLARE_SWIFT_REFLECTION_SECTION(swift5_assocty)
DECLARE_SWIFT_REFLECTION_SECTION(swift5_capture)
DECLARE_SWIFT_REFLECTION_SECTION(swift5_reflstr)
DECLARE_SWIFT_REFLECTION_SECTION(swift5_typeref)
DECLARE_SWIFT_REFLECTION_SECTION(swift5_mpenum)

DECLARE_SWIFT_SECTION(swift5_replace)
DECLARE_SWIFT_SECTION(swift5_replac2)
DECLARE_SWIFT_SECTION(swift5_accessible_functions)
DECLARE_SWIFT_SECTION(swift5_runtime_attributes)

DECLARE_SWIFT_SECTION(swift5_tests)
}

#if SWIFT_OBJC_INTEROP && defined(__ELF__) && !defined(__APPLE__)
// HARMONY: under ObjC interop over libobjc2, swiftc also emits objc4-shaped
// ObjC metadata sections that the GNUstep runtime's own loader never scans:
// objc_selrefs (selector references to unique in place) and objc_classlist
// (classes to register via objc_readClassPair).  Register them from this
// image constructor through libobjc2's loader entry point -- declared
// weakly so images linked without libobjc stay loadable (their sections
// are empty anyway).  Section attributes match swiftc's emission exactly
// (selrefs: write+alloc; classlist: write+alloc+retain) so the empty
// declarations merge with the real sections.
extern "C" {
DECLARE_EMPTY_METADATA_SECTION(objc_selrefs, "aw")
DECLARE_BOUNDS(objc_selrefs)
DECLARE_EMPTY_METADATA_SECTION(objc_classlist, "awR")
DECLARE_BOUNDS(objc_classlist)

void objc_load_swift_image_np(const char **selrefs_begin,
                              const char **selrefs_end,
                              void **classlist_begin,
                              void **classlist_end) __attribute__((__weak__));
}
#endif

#undef DECLARE_SWIFT_SECTION

namespace {
static swift::MetadataSections sections{};
}

SWIFT_ALLOWED_RUNTIME_GLOBAL_CTOR_BEGIN
__attribute__((__constructor__))
static void swift_image_constructor() {
#define SWIFT_SECTION_RANGE(name)                                              \
  { reinterpret_cast<uintptr_t>(&__start_##name),                              \
    static_cast<uintptr_t>(&__stop_##name - &__start_##name) }

    const void *baseAddress = nullptr;
#if defined(__ELF__)
  if (&__ehdr_start != nullptr) {
    baseAddress = __ehdr_start;
  }
#elif defined(__wasm__)
  // NOTE: Multi images in a single process is not yet stabilized in WebAssembly
  // toolchain outside of Emscripten.
#endif

  ::new (&sections) swift::MetadataSections {
      swift::CurrentSectionMetadataVersion,
      baseAddress,

      nullptr,
      nullptr,

      SWIFT_SECTION_RANGE(swift5_protocols),
      SWIFT_SECTION_RANGE(swift5_protocol_conformances),
      SWIFT_SECTION_RANGE(swift5_type_metadata),

      SWIFT_SECTION_RANGE(swift5_typeref),
      SWIFT_SECTION_RANGE(swift5_reflstr),
      SWIFT_SECTION_RANGE(swift5_fieldmd),
      SWIFT_SECTION_RANGE(swift5_assocty),
      SWIFT_SECTION_RANGE(swift5_replace),
      SWIFT_SECTION_RANGE(swift5_replac2),
      SWIFT_SECTION_RANGE(swift5_builtin),
      SWIFT_SECTION_RANGE(swift5_capture),
      SWIFT_SECTION_RANGE(swift5_mpenum),
      SWIFT_SECTION_RANGE(swift5_accessible_functions),
      SWIFT_SECTION_RANGE(swift5_runtime_attributes),
      SWIFT_SECTION_RANGE(swift5_tests),
  };

#undef SWIFT_SECTION_RANGE

  swift_addNewDSOImage(&sections);

#if SWIFT_OBJC_INTEROP && defined(__ELF__) && !defined(__APPLE__)
  // HARMONY: hand this image's objc4-shaped ObjC sections to libobjc2
  // (idempotent there, so a legacy hand-linked shim in the same image is
  // harmless during migration).
  if (&objc_load_swift_image_np != nullptr) {
    objc_load_swift_image_np(
        reinterpret_cast<const char **>(
            const_cast<char *>(&__start_objc_selrefs)),
        reinterpret_cast<const char **>(
            const_cast<char *>(&__stop_objc_selrefs)),
        reinterpret_cast<void **>(const_cast<char *>(&__start_objc_classlist)),
        reinterpret_cast<void **>(const_cast<char *>(&__stop_objc_classlist)));
  }
#endif
}
SWIFT_ALLOWED_RUNTIME_GLOBAL_CTOR_END
