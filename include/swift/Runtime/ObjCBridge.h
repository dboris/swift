//===--- ObjCBridge.h - Swift Language Objective-C Bridging ABI -*- C++ -*-===//
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
//
// Swift ABI for interacting with Objective-C.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_ABI_OBJCBRIDGE_H
#define SWIFT_ABI_OBJCBRIDGE_H

#include "swift/Runtime/Config.h"
#include <cstdint>

struct objc_class;

namespace swift {

template <typename Runtime> struct TargetMetadata;
using Metadata = TargetMetadata<InProcess>;

struct HeapObject;

} // end namespace swift

#if SWIFT_OBJC_INTEROP
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/objc-api.h>

// Redeclare APIs from the Objective-C runtime.
// These functions are not available through public headers, but are guaranteed
// to exist on OS X >= 10.9 and iOS >= 7.0.

// HARMONY (slice 6h): OBJC_EXPORT comes from objc4's objc-api.h; libobjc2
// ships an objc-api.h that does not define it. The declarations below are
// objc4 SPI that Swift redeclares itself, and libobjc2 exports every one of
// them (the objc-arc.h family), so the only gap is the macro. Pull
// objc-arc.h too: it declares the parts of the ARC weak family that are
// PUBLIC-header API on Darwin and so are not redeclared below
// (objc_storeWeak). Darwin availability annotations degrade to no-ops.
#if __has_include(<objc/objc-arc.h>)
#  include <objc/objc-arc.h>
#endif
#ifndef OBJC_EXPORT
#  if defined(__cplusplus)
#    define OBJC_EXPORT extern "C"
#  else
#    define OBJC_EXPORT extern
#  endif
#endif
#ifndef __OSX_AVAILABLE_STARTING
#  define __OSX_AVAILABLE_STARTING(osx, ios)
#endif

OBJC_EXPORT id objc_retain(id);
OBJC_EXPORT void objc_release(id);
OBJC_EXPORT id _objc_rootAutorelease(id);
OBJC_EXPORT void objc_moveWeak(id*, id*);
OBJC_EXPORT void objc_copyWeak(id*, id*);
OBJC_EXPORT id objc_initWeak(id*, id);
OBJC_EXPORT void objc_destroyWeak(id*);
OBJC_EXPORT id objc_loadWeakRetained(id*);

// Description of an Objective-C image.
// __DATA,__objc_imageinfo stores one of these.
typedef struct objc_image_info {
    uint32_t version; // currently 0
    uint32_t flags;
} objc_image_info;

// Class and metaclass construction from a compiler-generated memory image.
// cls and cls->isa must each be OBJC_MAX_CLASS_SIZE bytes.
// Extra bytes not used the metadata must be zero.
// info is the same objc_image_info that would be emitted by a static compiler.
// Returns nil if a class with the same name already exists.
// Returns nil if the superclass is nil and the class is not marked as a root.
// Returns nil if the superclass is under construction.
// Do not call objc_registerClassPair().
OBJC_EXPORT Class objc_readClassPair(Class cls,
                                     const struct objc_image_info *info)
    __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);

// Magic symbol whose _address_ is the runtime's isa mask.
OBJC_EXPORT const struct { char c; } objc_absolute_packed_isa_class_mask;

// HARMONY (slice 6i): clang's gnustep codegen defines an ObjC class as
// `._OBJC_CLASS_X` / `._OBJC_METACLASS_X` (the metaclass symbol TU-local),
// while swiftc-emitted code references the objc4 names `OBJC_CLASS_$_X` /
// `OBJC_METACLASS_$_X`.  For the runtime's own clang-compiled classes, alias
// the objc4 names onto the gnustep definitions IN THE DEFINING TU -- the
// proven 6c.4 technique (a linker --defsym would be an absolute, unrelocated
// symbol, and the local metaclass is unreachable from other TUs anyway).
#if defined(_WIN32)
// HARMONY (W3): the COFF arm -- the W2 /alternatename pattern; gnustep-3.0
// class objects are $-prefixed on COFF (llvm-nm verified in spike-16) and
// .set is ELF-only (it assembled silently to garbage in W2 wall 6).
// NO metaclass alternatename here: SwiftRT-COFF.cpp maps the objc4
// metaclass names onto per-image link anchors (one alternatename per
// symbol per link -- two targets is an lld hard error), and its image
// constructor rewrites anchor-pointing metaclass words through the
// runtime.  The class-object alias stays: those references are
// intra-image and the gnustep class object is global.
#define SWIFT_HARMONY_OBJC4_CLASS_ALIAS(name)                                  \
  __pragma(comment(linker,                                                     \
      "/alternatename:OBJC_CLASS_$_" #name "=$_OBJC_CLASS_" #name))            \
  static_assert(true, "swallow the call-site semicolon")
#elif !defined(__APPLE__)
#define SWIFT_HARMONY_OBJC4_CLASS_ALIAS(name)                                  \
  __asm__(".globl \"OBJC_CLASS_$_" #name "\"\n"                                \
          ".set \"OBJC_CLASS_$_" #name "\", ._OBJC_CLASS_" #name "\n"          \
          ".globl \"OBJC_METACLASS_$_" #name "\"\n"                            \
          ".set \"OBJC_METACLASS_$_" #name "\", ._OBJC_METACLASS_" #name "\n")
#else
#define SWIFT_HARMONY_OBJC4_CLASS_ALIAS(name)
#endif

// HARMONY (slice 6i): objc4 SPI that the runtime consults through
// SWIFT_RUNTIME_WEAK_CHECK and that libobjc2 does not provide.  Off-Darwin,
// declare them weak: ELF resolves unsatisfied weak references to null, so
// the checks fail cleanly and select the eager fallbacks
// (swift_instantiateObjCClass instead of stub realization; eager
// generic-class naming instead of the lazy-namer hook) -- the same paths
// these call sites take on pre-2018 objc4.
#if !defined(__APPLE__)
OBJC_EXPORT __attribute__((weak)) Class _Nullable
_objc_realizeClassFromSwift(Class _Nullable cls, void *_Nullable previously);
typedef const char *_Nullable (*objc_hook_lazyClassNamer)(Class _Nonnull cls);
OBJC_EXPORT __attribute__((weak)) void
objc_setHook_lazyClassNamer(objc_hook_lazyClassNamer _Nonnull newValue,
                            objc_hook_lazyClassNamer _Nullable *_Nonnull outOldValue);
#endif


namespace swift {

// Root -dealloc implementation for classes with Swift reference counting.
// This function should be used to implement -dealloc in a root class with
// Swift reference counting. [super dealloc] MUST NOT be called after this,
// for the object will have already been deallocated by the time
// this function returns.
SWIFT_RUNTIME_EXPORT
void swift_rootObjCDealloc(HeapObject *self);

// Uses Swift bridging to box a C string into an NSString without introducing
// a link-time dependency on NSString.
SWIFT_CC(swift) SWIFT_RUNTIME_STDLIB_API
id swift_stdlib_NSStringFromUTF8(const char *cstr, int len);

}

#endif // SWIFT_OBJC_INTEROP

#endif // SWIFT_ABI_OBJCBRIDGE_H
