cmake_minimum_required(VERSION 3.25)
project(TTPlayerV2M LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "TTPlayerV2M now requires the guide's MSVC Win32 toolchain")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "TTPlayer is 32-bit; use the x86 MSVC toolchain")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TTP_ENABLE_RONAN "Enable RONAN speech synthesis" ON)
enable_language(ASM_NASM)

set(TTP_V2M_ENGINE_SOURCES src/v2m/synth.asm)
if(TTP_ENABLE_RONAN)
  list(APPEND TTP_V2M_ENGINE_SOURCES src/v2m/ronan.cpp)
else()
  list(APPEND TTP_V2M_ENGINE_SOURCES src/v2m/synth_stub.cpp)
endif()
set_source_files_properties(src/v2m/synth.asm PROPERTIES LANGUAGE ASM_NASM)

add_library(ttp_v2m SHARED
  src/plugin.cpp
  src/reader.cpp
  src/v2m/v2init.cpp
  src/v2m/v2mplayer.cpp
  src/v2m/v2mconv.cpp
  ${TTP_V2M_ENGINE_SOURCES}
  src/ttp_v2m.def
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
if(TTP_ENABLE_RONAN)
  target_compile_definitions(ttp_v2m PRIVATE $<$<COMPILE_LANGUAGE:CXX>:RONAN=1>)
endif()

target_compile_options(ttp_v2m PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:/W3;/EHsc;/GR-;/O2;/fp:precise;/arch:IA32;/MT;/source-charset:.1252>
)
target_link_libraries(ttp_v2m PRIVATE ole32 uuid shlwapi)
target_link_options(ttp_v2m PRIVATE /OPT:REF /OPT:ICF /INCREMENTAL:NO /DEBUG:NONE)

set_target_properties(ttp_v2m PROPERTIES
  PREFIX "" OUTPUT_NAME "ttp_v2m" SUFFIX ".dll"
)

add_executable(ttp_v2m_test tests/test_host.cpp)
target_include_directories(ttp_v2m_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_options(ttp_v2m_test PRIVATE /W3 /EHsc /O2 /MT /source-charset:.1252)
target_compile_definitions(ttp_v2m_test PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(ttp_v2m_test PRIVATE ole32 uuid shlwapi shell32)
target_link_options(ttp_v2m_test PRIVATE /OPT:REF /OPT:ICF /INCREMENTAL:NO /DEBUG:NONE)

install(TARGETS ttp_v2m RUNTIME DESTINATION AddIn)
