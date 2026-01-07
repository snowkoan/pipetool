cmake_minimum_required(VERSION 3.25)

project(pipetool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(MSVC)
    add_compile_options(
        /permissive-
        /Zc:__cplusplus
        /Zc:preprocessor
        /Zc:char8_t
        /EHsc
        /W4
        /WX
        /wd26812 # enum class warning noise
        /Zi
    )
    add_link_options(
        /DEBUG:FULL
    )
else()
    message(FATAL_ERROR "pipetool is intended to be built with MSVC on Windows.")
endif()

add_executable(pipetool
    src/main.cpp
    src/logging.cpp
    src/pipe_client.cpp
    src/file_sender.cpp
    src/random_sender.cpp
    src/pipe_info.cpp
)

target_include_directories(pipetool PRIVATE include)

target_compile_definitions(pipetool PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    set_property(TARGET pipetool PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Link against Windows security libraries for pipe metadata access.
target_link_libraries(pipetool PRIVATE
    advapi32
    secur32
)

# Require Windows 10 features.
set_property(TARGET pipetool PROPERTY
    WIN32_EXECUTABLE OFF
)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

set_property(TARGET pipetool PROPERTY VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0")
