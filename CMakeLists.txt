cmake_minimum_required(VERSION 3.20)
project(TTPlayerV2M LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Use -DCMAKE_TOOLCHAIN_FILE=cmake/mingw32.cmake")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "TTPlayer is 32-bit; a Win32 compiler is required")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TTP_USE_ASM_SYNTH "Use original NASM synth engine (official timbre)" ON)
option(TTP_ENABLE_RONAN "Enable RONAN speech synthesis" ON)

set(TTP_V2M_ENGINE_SOURCES)
if(TTP_USE_ASM_SYNTH)
  enable_language(ASM_NASM)
  list(APPEND TTP_V2M_ENGINE_SOURCES src/v2m/synth.asm)
  if(TTP_ENABLE_RONAN)
    list(APPEND TTP_V2M_ENGINE_SOURCES src/v2m/ronan.cpp)
  else()
    list(APPEND TTP_V2M_ENGINE_SOURCES src/v2m/synth_stub.cpp)
  endif()
  set_source_files_properties(src/v2m/synth.asm PROPERTIES LANGUAGE ASM_NASM)
else()
  list(APPEND TTP_V2M_ENGINE_SOURCES src/v2m/synth_core.cpp)
endif()

add_library(ttp_v2m SHARED
  src/plugin.cpp
  src/reader.cpp
  src/v2m/v2init.cpp
  src/v2m/v2mplayer.cpp
  src/v2m/v2mconv.cpp
  ${TTP_V2M_ENGINE_SOURCES}
)

target_include_directories(ttp_v2m PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/v2m
)

target_compile_definitions(ttp_v2m PRIVATE
  UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  _WIN32_WINNT=0x0501 _CRT_SECURE_NO_WARNINGS
  V2MPLAYER_SYNC_FUNCTIONS=1
)

if(TTP_USE_ASM_SYNTH AND TTP_ENABLE_RONAN)
  target_compile_definitions(ttp_v2m PRIVATE RONAN=1)
endif()

if(MSVC)
  target_compile_options(ttp_v2m PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W3;/EHsc;/GR-;/O2;/fp:precise;/arch:SSE2>")
  target_link_libraries(ttp_v2m PRIVATE ole32 uuid shlwapi)
else()
  target_compile_options(ttp_v2m PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-O2;-msse2;-fno-strict-aliasing;-ffp-contract=off;-fno-asynchronous-unwind-tables>")
  target_link_libraries(ttp_v2m PRIVATE ole32 uuid shlwapi)
  target_link_options(ttp_v2m PRIVATE
    -static -static-libgcc -static-libstdc++ -Wl,--exclude-libs,ALL -Wl,--kill-at
  )
endif()

set_target_properties(ttp_v2m PROPERTIES
  PREFIX "" OUTPUT_NAME "ttp_v2m" SUFFIX ".dll"
)

add_executable(ttp_v2m_test tests/test_host.cpp)
target_include_directories(ttp_v2m_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ttp_v2m_test PRIVATE ole32 uuid shlwapi shell32)
if(NOT MSVC)
  target_compile_options(ttp_v2m_test PRIVATE -O2 -msse2)
  target_link_options(ttp_v2m_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()

install(TARGETS ttp_v2m RUNTIME DESTINATION AddIn)
