//===--- SwiftRT-COFF.cpp -------------------------------------------------===//
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

#include <cstdint>
#include <new>

#if SWIFT_OBJC_INTEROP
// HARMONY (W3): GetModuleHandleW/GetProcAddress for the objc loader probe
// below.  Include-guard no-op when the HarmonySafeWindows chokepoint
// already pre-parsed the SDK.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

extern "C" const char __ImageBase[];

#define PASTE_EXPANDED(a,b) a##b
#define PASTE(a,b) PASTE_EXPANDED(a,b)

#define STRING_EXPANDED(string) #string
#define STRING(string) STRING_EXPANDED(string)

#define C_LABEL(name) PASTE(__USER_LABEL_PREFIX__,name)

#define PRAGMA(pragma) _Pragma(#pragma)

#define DECLARE_SWIFT_SECTION(name)                                            \
  PRAGMA(section("." #name "$A", long, read))                                  \
  __declspec(allocate("." #name "$A"))                                         \
  __declspec(align(1))                                                         \
  static uintptr_t __start_##name = 0;                                         \
                                                                               \
  PRAGMA(section("." #name "$C", long, read))                                  \
  __declspec(allocate("." #name "$C"))                                         \
  __declspec(align(1))                                                         \
  static uintptr_t __stop_##name = 0;

extern "C" {
DECLARE_SWIFT_SECTION(sw5prt)
DECLARE_SWIFT_SECTION(sw5prtc)
DECLARE_SWIFT_SECTION(sw5tymd)

DECLARE_SWIFT_SECTION(sw5tyrf)
DECLARE_SWIFT_SECTION(sw5rfst)
DECLARE_SWIFT_SECTION(sw5flmd)
DECLARE_SWIFT_SECTION(sw5asty)
DECLARE_SWIFT_SECTION(sw5repl)
DECLARE_SWIFT_SECTION(sw5reps)
DECLARE_SWIFT_SECTION(sw5bltn)
DECLARE_SWIFT_SECTION(sw5cptr)
DECLARE_SWIFT_SECTION(sw5mpen)
DECLARE_SWIFT_SECTION(sw5acfn)
DECLARE_SWIFT_SECTION(sw5ratt)
DECLARE_SWIFT_SECTION(sw5test)
}

#if SWIFT_OBJC_INTEROP
// HARMONY (W3): the COFF arm of the slice-10 design -- under ObjC interop
// over libobjc2, swiftc emits objc4-shaped grouped sections
// (.objc_selrefs$B, .objc_classlist$B) the GNUstep loader never scans.
// Bracket them with $A/$C bookends (the same technique as the swift5
// sections above) and hand the bounds to libobjc2's
// objc_load_swift_image_np from the image constructor.  The entry point
// is resolved DYNAMICALLY from the loaded objc module: swiftrt.obj links
// into every Swift image and must not impose objc.lib on non-ObjC ones
// (their sections are empty anyway), and PE has no weak-undefined
// imports.  Registration is NAME-ONLY on the libobjc2 side; dtables stay
// with the lazy realizer (the slice-10 order lesson holds on PE too:
// .CRT$XCIS ctors run per-DLL at load, before the exe's).
#define DECLARE_OBJC_SECTION(name)                                            \
  PRAGMA(section("." #name "$A", long, read, write))                          \
  __declspec(allocate("." #name "$A"))                                        \
  __declspec(align(1))                                                        \
  static uintptr_t __start_##name = 0;                                        \
                                                                               \
  PRAGMA(section("." #name "$C", long, read, write))                          \
  __declspec(allocate("." #name "$C"))                                        \
  __declspec(align(1))                                                        \
  static uintptr_t __stop_##name = 0;

extern "C" {
DECLARE_OBJC_SECTION(objc_selrefs)
DECLARE_OBJC_SECTION(objc_classlist)
DECLARE_OBJC_SECTION(objc_classrefs)
DECLARE_OBJC_SECTION(objc_superrefs)
// W4.4: objc4 category_t records (@objc extension members on imported /
// other-module classes), invisible to ObjC sends until registered.  Each
// record's class word is an EAGER class-object pointer -- the same
// per-image anchor problem as classrefs; the constructor canonicalizes
// it before handing the list to libobjc2.
DECLARE_OBJC_SECTION(objc_catlist)
// HARMONY (W3B): the SELF-DESCRIBING class anchors IRGen defines for
// clang-imported external ObjC classes (GenDecl.cpp
// getAddrOfHarmonyPEObjCClassAnchor): each is a one-word constant whose
// content points at the class's runtime NAME.  The constructor below
// resolves any fixup-walk word that points into these ranges through
// objc_lookUpClass(name) -- the GENERAL arm of the anchor design; the
// fixed 8-root table below covers the Swift-DEFINED runtime roots that
// IRGen cannot anchor (no clang node).
DECLARE_OBJC_SECTION(hmny_canchor)
DECLARE_OBJC_SECTION(hmny_manchor)
}
#undef DECLARE_OBJC_SECTION

// swiftc-emitted class metadata initializes each cache word with the
// ADDRESS of _objc_empty_cache -- a load-time data relocation PE cannot
// express against another DLL's export (the W2 constant-anchor constraint,
// same shape).  Every Swift image therefore carries a module-local link
// anchor under that name; objc_load_swift_image_np stamps the CANONICAL
// objc.dll address into the registered classes' cache words before
// anything can message them, so the anchor's contents are never read.
extern "C" __declspec(align(64)) char
    _swift_harmony_objc_empty_cache_anchor[4096] = {0};
#pragma comment( \
    linker, \
    "/alternatename:_objc_empty_cache=_swift_harmony_objc_empty_cache_anchor")

// The same constraint one stratum up: swiftc-emitted METACLASS metadata
// (the CMm objects of SwiftObject-rooted classes) eagerly references the
// root metaclasses by symbol -- OBJC_METACLASS_$__TtCs12_SwiftObject lives
// in swiftCore.dll, OBJC_METACLASS_$_NSObject in the ObjC Foundation DLL.
// (Only the metaclass chain is raw objc4; class-object superrefs are lazy
// Swift metadata.)  Anchor both per image; the constructor below rewrites
// metaclass isa/superclass words that point at an anchor to the CANONICAL
// metaclass, found through the runtime by name -- DLL dependency order
// guarantees the defining image registered first.
#define HARMONY_METACLASS_ANCHOR(sym, cls)                                     \
  extern "C" __declspec(align(64)) char                                        \
      _swift_harmony_mc_anchor_##cls[64] = {0};                                \
  __pragma(comment(linker, "/alternatename:OBJC_METACLASS_$_" sym              \
                           "=_swift_harmony_mc_anchor_" #cls))                 \
  static_assert(true, "swallow the call-site semicolon")

HARMONY_METACLASS_ANCHOR("_TtCs12_SwiftObject", SwiftObject);
HARMONY_METACLASS_ANCHOR("NSObject", NSObject);
HARMONY_METACLASS_ANCHOR("__SwiftNativeNSArrayBase", NSArrayBase);
HARMONY_METACLASS_ANCHOR("__SwiftNativeNSMutableArrayBase", NSMutableArrayBase);
HARMONY_METACLASS_ANCHOR("__SwiftNativeNSDictionaryBase", NSDictionaryBase);
HARMONY_METACLASS_ANCHOR("__SwiftNativeNSSetBase", NSSetBase);
HARMONY_METACLASS_ANCHOR("__SwiftNativeNSStringBase", NSStringBase);
HARMONY_METACLASS_ANCHOR("__SwiftNativeNSEnumeratorBase", NSEnumeratorBase);
#undef HARMONY_METACLASS_ANCHOR

// And the CLASS objects, for EAGER references: .objc_classrefs /
// .objc_superrefs entries (objc_msgSend receivers, super sends) are
// load-time pointers to class objects that may live in another DLL --
// the wall-12 family ("gate 3's eager NSObject classref").  Same anchor
// scheme; the constructor rewrites anchor-pointing REF SLOTS to the
// canonical class object by name.  (Stub-class superclass references
// need none of this: resilient Swift metadata resolves them by mangled
// name at instantiation.)
#define HARMONY_CLASS_ANCHOR(sym, cls)                                         \
  extern "C" __declspec(align(64)) char                                        \
      _swift_harmony_c_anchor_##cls[64] = {0};                                 \
  __pragma(comment(linker, "/alternatename:OBJC_CLASS_$_" sym                  \
                           "=_swift_harmony_c_anchor_" #cls))                  \
  static_assert(true, "swallow the call-site semicolon")

HARMONY_CLASS_ANCHOR("_TtCs12_SwiftObject", SwiftObject);
HARMONY_CLASS_ANCHOR("NSObject", NSObject);
HARMONY_CLASS_ANCHOR("__SwiftNativeNSArrayBase", NSArrayBase);
HARMONY_CLASS_ANCHOR("__SwiftNativeNSMutableArrayBase", NSMutableArrayBase);
HARMONY_CLASS_ANCHOR("__SwiftNativeNSDictionaryBase", NSDictionaryBase);
HARMONY_CLASS_ANCHOR("__SwiftNativeNSSetBase", NSSetBase);
HARMONY_CLASS_ANCHOR("__SwiftNativeNSStringBase", NSStringBase);
HARMONY_CLASS_ANCHOR("__SwiftNativeNSEnumeratorBase", NSEnumeratorBase);
#undef HARMONY_CLASS_ANCHOR

namespace {
struct HarmonyMetaclassAnchor {
  const char *className; // the runtime registration name
  void *anchor;          // this image's link anchor for its metaclass
};
} // namespace
#endif

namespace {
static swift::MetadataSections sections{};
}

static void swift_image_constructor() {
#define SWIFT_SECTION_RANGE(name)                                              \
  { reinterpret_cast<uintptr_t>(&__start_##name) + sizeof(__start_##name),     \
    reinterpret_cast<uintptr_t>(&__stop_##name) - reinterpret_cast<uintptr_t>(&__start_##name) - sizeof(__start_##name) }

  ::new (&sections) swift::MetadataSections {
      swift::CurrentSectionMetadataVersion,
      { __ImageBase },

      nullptr,
      nullptr,

      SWIFT_SECTION_RANGE(sw5prt),
      SWIFT_SECTION_RANGE(sw5prtc),
      SWIFT_SECTION_RANGE(sw5tymd),

      SWIFT_SECTION_RANGE(sw5tyrf),
      SWIFT_SECTION_RANGE(sw5rfst),
      SWIFT_SECTION_RANGE(sw5flmd),
      SWIFT_SECTION_RANGE(sw5asty),
      SWIFT_SECTION_RANGE(sw5repl),
      SWIFT_SECTION_RANGE(sw5reps),
      SWIFT_SECTION_RANGE(sw5bltn),
      SWIFT_SECTION_RANGE(sw5cptr),
      SWIFT_SECTION_RANGE(sw5mpen),
      SWIFT_SECTION_RANGE(sw5acfn),
      SWIFT_SECTION_RANGE(sw5ratt),
      SWIFT_SECTION_RANGE(sw5test),
  };

#undef SWIFT_SECTION_RANGE

  swift_addNewDSOImage(&sections);

#if SWIFT_OBJC_INTEROP
  // HARMONY (W3): register this image's objc4 sections with libobjc2 (see
  // the section declarations above).  +1 skips the $A bookend's own slot,
  // exactly like SWIFT_SECTION_RANGE does for the swift5 sections.
  using objc_load_swift_image_fn = void (*)(const char **, const char **,
                                            void **, void **);
  using objc_load_swift_categories_fn = void (*)(void **, void **);
  using objc_get_class_fn = void *(*)(const char *);
  if (HMODULE objcModule = GetModuleHandleW(L"objc")) {
    void **classlistBegin =
        reinterpret_cast<void **>(&__start_objc_classlist + 1);
    void **classlistEnd = reinterpret_cast<void **>(&__stop_objc_classlist);
    void **catlistBegin = reinterpret_cast<void **>(&__start_objc_catlist + 1);
    void **catlistEnd = reinterpret_cast<void **>(&__stop_objc_catlist);

    // Metaclass-chain fixup BEFORE registration: rewrite isa/superclass
    // words of this image's metaclasses that point at the local link
    // anchors onto the canonical root metaclasses, found through the
    // runtime by name (class object word 0).  Within one image the
    // gnustep registration ctor (.CRT$XCL) has already run when this
    // (.CRT$XCIS) executes, and dependency DLLs initialized fully first
    // -- so every defining class is findable by the time its anchors
    // need resolving.
    if (auto getClass = reinterpret_cast<objc_get_class_fn>(
            reinterpret_cast<void *>(
                GetProcAddress(objcModule, "objc_lookUpClass")))) {
      const HarmonyMetaclassAnchor anchors[] = {
          {"_TtCs12_SwiftObject", _swift_harmony_mc_anchor_SwiftObject},
          {"NSObject", _swift_harmony_mc_anchor_NSObject},
          {"__SwiftNativeNSArrayBase", _swift_harmony_mc_anchor_NSArrayBase},
          {"__SwiftNativeNSMutableArrayBase",
           _swift_harmony_mc_anchor_NSMutableArrayBase},
          {"__SwiftNativeNSDictionaryBase",
           _swift_harmony_mc_anchor_NSDictionaryBase},
          {"__SwiftNativeNSSetBase", _swift_harmony_mc_anchor_NSSetBase},
          {"__SwiftNativeNSStringBase", _swift_harmony_mc_anchor_NSStringBase},
          {"__SwiftNativeNSEnumeratorBase",
           _swift_harmony_mc_anchor_NSEnumeratorBase},
      };
      const HarmonyMetaclassAnchor classAnchors[] = {
          {"_TtCs12_SwiftObject", _swift_harmony_c_anchor_SwiftObject},
          {"NSObject", _swift_harmony_c_anchor_NSObject},
          {"__SwiftNativeNSArrayBase", _swift_harmony_c_anchor_NSArrayBase},
          {"__SwiftNativeNSMutableArrayBase",
           _swift_harmony_c_anchor_NSMutableArrayBase},
          {"__SwiftNativeNSDictionaryBase",
           _swift_harmony_c_anchor_NSDictionaryBase},
          {"__SwiftNativeNSSetBase", _swift_harmony_c_anchor_NSSetBase},
          {"__SwiftNativeNSStringBase", _swift_harmony_c_anchor_NSStringBase},
          {"__SwiftNativeNSEnumeratorBase",
           _swift_harmony_c_anchor_NSEnumeratorBase},
      };
      const int anchorCount = sizeof(anchors) / sizeof(anchors[0]);
      void *canonicalClass[anchorCount] = {};
      void *canonical[anchorCount] = {};
      for (int i = 0; i < anchorCount; ++i)
        if (void *cls = getClass(anchors[i].className)) {
          canonicalClass[i] = cls;
          canonical[i] = *reinterpret_cast<void **>(cls); // word 0: isa
        }
      // HARMONY (W3B): the GENERAL anchors -- IRGen-defined per-image
      // symbols for clang-imported classes, self-describing (the anchor
      // word points at the runtime name).  A hit in .hmny_canchor
      // resolves to the class object; a hit in .hmny_manchor to its
      // metaclass (class word 0).  Returns null for non-anchor pointers
      // and for classes not yet registered (the latter would be a
      // dependency-order bug -- the slot then keeps the anchor address
      // and faults loudly at first dispatch rather than silently).
      const char *canchorBegin =
          reinterpret_cast<const char *>(&__start_hmny_canchor + 1);
      const char *canchorEnd =
          reinterpret_cast<const char *>(&__stop_hmny_canchor);
      const char *manchorBegin =
          reinterpret_cast<const char *>(&__start_hmny_manchor + 1);
      const char *manchorEnd =
          reinterpret_cast<const char *>(&__stop_hmny_manchor);
      auto resolveNamedAnchor = [&](void *p) -> void * {
        const char *cp = reinterpret_cast<const char *>(p);
        if (cp >= canchorBegin && cp < canchorEnd)
          return getClass(*reinterpret_cast<const char *const *>(cp));
        if (cp >= manchorBegin && cp < manchorEnd)
          if (void *cls =
                  getClass(*reinterpret_cast<const char *const *>(cp)))
            return *reinterpret_cast<void **>(cls); // word 0: metaclass
        return nullptr;
      };
      for (void **c = classlistBegin; c < classlistEnd; ++c) {
        if (!*c)
          continue;
        void **cls = reinterpret_cast<void **>(*c);
        // The class's own superclass word may be an eager class-object
        // reference (clang-emitted classes; Swift stubs resolve by name).
        for (int i = 0; i < anchorCount; ++i)
          if (cls[1] == classAnchors[i].anchor && canonicalClass[i])
            cls[1] = canonicalClass[i];
        if (void *r = resolveNamedAnchor(cls[1]))
          cls[1] = r;
        void **meta = reinterpret_cast<void **>(cls[0]); // class isa
        if (!meta)
          continue;
        for (int w = 0; w < 2; ++w) { // metaclass isa + superclass
          for (int i = 0; i < anchorCount; ++i)
            if (meta[w] == anchors[i].anchor && canonical[i])
              meta[w] = canonical[i];
          if (void *r = resolveNamedAnchor(meta[w]))
            meta[w] = r;
        }
      }
      // Eager reference slots: classrefs hold class objects, superrefs
      // (super-send targets) hold class objects too on this stack.
      struct RefRange {
        void **begin, **end;
      } refRanges[] = {
          {reinterpret_cast<void **>(&__start_objc_classrefs + 1),
           reinterpret_cast<void **>(&__stop_objc_classrefs)},
          {reinterpret_cast<void **>(&__start_objc_superrefs + 1),
           reinterpret_cast<void **>(&__stop_objc_superrefs)},
      };
      for (const auto &range : refRanges)
        for (void **slot = range.begin; slot < range.end; ++slot) {
          for (int i = 0; i < anchorCount; ++i) {
            if (*slot == classAnchors[i].anchor && canonicalClass[i])
              *slot = canonicalClass[i];
            else if (*slot == anchors[i].anchor && canonical[i])
              *slot = canonical[i];
          }
          if (void *r = resolveNamedAnchor(*slot))
            *slot = r;
        }
      // W4.4: category records -- canonicalize each category_t's class
      // word (word 1: an eager class-object pointer, exactly the
      // classrefs problem) before the registration call below hands the
      // list to libobjc2; libobjc2 reads the class's NAME through it.
      for (void **entry = catlistBegin; entry < catlistEnd; ++entry) {
        if (!*entry)
          continue;
        void **cat = reinterpret_cast<void **>(*entry);
        for (int i = 0; i < anchorCount; ++i)
          if (cat[1] == classAnchors[i].anchor && canonicalClass[i])
            cat[1] = canonicalClass[i];
        if (void *r = resolveNamedAnchor(cat[1]))
          cat[1] = r;
      }
    }

    if (auto loadImage = reinterpret_cast<objc_load_swift_image_fn>(
            reinterpret_cast<void *>(
                GetProcAddress(objcModule, "objc_load_swift_image_np")))) {
      loadImage(reinterpret_cast<const char **>(&__start_objc_selrefs + 1),
                reinterpret_cast<const char **>(&__stop_objc_selrefs),
                classlistBegin, classlistEnd);
    }
    // W4.4: register this image's Swift-emitted categories AFTER its
    // classes.  A separate probed entry point (not new parameters on the
    // one above): PE has no symbol versioning, and an absent probe on an
    // older objc.dll leaves the categories dormant -- the gate's category
    // leg catches the staleness loudly -- instead of silently mis-passing
    // arguments.
    if (auto loadCategories = reinterpret_cast<objc_load_swift_categories_fn>(
            reinterpret_cast<void *>(GetProcAddress(
                objcModule, "objc_load_swift_image_categories_np")))) {
      loadCategories(catlistBegin, catlistEnd);
    }
  }
#endif
}

#pragma section(".CRT$XCIS", long, read)

__declspec(allocate(".CRT$XCIS"))
extern "C" void (*pSwiftImageConstructor)(void) = &swift_image_constructor;
#pragma comment(linker, "/include:" STRING(C_LABEL(pSwiftImageConstructor)))

#if SWIFT_OBJC_INTEROP
// HARMONY (static-class interop, spike-23 STAGE 3): a SECOND, LATER pass that resolves the
// eager .objc_classrefs / .objc_superrefs anchor slots for STATICALLY-LINKED
// imported ObjC classes -- the case swift_image_constructor (.CRT$XCIS) cannot
// handle.  Why a second ctor: clang emits each gnustep ObjC class's
// registration ctor into .CRT$XCLz (CGObjCGNU.cpp), and Windows _initterm runs
// .CRT$XC* in SUFFIX-SORTED order.  "XCIS" < "XCLz" (byte 3: 'I' < 'L'), so
// swift_image_constructor runs BEFORE this image's own static classes register;
// objc_lookUpClass() there returns null and the classref slot keeps its
// .hmny_canchor anchor, so the first objc_msgSend faults.  (DLL classes escape:
// their DLL's .CRT$XCLz runs entirely before the exe's constructors, so they
// are already resolved at XCIS.)  This pass sits in .CRT$XCT, which sorts AFTER
// .CRT$XCLz (registration done) and BEFORE user constructors .CRT$XCU (which may
// message these classes).  It is ADDITIVE: swift_image_constructor is unchanged,
// so DLL-class and Swift-native paths (the green W3.5 gate) are byte-for-byte
// preserved.  Slots already resolved at XCIS no longer point into an anchor
// range, so re-running here is a no-op for them; only still-anchored slots
// (static imported classes, now registered) get rewritten.  Only the GENERAL
// named anchors are walked -- the fixed swift-core roots are DLL/swiftCore
// classes already resolved at XCIS.
static void swift_resolve_static_objc_classrefs() {
  HMODULE objcModule = GetModuleHandleW(L"objc");
  if (!objcModule)
    return;
  using objc_get_class_fn = void *(*)(const char *);
  auto getClass = reinterpret_cast<objc_get_class_fn>(reinterpret_cast<void *>(
      GetProcAddress(objcModule, "objc_lookUpClass")));
  if (!getClass)
    return;

  const char *canchorBegin =
      reinterpret_cast<const char *>(&__start_hmny_canchor + 1);
  const char *canchorEnd = reinterpret_cast<const char *>(&__stop_hmny_canchor);
  const char *manchorBegin =
      reinterpret_cast<const char *>(&__start_hmny_manchor + 1);
  const char *manchorEnd = reinterpret_cast<const char *>(&__stop_hmny_manchor);
  auto resolveNamedAnchor = [&](void *p) -> void * {
    const char *cp = reinterpret_cast<const char *>(p);
    if (cp >= canchorBegin && cp < canchorEnd)
      return getClass(*reinterpret_cast<const char *const *>(cp));
    if (cp >= manchorBegin && cp < manchorEnd)
      if (void *cls = getClass(*reinterpret_cast<const char *const *>(cp)))
        return *reinterpret_cast<void **>(cls); // word 0: metaclass
    return nullptr;
  };

  struct RefRange {
    void **begin, **end;
  } refRanges[] = {
      {reinterpret_cast<void **>(&__start_objc_classrefs + 1),
       reinterpret_cast<void **>(&__stop_objc_classrefs)},
      {reinterpret_cast<void **>(&__start_objc_superrefs + 1),
       reinterpret_cast<void **>(&__stop_objc_superrefs)},
  };
  for (const auto &range : refRanges)
    for (void **slot = range.begin; slot < range.end; ++slot)
      if (*slot)
        if (void *r = resolveNamedAnchor(*slot))
          *slot = r;
}

#pragma section(".CRT$XCT", long, read)

__declspec(allocate(".CRT$XCT"))
extern "C" void (*pSwiftStaticClassrefConstructor)(void) =
    &swift_resolve_static_objc_classrefs;
#pragma comment( \
    linker, "/include:" STRING(C_LABEL(pSwiftStaticClassrefConstructor)))
#endif

