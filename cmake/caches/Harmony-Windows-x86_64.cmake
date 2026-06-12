# HARMONY (W3): the Windows fork-compiler cache -- Windows-x86_64.cmake
# trimmed to what the Harmony toolchain dist actually ships (ADR 0006 /
# the W3 handoff): clang (CGObjCGNUstep3) + lld + swift-frontend/swiftc +
# the swift-syntax host libs + resource headers + the llvm binutils the
# wincat build consumes.  No lldb / clangd / clang-tidy / sourcekit /
# remote-mirror; X86 only; no Python/libxml2/zlib (W1.1: any MSYS copy
# poisons an MSVC-ABI build, and nothing kept here needs them).

set(LLVM_ENABLE_PROJECTS
      clang
      lld
    CACHE STRING "")

set(LLVM_EXTERNAL_PROJECTS
      swift
    CACHE STRING "")

set(ENABLE_X86_RELAX_RELOCATIONS YES CACHE BOOL "")

set(LLVM_DEFAULT_TARGET_TRIPLE x86_64-unknown-windows-msvc CACHE STRING "")

set(LLVM_APPEND_VC_REV NO CACHE BOOL "")
set(LLVM_ENABLE_PER_TARGET_RUNTIME_DIR YES CACHE BOOL "")
set(LLVM_ENABLE_PYTHON NO CACHE BOOL "")

set(LLVM_TARGETS_TO_BUILD X86 CACHE STRING "")

set(LLVM_BUILD_LLVM_DYLIB NO CACHE BOOL "")
set(LLVM_BUILD_LLVM_C_DYLIB NO CACHE BOOL "")
set(LLVM_ENABLE_LIBEDIT NO CACHE BOOL "")
set(LLVM_ENABLE_LIBXML2 NO CACHE BOOL "")
set(LLVM_ENABLE_OCAMLDOC NO CACHE BOOL "")
set(LLVM_ENABLE_TERMINFO NO CACHE BOOL "")
set(LLVM_ENABLE_Z3_SOLVER NO CACHE BOOL "")
set(LLVM_ENABLE_ZLIB NO CACHE BOOL "")
set(LLVM_ENABLE_ZSTD NO CACHE BOOL "")
set(LLVM_INCLUDE_BENCHMARKS NO CACHE BOOL "")
set(LLVM_INCLUDE_DOCS NO CACHE BOOL "")
set(LLVM_INCLUDE_EXAMPLES NO CACHE BOOL "")
set(LLVM_INCLUDE_TESTS NO CACHE BOOL "")

set(CLANG_ENABLE_LIBXML2 NO CACHE BOOL "")
set(CLANG_INCLUDE_TESTS NO CACHE BOOL "")

set(SWIFT_INCLUDE_DOCS NO CACHE BOOL "")
set(SWIFT_INCLUDE_TESTS NO CACHE BOOL "")
set(SWIFT_BUILD_SOURCEKIT NO CACHE BOOL "")
set(SWIFT_BUILD_ENABLE_PARSER_LIB YES CACHE BOOL "")
set(SWIFT_BUILD_STDLIB_EXTRA_TOOLCHAIN_CONTENT NO CACHE BOOL "")
set(SWIFT_BUILD_STDLIB_CXX_MODULE NO CACHE BOOL "")
set(SWIFT_BUILD_STATIC_STDLIB NO CACHE BOOL "")
set(SWIFT_BUILD_STATIC_SDK_OVERLAY NO CACHE BOOL "")
set(SWIFT_BUILD_REMOTE_MIRROR NO CACHE BOOL "")

set(LLVM_INSTALL_BINUTILS_SYMLINKS YES CACHE BOOL "")
set(LLVM_INSTALL_TOOLCHAIN_ONLY YES CACHE BOOL "")
set(LLVM_TOOLCHAIN_TOOLS
      llvm-ar
      llvm-cvtres
      llvm-dlltool
      llvm-lib
      # NOT llvm-mt: it only exists with LLVM_ENABLE_LIBXML2 (off here),
      # and wincat explicitly uses the SDK's mt.exe, never llvm-mt.
      llvm-nm
      llvm-objcopy
      llvm-objdump
      llvm-pdbutil
      llvm-ranlib
      llvm-rc
      llvm-readobj
      llvm-strip
      llvm-symbolizer
      llvm-undname
    CACHE STRING "")

set(CLANG_TOOLS
      clang
      clang-resource-headers
    CACHE STRING "")

set(LLD_TOOLS
      lld
    CACHE STRING "")

set(SWIFT_INSTALL_COMPONENTS
      autolink-driver
      compiler
      compiler-swift-syntax-lib
      clang-builtin-headers
      tools
      swift-syntax-lib
    CACHE STRING "")

set(LLVM_DISTRIBUTION_COMPONENTS
      ${LLVM_TOOLCHAIN_TOOLS}
      ${CLANG_TOOLS}
      ${LLD_TOOLS}
      ${SWIFT_INSTALL_COMPONENTS}
    CACHE STRING "")
