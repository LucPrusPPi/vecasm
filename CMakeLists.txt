cmake_minimum_required(VERSION 3.20)

# Resolve NASM before enable_language to avoid reconfigure loops.
list(APPEND CMAKE_PROGRAM_PATH
  "${CMAKE_SOURCE_DIR}/tools/nasm/nasm-2.16.03"
  "${CMAKE_CURRENT_SOURCE_DIR}/tools/nasm/nasm-2.16.03"
)
if(NOT CMAKE_ASM_NASM_COMPILER)
  find_program(CMAKE_ASM_NASM_COMPILER NAMES nasm nasm.exe REQUIRED)
endif()

project(vecasm C CXX ASM_NASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(VECASM_BUILD_TESTS "Build tests" ON)
option(VECASM_BUILD_BENCH "Build benchmarks" ON)

# Object format + calling convention per OS.
if(WIN32)
  set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
  set(VECASM_NASM_ABI "VECASM_ABI_WIN64")
  set(_VECASM_NASM_DBG "-g -F cv8")
elseif(APPLE)
  set(CMAKE_ASM_NASM_OBJECT_FORMAT macho64)
  set(VECASM_NASM_ABI "VECASM_ABI_SYSV")
  set(_VECASM_NASM_DBG "")
else()
  set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
  set(VECASM_NASM_ABI "VECASM_ABI_SYSV")
  set(_VECASM_NASM_DBG "")
endif()

# Include path for %include "abi.inc" + ABI define.
set(CMAKE_ASM_NASM_FLAGS "${_VECASM_NASM_DBG} -I${CMAKE_SOURCE_DIR}/asm/x86_64/ -D${VECASM_NASM_ABI}")

set(VECASM_ASM_SOURCES
  asm/x86_64/dot_f32_avx2.asm
  asm/x86_64/sum_f32_avx2.asm
  asm/x86_64/axpy_f32_avx2.asm
  asm/x86_64/dot_f32_avx512.asm
  asm/x86_64/sum_f32_avx512.asm
  asm/x86_64/axpy_f32_avx512.asm
  asm/x86_64/dot_f32_avx512icl.asm
  asm/x86_64/sum_f32_avx512icl.asm
  asm/x86_64/axpy_f32_avx512icl.asm
  asm/x86_64/dot3_f32.asm
  asm/x86_64/cross3_f32.asm
  asm/x86_64/normalize3_f32.asm
)

add_library(vecasm
  src/scalar.c
  src/dispatch.c
  ${VECASM_ASM_SOURCES}
)
target_include_directories(vecasm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_definitions(vecasm PRIVATE
  $<$<COMPILE_LANGUAGE:C>:VECASM_BUILD>
  $<$<COMPILE_LANGUAGE:CXX>:VECASM_BUILD>
)
if(MSVC)
  target_compile_options(vecasm PRIVATE
    $<$<COMPILE_LANGUAGE:C>:/O2>
    $<$<COMPILE_LANGUAGE:CXX>:/O2>
  )
else()
  target_compile_options(vecasm PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-O3>
    $<$<COMPILE_LANGUAGE:CXX>:-O3>
  )
endif()
# Prevent MSVC /O2 from leaking into nasm.
set_source_files_properties(${VECASM_ASM_SOURCES} PROPERTIES COMPILE_FLAGS "")

add_library(vecasm::vecasm ALIAS vecasm)

if(NOT MSVC)
  # sqrtf/sqrt live in libm; propagate to all link consumers (tests, bench, apps).
  target_link_libraries(vecasm PUBLIC m)
endif()

if(VECASM_BUILD_TESTS)
  add_executable(vecasm_test_dot tests/test_dot.c)
  target_link_libraries(vecasm_test_dot PRIVATE vecasm)
endif()

if(VECASM_BUILD_BENCH)
  add_executable(vecasm_bench_dot bench/bench_dot.c)
  target_link_libraries(vecasm_bench_dot PRIVATE vecasm)
endif()

install(TARGETS vecasm ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(FILES include/vecasm.h include/vecasm.hpp DESTINATION include)
install(FILES LICENSE NOTICE DESTINATION share/doc/vecasm)
