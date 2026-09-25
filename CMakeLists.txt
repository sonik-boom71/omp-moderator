cmake_minimum_required(VERSION 3.19)
project(moderator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT, so the server machine doesn't need the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(deps/open.mp-sdk EXCLUDE_FROM_ALL)

if(MSVC)
    set(MODERATOR_WARNINGS /utf-8 /W4 /permissive- /external:anglebrackets /external:W0)
else()
    set(MODERATOR_WARNINGS -Wall -Wextra)
endif()

add_library(moderator_core STATIC src/cp1251.cpp src/filter.cpp)
target_include_directories(moderator_core PUBLIC src)
target_compile_options(moderator_core PRIVATE ${MODERATOR_WARNINGS})

add_library(Moderator SHARED src/component.cpp)
target_include_directories(Moderator PRIVATE deps)
target_link_libraries(Moderator PRIVATE moderator_core OMP-SDK)
# Otherwise amx.h declares its own int32_t on MSVC, clashing with <stdint.h>.
target_compile_definitions(Moderator PRIVATE HAVE_STDINT_H=1)
target_compile_options(Moderator PRIVATE ${MODERATOR_WARNINGS})
set_target_properties(Moderator PROPERTIES PREFIX "")

enable_testing()
add_executable(moderator_tests tests/filter_tests.cpp)
target_link_libraries(moderator_tests PRIVATE moderator_core)
target_compile_options(moderator_tests PRIVATE ${MODERATOR_WARNINGS})
add_test(NAME moderator_tests COMMAND moderator_tests)
