set(swift_stdlib_unittest_compile_flags)

add_swift_target_library(swiftStdlibCollectionUnittest ${SWIFT_STDLIB_LIBRARY_BUILD_TYPES} IS_STDLIB
  # This file should be listed the first.  Module name is inferred from the
  # filename.
  StdlibCollectionUnittest.swift

  CheckCollectionInstance.swift
  CheckCollectionType.swift
  CheckMutableCollectionType.swift
  CheckRangeReplaceableCollectionType.swift
  CheckRangeReplaceableSliceType.swift
  CheckSequenceInstance.swift
  CheckSequenceType.swift
  COWLoggingArray.swift
  LoggingWrappers.swift
  MinimalCollections.swift
  RangeSelection.swift
  WriteBackMutableSlice.swift

  SWIFT_MODULE_DEPENDS StdlibUnittest
  SWIFT_MODULE_DEPENDS_ANDROID Android
  SWIFT_MODULE_DEPENDS_LINUX Glibc
  SWIFT_MODULE_DEPENDS_LINUX_STATIC Musl
  SWIFT_MODULE_DEPENDS_FREEBSD Glibc
  SWIFT_MODULE_DEPENDS_OPENBSD Glibc
  SWIFT_MODULE_DEPENDS_CYGWIN Glibc
  SWIFT_MODULE_DEPENDS_HAIKU Glibc
  SWIFT_MODULE_DEPENDS_WINDOWS CRT
  SWIFT_COMPILE_FLAGS ${swift_stdlib_unittest_compile_flags} ${SWIFT_STANDARD_LIBRARY_SWIFT_FLAGS}
  INSTALL_IN_COMPONENT stdlib-experimental
  DARWIN_INSTALL_NAME_DIR "${SWIFT_DARWIN_STDLIB_PRIVATE_INSTALL_NAME_DIR}"
  MACCATALYST_BUILD_FLAVOR "zippered")
