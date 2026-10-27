cmake_minimum_required(VERSION 3.21)
project(BetterGPSProbe VERSION 0.3.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "BetterGPSProbe is a Windows x64 ATS plugin. Configure this project with Visual Studio on Windows.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "BetterGPSProbe must be built x64.")
endif()

add_library(BetterGPSProbe SHARED
    src/main.cpp
    src/probe.cpp
    src/logger.cpp
    src/memory.cpp
    src/live_capture.cpp
    BetterGPSProbe.def
)

target_compile_features(BetterGPSProbe PRIVATE cxx_std_17)
target_include_directories(BetterGPSProbe PRIVATE src third_party/scs_sdk/include)
target_compile_definitions(BetterGPSProbe PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(BetterGPSProbe PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus)
    set_property(TARGET BetterGPSProbe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_target_properties(BetterGPSProbe PROPERTIES
    OUTPUT_NAME "BetterGPSProbe"
    PREFIX ""
)

add_executable(CaptureTransportTest tests/capture_transport.cpp src/live_capture.cpp)
target_compile_features(CaptureTransportTest PRIVATE cxx_std_17)
target_include_directories(CaptureTransportTest PRIVATE src third_party/scs_sdk/include)
target_compile_definitions(CaptureTransportTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    target_compile_options(CaptureTransportTest PRIVATE /W4 /permissive- /EHsc)
    set_property(TARGET CaptureTransportTest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_executable(RouteIdentityTest tests/route_identity.cpp)
target_compile_features(RouteIdentityTest PRIVATE cxx_std_17)
target_include_directories(RouteIdentityTest PRIVATE src third_party/scs_sdk/include)
target_compile_definitions(RouteIdentityTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

add_executable(ProbeSamplingTest tests/probe_sampling.cpp src/probe.cpp src/logger.cpp src/memory.cpp src/live_capture.cpp)
target_compile_features(ProbeSamplingTest PRIVATE cxx_std_17)
target_include_directories(ProbeSamplingTest PRIVATE src third_party/scs_sdk/include)
target_compile_definitions(ProbeSamplingTest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
