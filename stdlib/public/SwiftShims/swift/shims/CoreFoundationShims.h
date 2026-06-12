//===--- CoreFoundationShims.h - Access to CF for the core stdlib ---------===//
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
//  Using the CoreFoundation module in the core stdlib would create a
//  circular dependency, so instead we import these declarations as
//  part of SwiftShims.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_STDLIB_SHIMS_COREFOUNDATIONSHIMS_H
#define SWIFT_STDLIB_SHIMS_COREFOUNDATIONSHIMS_H

#include "SwiftStdint.h"
#include "Visibility.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __OBJC2__
// HARMONY (W3): this header is parsed standalone (the SwiftShims module),
// where nothing defines __LLP64__ -- CFBase.h derives it from _WIN64 for
// CF's own TUs, so on Win64 the shims silently took the 32-bit branch
// while the live CF ABI is 64-bit (the same compiler-vs-runtime-disagree
// shape as the 6i SWIFT_CLASS_IS_SWIFT_MASK find).  Make the guard
// self-sufficient; Darwin/Linux preprocess identically.
#if defined(__LLP64__) || defined(_WIN64)
typedef unsigned long long _swift_shims_CFHashCode;
typedef signed long long _swift_shims_CFIndex;
#else
typedef unsigned long _swift_shims_CFHashCode;
typedef signed long _swift_shims_CFIndex;
#endif

// HARMONY (W3): NSUInteger is pointer-sized on every platform this stack
// serves (wincat: uintptr_t); bare unsigned long is 32-bit on LLP64 and
// would truncate -hash through the trampolines.
#if defined(_WIN64)
typedef unsigned long long _swift_shims_NSUInteger;
#else
typedef unsigned long _swift_shims_NSUInteger;
#endif

// Consider creating SwiftMacTypes.h for these
typedef unsigned char _swift_shims_Boolean;
typedef __swift_uint8_t _swift_shims_UInt8;
typedef __swift_uint32_t _swift_shims_CFStringEncoding;

/* This is layout-compatible with constant CFStringRefs on Darwin */
typedef struct __swift_shims_builtin_CFString {
  const void * _Nonnull isa; // point to __CFConstantStringClassReference
  unsigned long flags;
  const __swift_uint8_t * _Nonnull str;
  unsigned long length;
} _swift_shims_builtin_CFString;

SWIFT_RUNTIME_STDLIB_API
__swift_uint8_t _swift_stdlib_isNSString(id _Nonnull obj);

SWIFT_RUNTIME_STDLIB_API
_swift_shims_CFHashCode _swift_stdlib_CFStringHashNSString(id _Nonnull obj);

SWIFT_RUNTIME_STDLIB_API
_swift_shims_CFHashCode
_swift_stdlib_CFStringHashCString(const _swift_shims_UInt8 * _Nonnull bytes,
                                  _swift_shims_CFIndex length);

SWIFT_RUNTIME_STDLIB_API
const __swift_uint8_t * _Nullable
_swift_stdlib_NSStringCStringUsingEncodingTrampoline(id _Nonnull obj,
                                                     _swift_shims_NSUInteger encoding);

SWIFT_RUNTIME_STDLIB_API
__swift_uint8_t
_swift_stdlib_NSStringGetCStringTrampoline(id _Nonnull obj,
                                           _swift_shims_UInt8 *_Nonnull buffer,
                                           _swift_shims_CFIndex maxLength,
                                           _swift_shims_NSUInteger encoding);

SWIFT_RUNTIME_STDLIB_API
__swift_uint8_t
_swift_stdlib_dyld_is_objc_constant_string(const void * _Nonnull addr);

SWIFT_RUNTIME_STDLIB_API
const void * _Nullable
_swift_stdlib_CreateIndirectTaggedPointerString(const __swift_uint8_t * _Nonnull bytes,
                                                _swift_shims_CFIndex len);

SWIFT_RUNTIME_STDLIB_API
_swift_shims_NSUInteger
_swift_stdlib_NSStringLengthOfBytesInEncodingTrampoline(id _Nonnull obj,
                                                        _swift_shims_NSUInteger encoding);

#endif // __OBJC2__

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SWIFT_STDLIB_SHIMS_COREFOUNDATIONSHIMS_H

