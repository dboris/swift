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
// W4.4: objc_catlist -- objc4 category_t records (@objc extension members
// on imported / other-module classes), invisible to ObjC sends until
// registered.  On ELF every category's class word is load-bound, so the
// list goes to libobjc2 as-is.
DECLARE_EMPTY_METADATA_SECTION(objc_catlist, "awR")
DECLARE_BOUNDS(objc_catlist)
// The class STUBS -- and this is the list an APP's OWN classes are in.
// GenMeta.cpp routes a class whose ivar layout depends on a superclass in
// another binary (i.e. every `class Foo: UIView`) to addObjCClassStub() and
// emits NO objc_classlist entry for it, so the classlist arm above sees only
// the classes nobody writes.  Until 2026-09-12 such a class was reachable ONLY
// through the Swift runtime's demangling getClass hook, under its MANGLED
// name, so objc_getClass() on the name a compiled nib carries returned nil and
// every storyboard/xib naming a Swift class decoded that object as nil.
DECLARE_EMPTY_METADATA_SECTION(objc_stublist, "awR")
DECLARE_BOUNDS(objc_stublist)

void objc_load_swift_image_np(const char **selrefs_begin,
                              const char **selrefs_end,
                              void **classlist_begin,
                              void **classlist_end) __attribute__((__weak__));
void objc_load_swift_image_categories_np(void **catlist_begin,
                                         void **catlist_end)
    __attribute__((__weak__));
void objc_load_swift_image_stubs_np(void **stublist_begin,
                                    void **stublist_end)
    __attribute__((__weak__));
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
  // W4.4: register this image's Swift-emitted categories AFTER its classes.
  // Weak: an older libobjc2 lacks the entry point and the categories stay
  // dormant (the gate's category leg catches the staleness loudly).
  if (&objc_load_swift_image_categories_np != nullptr) {
    objc_load_swift_image_categories_np(
        reinterpret_cast<void **>(const_cast<char *>(&__start_objc_catlist)),
        reinterpret_cast<void **>(const_cast<char *>(&__stop_objc_catlist)));
  }
  // ⛔⛔ THE STUB-CLASS REGISTRATION IS **NOT CALLED HERE**, AND THAT IS MEASURED,
  // NOT CAUTION.  The sections above are still bracketed so the list is available
  // the moment there is a safe place to consume it -- there is not one in this
  // constructor.
  //
  // Registering a stub means CALLING its initializer, which instantiates the
  // Swift metadata and therefore WALKS THE SUPERCLASS CHAIN.  From an image
  // constructor a superclass that is an imported ObjC class in a STATICALLY
  // LINKED archive (libuikit-linux.a) has not registered yet -- its gnustep
  // registration runs from its own TU constructors, whose order relative to
  // swiftrt.o's is not ours to choose.  Measured 2026-09-12 with a control pair
  // on one binary (examples/swiftpm-swiftuitest over the Linux SDK):
  //
  //    call DISABLED : 288 PASS, 3 FAIL  (exactly the documented render XFAILs)
  //    call ENABLED  :   0 PASS, Signal 6 at startup --
  //        "failed to demangle superclass of UIHostingController from mangled
  //         name 'So16UIViewControllerC': unknown error"
  //
  // ⚠️ ELF CONSTRUCTOR PRIORITY CANNOT FIX IT: prioritised constructors
  // (101..65535) run BEFORE all unprioritised ones, so there is no later slot to
  // move into.  The COFF arm escapes only because PE gives it the `.CRT$XCT`
  // pass, which sorts after the `.CRT$XCLz` class registrations.
  //
  // THE FIX IS A REDESIGN, and it is better on BOTH arms: have libobjc2 RECORD
  // the range here and realize it on the first class-lookup MISS
  // (`_objc_lookup_class`), when every image is initialised.  A missed name
  // lookup is exactly the event the nib path cares about, and it drops the COFF
  // arm's dependency on XCT too.
  // docs/handoffs/2026-09-12-embed-segue-tier.md §5f.
  (void)&objc_load_swift_image_stubs_np;
#endif
}
SWIFT_ALLOWED_RUNTIME_GLOBAL_CTOR_END
