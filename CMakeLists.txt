cmake_minimum_required(VERSION 3.19)
project(chatguard LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT, so the server machine doesn't need the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(deps/open.mp-sdk EXCLUDE_FROM_ALL)

if(MSVC)
    set(CHATGUARD_WARNINGS /utf-8 /W4 /permissive- /external:anglebrackets /external:W0)
else()
    set(CHATGUARD_WARNINGS -Wall -Wextra)
endif()

add_library(chatguard_core STATIC src/cp1251.cpp src/filter.cpp)
target_include_directories(chatguard_core PUBLIC src)
target_compile_options(chatguard_core PRIVATE ${CHATGUARD_WARNINGS})

add_library(ChatGuard SHARED src/component.cpp)
target_include_directories(ChatGuard PRIVATE deps)
target_link_libraries(ChatGuard PRIVATE chatguard_core OMP-SDK)
# Otherwise amx.h declares its own int32_t on MSVC, clashing with <stdint.h>.
target_compile_definitions(ChatGuard PRIVATE HAVE_STDINT_H=1)
target_compile_options(ChatGuard PRIVATE ${CHATGUARD_WARNINGS})
set_target_properties(ChatGuard PROPERTIES PREFIX "")

enable_testing()
add_executable(chatguard_tests tests/filter_tests.cpp)
target_link_libraries(chatguard_tests PRIVATE chatguard_core)
target_compile_options(chatguard_tests PRIVATE ${CHATGUARD_WARNINGS})
add_test(NAME chatguard_tests COMMAND chatguard_tests)
