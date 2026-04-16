cmake_minimum_required(VERSION 3.20)
project(Masstree VERSION 0.1 LANGUAGES CXX)

include(CheckCXXSymbolExists)
include(CheckIncludeFileCXX)
include(CheckCXXSourceCompiles)
include(CheckTypeSize)
include(TestBigEndian)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------------
# User-facing options
# ---------------------------------------------------------------------------
option(MASSTREE_BUILD_TESTS
       "Build mttest / mtd / mtclient / scantest / jsontest / msgpacktest / test_atomics / test_string / unit-mt"
       ${PROJECT_IS_TOP_LEVEL})
option(MASSTREE_ENABLE_NUMA       "Link against libnuma (Linux only)"      OFF)
option(MASSTREE_ENABLE_JEMALLOC   "Link against jemalloc"                  OFF)
option(MASSTREE_ENABLE_TCMALLOC   "Link against gperftools tcmalloc"       OFF)
option(MASSTREE_ENABLE_MEMDEBUG   "Enable masstree memory debugging hooks" OFF)
option(MASSTREE_ENABLE_SUPERPAGE  "Enable hugepage/superpage allocation"   OFF)

set(MASSTREE_NODEVERSION_IMPL "atomic" CACHE STRING
    "Nodeversion implementation (handrolled / atomicallfences / atomic)")
set_property(CACHE MASSTREE_NODEVERSION_IMPL PROPERTY STRINGS
             handrolled atomicallfences atomic)

set(MASSTREE_STRINGBAG_IMPL "atomic" CACHE STRING
    "Stringbag implementation (original / atomic / atomicref)")
set_property(CACHE MASSTREE_STRINGBAG_IMPL PROPERTY STRINGS
             original atomic atomicref)

set(MASSTREE_RELAX_FENCE "pause" CACHE STRING
    "Relax fence backoff hint (pause / schedyield / none)")
set_property(CACHE MASSTREE_RELAX_FENCE PROPERTY STRINGS pause schedyield none)

set(MASSTREE_ATOMIC_FENCE_DEFAULT "thread" CACHE STRING
    "Default atomic fence implementation (thread / signal). 'thread' is correct on all architectures; 'signal' matches the autoconf default (only safe on strongly-ordered archs like x86).")
set_property(CACHE MASSTREE_ATOMIC_FENCE_DEFAULT PROPERTY STRINGS thread signal)

set(MASSTREE_ROW_TYPE "bag" CACHE STRING
    "Default row type used by mttest / mtd / mtclient (bag / str / array / array_ver). Does not affect consumers that set their own query_table_params::value_type.")
set_property(CACHE MASSTREE_ROW_TYPE PROPERTY STRINGS bag str array array_ver)

set(MASSTREE_MAXKEYLEN    255 CACHE STRING "Maximum key length in bytes.")
set(MASSTREE_CACHE_LINE_SIZE 64 CACHE STRING "Assumed cache-line size in bytes.")

# ---------------------------------------------------------------------------
# Dependency probes
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)

set(MASSTREE_HAS_NUMA 0)
if(MASSTREE_ENABLE_NUMA)
  find_library(NUMA_LIBRARY NAMES numa)
  check_include_file_cxx(numa.h HAVE_NUMA_H)
  if(NUMA_LIBRARY AND HAVE_NUMA_H)
    set(MASSTREE_HAS_NUMA 1)
  else()
    message(FATAL_ERROR "MASSTREE_ENABLE_NUMA=ON but libnuma / numa.h not found")
  endif()
endif()

set(MASSTREE_HAS_JEMALLOC 0)
if(MASSTREE_ENABLE_JEMALLOC)
  find_library(JEMALLOC_LIBRARY NAMES jemalloc)
  if(JEMALLOC_LIBRARY)
    set(MASSTREE_HAS_JEMALLOC 1)
  else()
    message(FATAL_ERROR "MASSTREE_ENABLE_JEMALLOC=ON but libjemalloc not found")
  endif()
endif()

set(MASSTREE_HAS_TCMALLOC 0)
if(MASSTREE_ENABLE_TCMALLOC)
  find_library(TCMALLOC_LIBRARY NAMES tcmalloc_minimal tcmalloc)
  if(TCMALLOC_LIBRARY)
    set(MASSTREE_HAS_TCMALLOC 1)
  else()
    message(FATAL_ERROR "MASSTREE_ENABLE_TCMALLOC=ON but libtcmalloc not found")
  endif()
endif()

# ---------------------------------------------------------------------------
# Feature detection → fills in config.h
# ---------------------------------------------------------------------------
check_include_file_cxx(execinfo.h HAVE_EXECINFO_H)
check_include_file_cxx(sys/epoll.h HAVE_SYS_EPOLL_H)
check_include_file_cxx(sys/types.h HAVE_SYS_TYPES_H)
check_include_file_cxx(inttypes.h HAVE_INTTYPES_H)
check_include_file_cxx(type_traits HAVE_TYPE_TRAITS)

check_cxx_symbol_exists(vsnprintf stdio.h HAVE_VSNPRINTF)
check_cxx_symbol_exists(strtoul stdlib.h HAVE_STRTOUL)
check_cxx_source_compiles("
  #include <stdint.h>
  int main(){ uintptr_t p = 0; return (int)p; }" HAVE_UINTPTR_T)

foreach(_builtin __builtin_clz __builtin_clzl __builtin_clzll
                 __builtin_ctz __builtin_ctzl __builtin_ctzll)
  string(TOUPPER "HAVE_${_builtin}" _var)
  check_cxx_source_compiles("int main(){ return ${_builtin}(1); }" ${_var})
endforeach()

foreach(_sync __sync_bool_compare_and_swap __sync_val_compare_and_swap
              __sync_fetch_and_add)
  string(TOUPPER "HAVE_${_sync}" _var)
  check_cxx_source_compiles("
    int main(){ int x=0; ${_sync}(&x,0,1); return 0; }" ${_var})
endforeach()
check_cxx_source_compiles("
  #include <stdint.h>
  int main(){ uint64_t x=0; __sync_bool_compare_and_swap(&x,0ULL,1ULL); return 0; }
  " HAVE___SYNC_BOOL_COMPARE_AND_SWAP_8)
check_cxx_source_compiles("
  #include <stdint.h>
  int main(){ uint64_t x=0; __sync_val_compare_and_swap(&x,0ULL,1ULL); return 0; }
  " HAVE___SYNC_VAL_COMPARE_AND_SWAP_8)
check_cxx_source_compiles("
  #include <stdint.h>
  int main(){ uint64_t x=0; __sync_fetch_and_add(&x,1ULL); return 0; }
  " HAVE___SYNC_FETCH_AND_ADD_8)
check_cxx_source_compiles("
  #include <stdint.h>
  int main(){ uint64_t x=0; __sync_fetch_and_or(&x,1ULL); return 0; }
  " HAVE___SYNC_FETCH_AND_OR_8)

check_cxx_source_compiles("
  struct S { S(const S&)=default; };
  int main(){ return __has_trivial_copy(S); }" HAVE___HAS_TRIVIAL_COPY)
check_cxx_source_compiles("
  struct S { ~S()=default; };
  int main(){ return __has_trivial_destructor(S); }" HAVE___HAS_TRIVIAL_DESTRUCTOR)
check_cxx_source_compiles("
  #include <type_traits>
  int main(){ return std::is_trivially_copyable<int>::value; }
  " HAVE_STD_IS_TRIVIALLY_COPYABLE)
check_cxx_source_compiles("
  #include <type_traits>
  int main(){ return std::is_trivially_destructible<int>::value; }
  " HAVE_STD_IS_TRIVIALLY_DESTRUCTIBLE)
check_cxx_source_compiles("
  #include <functional>
  int main(){ std::hash<int> h; return h(1)!=0; }" HAVE_STD_HASH)
check_cxx_source_compiles("
  template <class T> using alias = T*;
  int main(){ alias<int> p = 0; return p==0; }" HAVE_CXX_TEMPLATE_ALIAS)
check_cxx_source_compiles("
  int operator\"\" _foo(unsigned long long){ return 0; }
  int main(){ return 1_foo; }" HAVE_CXX_USER_LITERALS)

# Type-size driven defines
set(CMAKE_EXTRA_INCLUDE_FILES "stdint.h")
check_type_size("int64_t"       SIZEOF_INT64_T   LANGUAGE CXX)
set(CMAKE_EXTRA_INCLUDE_FILES)
check_type_size("short"         SIZEOF_SHORT     LANGUAGE CXX BUILTIN_TYPES_ONLY)
check_type_size("int"           SIZEOF_INT       LANGUAGE CXX BUILTIN_TYPES_ONLY)
check_type_size("long"          SIZEOF_LONG      LANGUAGE CXX BUILTIN_TYPES_ONLY)
check_type_size("long long"     SIZEOF_LONG_LONG LANGUAGE CXX BUILTIN_TYPES_ONLY)
check_type_size("void *"        SIZEOF_VOID_P    LANGUAGE CXX BUILTIN_TYPES_ONLY)
check_type_size("off_t"         SIZEOF_OFF_T     LANGUAGE CXX)
check_type_size("size_t"        SIZEOF_SIZE_T    LANGUAGE CXX)
check_type_size("unsigned"      SIZEOF_UNSIGNED  LANGUAGE CXX BUILTIN_TYPES_ONLY)
check_type_size("unsigned long" SIZEOF_UNSIGNED_LONG      LANGUAGE CXX BUILTIN_TYPES_ONLY)
check_type_size("unsigned long long" SIZEOF_UNSIGNED_LONG_LONG LANGUAGE CXX BUILTIN_TYPES_ONLY)

set(HAVE_LONG_LONG 1)
if(SIZEOF_INT64_T EQUAL SIZEOF_LONG)
  set(HAVE_INT64_T_IS_LONG 1)
  set(HAVE_INT64_IS_LONG 1)
endif()
if(SIZEOF_INT64_T EQUAL SIZEOF_LONG_LONG AND NOT SIZEOF_INT64_T EQUAL SIZEOF_LONG)
  set(HAVE_INT64_T_IS_LONG_LONG 1)
  set(HAVE_INT64_IS_LONG_LONG 1)
endif()
if(SIZEOF_OFF_T EQUAL SIZEOF_LONG_LONG)
  set(HAVE_OFF_T_IS_LONG_LONG 1)
endif()
if(SIZEOF_SIZE_T EQUAL SIZEOF_UNSIGNED_LONG_LONG AND
   NOT SIZEOF_UNSIGNED_LONG_LONG EQUAL SIZEOF_UNSIGNED_LONG)
  set(HAVE_SIZE_T_IS_UNSIGNED_LONG_LONG 1)
elseif(SIZEOF_SIZE_T EQUAL SIZEOF_UNSIGNED_LONG)
  set(HAVE_SIZE_T_IS_UNSIGNED_LONG 1)
endif()

# Unaligned access + big-endian detection
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3456]86|x86_64|amd64|AMD64|aarch64|arm.*|ARM.*)$")
  set(HAVE_UNALIGNED_ACCESS 1)
  set(HAVE_INDIFFERENT_ALIGNMENT 1)
endif()
test_big_endian(WORDS_BIGENDIAN)

# Variant selectors. For `#cmakedefine FOO 1`, CMake emits the define when
# ${FOO} is truthy (non-empty, not 0/OFF/FALSE/NO) and leaves it undefined
# otherwise. So we only set the selected variant.
if(MASSTREE_NODEVERSION_IMPL STREQUAL "handrolled")
  set(NODEVERSION_IMPL_HANDROLLED 1)
elseif(MASSTREE_NODEVERSION_IMPL STREQUAL "atomicallfences")
  set(NODEVERSION_IMPL_ATOMICALLFENCES 1)
elseif(MASSTREE_NODEVERSION_IMPL STREQUAL "atomic")
  set(NODEVERSION_IMPL_ATOMIC 1)
else()
  message(FATAL_ERROR "Unknown MASSTREE_NODEVERSION_IMPL=${MASSTREE_NODEVERSION_IMPL}")
endif()

if(MASSTREE_STRINGBAG_IMPL STREQUAL "original")
  set(STRINGBAG_IMPL_ORIGINAL 1)
elseif(MASSTREE_STRINGBAG_IMPL STREQUAL "atomic")
  set(STRINGBAG_IMPL_ATOMIC 1)
elseif(MASSTREE_STRINGBAG_IMPL STREQUAL "atomicref")
  set(STRINGBAG_IMPL_ATOMICREF 1)
else()
  message(FATAL_ERROR "Unknown MASSTREE_STRINGBAG_IMPL=${MASSTREE_STRINGBAG_IMPL}")
endif()

if(MASSTREE_RELAX_FENCE STREQUAL "pause")
  set(RELAX_FENCE_PAUSE 1)
elseif(MASSTREE_RELAX_FENCE STREQUAL "schedyield")
  set(RELAX_FENCE_SCHED_YIELD 1)
elseif(NOT MASSTREE_RELAX_FENCE STREQUAL "none")
  message(FATAL_ERROR "Unknown MASSTREE_RELAX_FENCE=${MASSTREE_RELAX_FENCE}")
endif()

if(MASSTREE_ATOMIC_FENCE_DEFAULT STREQUAL "thread")
  set(ATOMIC_THREAD_FENCE_DEFAULT 1)
elseif(MASSTREE_ATOMIC_FENCE_DEFAULT STREQUAL "signal")
  set(ATOMIC_SIGNAL_FENCE_DEFAULT 1)
else()
  message(FATAL_ERROR "Unknown MASSTREE_ATOMIC_FENCE_DEFAULT=${MASSTREE_ATOMIC_FENCE_DEFAULT}")
endif()

if(MASSTREE_ROW_TYPE STREQUAL "array")
  set(MASSTREE_ROW_TYPE_ARRAY 1)
elseif(MASSTREE_ROW_TYPE STREQUAL "array_ver")
  set(MASSTREE_ROW_TYPE_ARRAY_VER 1)
elseif(MASSTREE_ROW_TYPE STREQUAL "bag")
  set(MASSTREE_ROW_TYPE_BAG 1)
elseif(MASSTREE_ROW_TYPE STREQUAL "str")
  set(MASSTREE_ROW_TYPE_STR 1)
else()
  message(FATAL_ERROR "Unknown MASSTREE_ROW_TYPE=${MASSTREE_ROW_TYPE}")
endif()

if(MASSTREE_ENABLE_MEMDEBUG)
  set(HAVE_MEMDEBUG 1)
endif()
if(MASSTREE_ENABLE_SUPERPAGE)
  set(HAVE_SUPERPAGE 1)
endif()
if(MASSTREE_HAS_NUMA)
  set(HAVE_LIBNUMA 1)
endif()
if(MASSTREE_HAS_JEMALLOC)
  set(HAVE_JEMALLOC 1)
endif()
if(MASSTREE_HAS_TCMALLOC)
  set(HAVE_TCMALLOC 1)
endif()

# Assertion family follows CMAKE_BUILD_TYPE (matches configure-all.sh's
# --disable-assertions behavior in Release). ENABLE_ASSERTIONS is left
# undefined in Release (autoconf emits /* #undef ENABLE_ASSERTIONS */);
# ENABLE_PRECONDITIONS / ENABLE_INVARIANTS are always defined as 0 or 1
# because the AH_BOTTOM logic reads them as values.
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  set(ENABLE_PRECONDITIONS 0)
  set(ENABLE_INVARIANTS    0)
else()
  set(ENABLE_ASSERTIONS    1)
  set(ENABLE_PRECONDITIONS 1)
  set(ENABLE_INVARIANTS    1)
endif()

set(CACHE_LINE_SIZE ${MASSTREE_CACHE_LINE_SIZE})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
set(MASSTREE_LIB_SOURCES
    atomic_pthread.cc
    checkpoint.cc
    clp.c
    compiler.cc
    file.cc
    json.cc
    kvio.cc
    kvrandom.cc
    kvthread.cc
    log.cc
    memdebug.cc
    misc.cc
    msgpack.cc
    perfstat.cc
    query_masstree.cc
    str.cc
    straccum.cc
    string.cc
    string_slice.cc
    value_array.cc
    value_string.cc
    value_versioned_array.cc)

add_library(masstree STATIC ${MASSTREE_LIB_SOURCES})
add_library(masstree::masstree ALIAS masstree)

# clp.c is C source but uses C++-only syntax from the force-included config.h
# (default argument values on extern declarations). The autoconf Makefile
# worked around this by compiling it with $(CXX); replicate that here.
set_source_files_properties(clp.c PROPERTIES LANGUAGE CXX)

target_compile_features(masstree PUBLIC cxx_std_20)
target_compile_options(masstree PRIVATE -W -Wall)

# Force-include the generated config.h so every TU — inside the library and in
# any consumer that links against masstree — sees the same HAVE_*/ENABLE_*
# macros. This matches the autoconf Makefile's `-include config.h` behavior.
target_compile_options(masstree PUBLIC
  $<BUILD_INTERFACE:SHELL:-include ${CMAKE_CURRENT_BINARY_DIR}/config.h>)

target_include_directories(masstree PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_link_libraries(masstree PUBLIC Threads::Threads)
if(MASSTREE_HAS_NUMA)
  target_link_libraries(masstree PUBLIC ${NUMA_LIBRARY})
endif()
if(MASSTREE_HAS_JEMALLOC)
  target_link_libraries(masstree PUBLIC ${JEMALLOC_LIBRARY})
endif()
if(MASSTREE_HAS_TCMALLOC)
  target_link_libraries(masstree PUBLIC ${TCMALLOC_LIBRARY})
endif()

# ---------------------------------------------------------------------------
# Test / demo programs
# ---------------------------------------------------------------------------
if(MASSTREE_BUILD_TESTS)
  # mttest and mtclient pull in testrunner.cc (testrunner_base globals).
  add_executable(mttest          mttest.cc      testrunner.cc)
  add_executable(mtclient        mtclient.cc    testrunner.cc)
  add_executable(mtd             mtd.cc)
  add_executable(jsontest        jsontest.cc)
  add_executable(msgpacktest     msgpacktest.cc)
  add_executable(scantest        scantest.cc)
  add_executable(test_atomics    test_atomics.cc)
  add_executable(test_atomic_str test_atomic_str.cc)
  add_executable(test_string     test_string.cc)
  add_executable(unit-mt         unit-mt.cc)

  foreach(_tgt mttest mtd mtclient jsontest msgpacktest scantest
               test_atomics test_atomic_str test_string unit-mt)
    target_link_libraries(${_tgt} PRIVATE masstree::masstree)
  endforeach()

  # Register unattended pass/fail tests with ctest.
  # Exclusions:
  #   - mttest    — 20-second benchmark, not a pass/fail test.
  #   - mtd / mtclient — need coordinated launch; not suitable for `ctest`.
  if(PROJECT_IS_TOP_LEVEL)
    enable_testing()
  endif()
  foreach(_t unit-mt scantest test_atomics test_atomic_str test_string
             jsontest msgpacktest)
    add_test(NAME ${_t} COMMAND ${_t})
  endforeach()
endif()

# ---------------------------------------------------------------------------
# Install / export (only when consumed standalone)
# ---------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
  install(TARGETS masstree EXPORT masstreeTargets
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
          INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT masstreeTargets
          FILE masstreeTargets.cmake
          NAMESPACE masstree::
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/masstree)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/masstreeConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
endif()
