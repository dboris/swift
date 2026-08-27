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

// The two fixed anchor tables, at file scope so the three consumers -- the
// .CRT$XCIS image constructor, the .CRT$XCT catlist pass, and the on-demand
// resolver below -- read ONE list.  (They were three local copies; a name
// added to one and not the others is a silent hole.)
const HarmonyMetaclassAnchor kHarmonyMetaclassAnchors[] = {
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
const HarmonyMetaclassAnchor kHarmonyClassAnchors[] = {
    {"_TtCs12_SwiftObject", _swift_harmony_c_anchor_SwiftObject},
    {"NSObject", _swift_harmony_c_anchor_NSObject},
    {"__SwiftNativeNSArrayBase", _swift_harmony_c_anchor_NSArrayBase},
    {"__SwiftNativeNSMutableArrayBase",
     _swift_harmony_c_anchor_NSMutableArrayBase},
    {"__SwiftNativeNSDictionaryBase",
     _swift_harmony_c_anchor_NSDictionaryBase},
    {"__SwiftNativeNSSetBase", _swift_harmony_c_anchor_NSSetBase},
    {"__SwiftNativeNSStringBase", _swift_harmony_c_anchor_NSStringBase},
    {"__SwiftNativeNSEnumeratorBase", _swift_harmony_c_anchor_NSEnumeratorBase},
};
const int kHarmonyAnchorCount =
    sizeof(kHarmonyMetaclassAnchors) / sizeof(kHarmonyMetaclassAnchors[0]);
static_assert(sizeof(kHarmonyClassAnchors) ==
                  sizeof(kHarmonyMetaclassAnchors),
              "the class and metaclass anchor tables must stay parallel");
} // namespace

// HARMONY (defect B2): resolve an anchor-valued METACLASS superclass word to
// the CLASS object it stands for, ON DEMAND rather than from the image
// constructor.
//
// Why this exists.  A Swift root class carrying
// @_swift_native_objc_runtime_base(<base>) gets its ObjC base only through the
// metaclass chain on PE: IRGen picks ClassMetadataStrategy::Singleton for every
// COFF class (LazyInitializeClassMetadata = isOSBinFormatCOFF), and under that
// strategy the CLASS metadata's Superclass word is emitted NULL for the runtime
// to fill in -- while the statically-emitted metaclass still carries
// OBJC_METACLASS_$_<base>, i.e. this image's link anchor.  The Swift runtime's
// class-metadata initialization (Metadata.cpp's _swift_initClassMetadataImpl)
// therefore has no Swift superclass name to demangle and no class-side pointer
// to canonicalize, and used to root the class at SwiftObject -- which is why a
// bridged Swift string was not an NSString on Windows and was on ELF (there the
// Fixed/Update strategy emits the base statically and the dynamic linker binds
// it).  Feeding the metaclass word through here restores the ELF behaviour.
//
// ON DEMAND, not constructor-time: swift_image_constructor runs at .CRT$XCIS,
// which sorts BEFORE this image's own gnustep class registrations (.CRT$XCLz),
// so objc_lookUpClass cannot yet see swiftCore's own __SwiftNativeNS*Base
// classes.  By the time any Swift metadata is initialized they are registered.
//
// Returns null for anything that is not one of this image's anchors -- callers
// keep their existing behaviour then.
extern "C" void *_swift_harmony_classForMetaclassAnchor(void *p) {
  if (!p)
    return nullptr;
  HMODULE objcModule = GetModuleHandleW(L"objc");
  if (!objcModule)
    return nullptr;
  using objc_get_class_fn = void *(*)(const char *);
  auto getClass = reinterpret_cast<objc_get_class_fn>(reinterpret_cast<void *>(
      GetProcAddress(objcModule, "objc_lookUpClass")));
  if (!getClass)
    return nullptr;

  // The fixed anchors: the Swift-DEFINED runtime roots, which IRGen cannot
  // give a self-describing anchor (no clang node to hang one on).
  for (int i = 0; i < kHarmonyAnchorCount; ++i)
    if (p == kHarmonyMetaclassAnchors[i].anchor)
      return getClass(kHarmonyMetaclassAnchors[i].className);

  // The GENERAL, self-describing anchors (clang-imported classes): the anchor
  // word points at the class's runtime name.
  const char *cp = reinterpret_cast<const char *>(p);
  const char *manchorBegin =
      reinterpret_cast<const char *>(&__start_hmny_manchor + 1);
  const char *manchorEnd = reinterpret_cast<const char *>(&__stop_hmny_manchor);
  if (cp >= manchorBegin && cp < manchorEnd)
    return getClass(*reinterpret_cast<const char *const *>(cp));

  return nullptr;
}
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
  using objc_get_class_fn = void *(*)(const char *);
  if (HMODULE objcModule = GetModuleHandleW(L"objc")) {
    void **classlistBegin =
        reinterpret_cast<void **>(&__start_objc_classlist + 1);
    void **classlistEnd = reinterpret_cast<void **>(&__stop_objc_classlist);

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
      const HarmonyMetaclassAnchor *anchors = kHarmonyMetaclassAnchors;
      const HarmonyMetaclassAnchor *classAnchors = kHarmonyClassAnchors;
      const int anchorCount = kHarmonyAnchorCount;
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
    }

    if (auto loadImage = reinterpret_cast<objc_load_swift_image_fn>(
            reinterpret_cast<void *>(
                GetProcAddress(objcModule, "objc_load_swift_image_np")))) {
      loadImage(reinterpret_cast<const char **>(&__start_objc_selrefs + 1),
                reinterpret_cast<const char **>(&__stop_objc_selrefs),
                classlistBegin, classlistEnd);
    }
    // W4.4 -> fluentui slice-7: this image's Swift-emitted categories
    // (.objc_catlist) are canonicalized + registered in the .CRT$XCT pass
    // below (swift_resolve_static_objc_classrefs), NOT here.  A category
    // targeting a STATICALLY-linked imported ObjC class (e.g. uikit.lib's
    // UIView in an exe) cannot resolve at XCIS: the class's gnustep
    // registration ctor (.CRT$XCLz) has not run yet, objc_lookUpClass
    // returns null, the record's class word keeps its .hmny_canchor
    // anchor address -- and libobjc2 would walk the ANCHOR as a Class
    // (neighboring name-pointer words misread as isa/superclass; the
    // fluentuitest startup 0xC0000005).  DLL-class and Swift-class
    // targets lose nothing by the move: XCT still runs during this
    // image's _initterm, before any user code can message them.
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
// HARMONY (anchor diagnostic, lesson #485): when objc_lookUpClass(name) comes
// back null the resolver below LEAVES the slot on its .hmny_canchor anchor, by
// design -- it "faults loudly at first dispatch rather than silently".  But
// loudly is a bare 0xC0000005 at objc_msgSend+0x28 inside objc.dll with a bad
// isa, naming NOTHING: on 2026-08-27 that cost a full debugging session
// (swift-canvas's dlopen'd live view; the missing class turned out to be
// UIPinchGestureRecognizer, in the Linux UIKit build and never in
// windows.cmake), and only an lldb read of the receiver's word 0 named it.
// The anchor is SELF-DESCRIBING, so the census that session did by hand
// belongs here: after the resolution pass, every slot STILL inside an anchor
// range names a class that NO loaded image registers.  One report at load
// turns "0xC0000005 in objc.dll, no message" into "class X referenced by this
// image is not registered".
//
// Unconditional by default -- set HARMONY_ANCHOR_DIAG=0 to silence.  Every
// line is a LATENT CRASH in this image, so a healthy process prints nothing at
// all; there is no steady-state noise to gate away.
//
// No CRT here: swiftrt.obj links into EVERY Swift image, so this writes with
// WriteFile + OutputDebugStringA and reads the env with GetEnvironmentVariableA
// rather than dragging stdio/getenv into every image -- which also dodges
// ucrt's TEXT-mode stdio line-ending surprises.
namespace {

bool harmonyAnchorDiagEnabled() {
  char buf[8] = {};
  DWORD n = GetEnvironmentVariableA("HARMONY_ANCHOR_DIAG", buf, sizeof(buf));
  if (n == 0 || n >= sizeof(buf))
    return true; // unset (or something long/odd) -> on
  return !(buf[0] == '0' && buf[1] == '\0');
}

// A name pointer read out of an anchor cell must never crash the DIAGNOSTIC.
// Names live in .rdata so this is belt-and-braces, but a garbled anchor is
// exactly the situation this code runs in.
bool harmonyAnchorNameReadable(const char *p) {
  if (!p)
    return false;
  MEMORY_BASIC_INFORMATION mbi = {};
  if (!VirtualQuery(p, &mbi, sizeof(mbi)))
    return false;
  if (mbi.State != MEM_COMMIT)
    return false;
  if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
    return false;
  const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                         PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                         PAGE_EXECUTE_WRITECOPY;
  return (mbi.Protect & readable) != 0;
}

// One line, assembled in a fixed buffer and written to BOTH the debugger and
// stderr (a windowed app has no console; a console app has no debugger).
struct HarmonyDiagLine {
  char buf[512];
  size_t len;

  HarmonyDiagLine() : len(0) { buf[0] = '\0'; }

  void add(const char *s) {
    if (!s)
      s = "(null)";
    while (*s && len + 1 < sizeof(buf))
      buf[len++] = *s++;
    buf[len] = '\0';
  }

  void addUInt(unsigned v) {
    char tmp[12];
    int i = 0;
    do {
      tmp[i++] = static_cast<char>('0' + (v % 10));
      v /= 10;
    } while (v && i < static_cast<int>(sizeof(tmp)));
    while (i-- > 0 && len + 1 < sizeof(buf))
      buf[len++] = tmp[i];
    buf[len] = '\0';
  }

  void flush() {
    add("\n");
    OutputDebugStringA(buf);
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
      DWORD written = 0;
      WriteFile(h, buf, static_cast<DWORD>(len), &written, nullptr);
    }
    len = 0;
    buf[0] = '\0';
  }
};

// The image whose sections were just walked -- FROM_ADDRESS, so this names the
// DLL and not the host exe when swiftrt.obj rides in a plugin.
const char *harmonyOwningImageName(const void *addr) {
  static char path[MAX_PATH];
  HMODULE mod = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(addr), &mod))
    return "(this image)";
  DWORD n = GetModuleFileNameA(mod, path, sizeof(path));
  if (n == 0 || n >= sizeof(path))
    return "(this image)";
  const char *base = path;
  for (const char *p = path; *p; ++p)
    if (*p == '\\' || *p == '/')
      base = p + 1;
  return base;
}

// Deduplicated by NAME: one class is typically referenced from several slots,
// and the useful output is the CLASS list, not the slot list.  Fixed capacity,
// no allocation (this runs from a .CRT$XC* constructor, before user code); an
// overflow is counted and reported rather than dropped silently.
struct HarmonyAnchorCensus {
  enum Kind : unsigned {
    Classref = 1u << 0,
    Superref = 1u << 1,
    Category = 1u << 2,
    Metaclass = 1u << 3, // modifier: the slot wanted the METAclass
  };

  static const int kCapacity = 64;
  struct Entry {
    const char *name;
    unsigned slots;
    unsigned kinds;
  };
  Entry entries[kCapacity];
  int count;
  unsigned dropped;

  HarmonyAnchorCensus() : count(0), dropped(0) {}

  static bool sameName(const char *a, const char *b) {
    if (a == b)
      return true;
    while (*a && *a == *b) {
      ++a;
      ++b;
    }
    return *a == *b;
  }

  void record(const char *name, unsigned kind) {
    for (int i = 0; i < count; ++i)
      if (sameName(entries[i].name, name)) {
        ++entries[i].slots;
        entries[i].kinds |= kind;
        return;
      }
    if (count == kCapacity) {
      ++dropped;
      return;
    }
    entries[count].name = name;
    entries[count].slots = 1;
    entries[count].kinds = kind;
    ++count;
  }

  void report(const void *imageAddr) const {
    if (count == 0 || !harmonyAnchorDiagEnabled())
      return;
    HarmonyDiagLine line;
    line.add("harmony-anchor: ");
    line.add(harmonyOwningImageName(imageAddr));
    line.add(": ");
    line.addUInt(static_cast<unsigned>(count));
    line.add(" ObjC class(es) referenced by this image are registered in NO "
             "loaded image; each unresolved slot still points at its "
             ".hmny_canchor anchor and will fault at first message send "
             "(0xC0000005 in objc_msgSend, bad isa):");
    line.flush();
    for (int i = 0; i < count; ++i) {
      line.add("harmony-anchor:   ");
      line.add(entries[i].name);
      line.add("  [");
      line.addUInt(entries[i].slots);
      line.add(" slot(s):");
      if (entries[i].kinds & Classref)
        line.add(" classref");
      if (entries[i].kinds & Superref)
        line.add(" superref");
      if (entries[i].kinds & Category)
        line.add(" category");
      if (entries[i].kinds & Metaclass)
        line.add(" metaclass");
      line.add("]");
      line.flush();
    }
    if (dropped) {
      line.add("harmony-anchor:   ... and ");
      line.addUInt(dropped);
      line.add(" further unresolved slot(s) beyond the census capacity");
      line.flush();
    }
  }
};

} // namespace

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

  // The diagnostic half of resolveNamedAnchor: a slot that is an anchor but
  // did NOT resolve.  Reads the anchor's self-describing name word (word 0)
  // and reports whether it is a class or a metaclass anchor.  Never returns
  // null for an in-range address -- an anchor whose name word is unreadable
  // must still be COUNTED, or a garbled anchor would go unreported.
  auto anchorNameAt = [&](void *p, bool *isMeta) -> const char * {
    const char *cp = reinterpret_cast<const char *>(p);
    bool meta = false;
    if (!(cp >= canchorBegin && cp < canchorEnd)) {
      if (!(cp >= manchorBegin && cp < manchorEnd))
        return nullptr; // not an anchor at all
      meta = true;
    }
    if (isMeta)
      *isMeta = meta;
    const char *name = *reinterpret_cast<const char *const *>(cp);
    return harmonyAnchorNameReadable(name) ? name
                                           : "(anchor name unreadable)";
  };
  HarmonyAnchorCensus census;

  // The kind rides IN the range, so adding a range cannot leave the census
  // silently mislabelling (or skipping) it.
  struct RefRange {
    void **begin, **end;
    unsigned kind;
  } refRanges[] = {
      {reinterpret_cast<void **>(&__start_objc_classrefs + 1),
       reinterpret_cast<void **>(&__stop_objc_classrefs),
       HarmonyAnchorCensus::Classref},
      {reinterpret_cast<void **>(&__start_objc_superrefs + 1),
       reinterpret_cast<void **>(&__stop_objc_superrefs),
       HarmonyAnchorCensus::Superref},
  };
  for (const auto &range : refRanges)
    for (void **slot = range.begin; slot < range.end; ++slot) {
      if (!*slot)
        continue;
      if (void *resolved = resolveNamedAnchor(*slot)) {
        *slot = resolved;
        continue;
      }
      // Still on its anchor after BOTH passes (XCIS and this one), so
      // objc_lookUpClass came back null and the class is in no loaded image.
      // The slot is left alone -- the deliberate loud fault -- but it is now
      // NAMED before it can happen.
      bool meta = false;
      if (const char *name = anchorNameAt(*slot, &meta))
        census.record(name, range.kind |
                                (meta ? HarmonyAnchorCensus::Metaclass : 0u));
    }

  // W4.4 -> fluentui slice-7: canonicalize + register this image's
  // Swift-emitted category records (.objc_catlist) HERE, after .CRT$XCLz.
  // Each record's class word is an eager class-object pointer -- exactly
  // the classrefs problem -- but unlike classrefs a category is HANDED TO
  // libobjc2 at registration (which walks the class object immediately),
  // so an unresolved anchor is not a latent fault at first dispatch, it
  // is a crash DURING the load call: at XCIS a category targeting a
  // STATICALLY-linked imported class (uikit.lib's UIView in an exe)
  // cannot resolve -- the class registers at XCLz, after XCIS -- and
  // libobjc2 misreads the anchor's neighboring name-pointer words as
  // isa/superclass (the fluentuitest startup 0xC0000005).  XCT sorts
  // after XCLz, and getAddrOfHarmonyPEObjCClassAnchor's per-class
  // /INCLUDE guarantees a statically-linked target's registration TU was
  // pulled, so by now every linked-in class has registered.  An entry
  // whose class word STILL points into an anchor range is NULLED -- the
  // class genuinely never registered; libobjc2 skips null entries -- so
  // the loader can never walk an anchor as a Class.
  {
    void **catlistBegin = reinterpret_cast<void **>(&__start_objc_catlist + 1);
    void **catlistEnd = reinterpret_cast<void **>(&__stop_objc_catlist);
    // The fixed-root CLASS anchors (the /alternatename table): a category
    // record can reference a runtime root through one when no named anchor
    // was emitted in-image (parity with the XCIS classref treatment).
    const HarmonyMetaclassAnchor *classAnchors = kHarmonyClassAnchors;
    const int anchorCount = kHarmonyAnchorCount;
    auto isAnchorAddress = [&](void *p) -> bool {
      const char *cp = reinterpret_cast<const char *>(p);
      if (cp >= canchorBegin && cp < canchorEnd)
        return true;
      if (cp >= manchorBegin && cp < manchorEnd)
        return true;
      for (int i = 0; i < anchorCount; ++i)
        if (p == classAnchors[i].anchor)
          return true;
      return false;
    };
    for (void **entry = catlistBegin; entry < catlistEnd; ++entry) {
      if (!*entry)
        continue;
      void **cat = reinterpret_cast<void **>(*entry);
      for (int i = 0; i < anchorCount; ++i)
        if (cat[1] == classAnchors[i].anchor)
          if (void *cls = getClass(classAnchors[i].className))
            cat[1] = cls;
      if (void *r = resolveNamedAnchor(cat[1]))
        cat[1] = r;
      if (isAnchorAddress(cat[1])) {
        // Name the target BEFORE the address is discarded below.  A named
        // anchor is self-describing; a fixed-root anchor is not, so its name
        // comes from the table.
        bool meta = false;
        const char *name = anchorNameAt(cat[1], &meta);
        if (!name)
          for (int i = 0; i < anchorCount; ++i)
            if (cat[1] == classAnchors[i].anchor)
              name = classAnchors[i].className;
        if (name)
          census.record(name, HarmonyAnchorCensus::Category |
                                  (meta ? HarmonyAnchorCensus::Metaclass : 0u));
        *entry = nullptr; // target class never registered; nothing to attach
      }
    }
    // A separate probed entry point (not new parameters on
    // objc_load_swift_image_np): PE has no symbol versioning, and an
    // absent probe on an older objc.dll leaves the categories dormant --
    // the gate's category leg catches the staleness loudly -- instead of
    // silently mis-passing arguments.
    using objc_load_swift_categories_fn = void (*)(void **, void **);
    if (auto loadCategories = reinterpret_cast<objc_load_swift_categories_fn>(
            reinterpret_cast<void *>(GetProcAddress(
                objcModule, "objc_load_swift_image_categories_np")))) {
      loadCategories(catlistBegin, catlistEnd);
    }
  }

  // The whole point (lesson #485): say WHICH classes stayed unresolved, by
  // name, here at load -- not later as an unattributed access violation.  A
  // healthy image censuses zero and prints nothing.  The address hands the
  // report this image's identity (swiftrt.obj is linked into every one).
  census.report(&__start_hmny_canchor);
}

#pragma section(".CRT$XCT", long, read)

__declspec(allocate(".CRT$XCT"))
extern "C" void (*pSwiftStaticClassrefConstructor)(void) =
    &swift_resolve_static_objc_classrefs;
#pragma comment( \
    linker, "/include:" STRING(C_LABEL(pSwiftStaticClassrefConstructor)))
#endif

