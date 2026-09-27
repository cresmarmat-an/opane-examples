cmake_minimum_required(VERSION 3.22)

project(opane_examples
    DESCRIPTION "Example programs for opane"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The library, downloaded from GitHub at the release these examples are
# written for, along with its dependencies. To build against your own copy
# instead, configure with
#   -DFETCHCONTENT_SOURCE_DIR_OPANE=path/to/opane
include(FetchContent)
FetchContent_Declare(opane
    GIT_REPOSITORY https://github.com/cresmarmat-an/opane.git
    GIT_TAG v0.0.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(opane)

# One icon for every example that opens an opane window, drawn while they
# build rather than kept as an image in the repository: see common/MakeIcon.cpp.
add_executable(make_icon common/MakeIcon.cpp)
set(ExampleIcon ${CMAKE_BINARY_DIR}/example-icon.bmp)
add_custom_command(
    OUTPUT ${ExampleIcon}
    COMMAND make_icon ${ExampleIcon}
    DEPENDS make_icon
    COMMENT "Drawing the examples' icon"
    VERBATIM)

function(add_example name)
    # Every example can report where it crashed; see common/CrashReport.h.
    add_executable(${name} ${name}/main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/common/CrashReport.cpp)
    target_link_libraries(${name} PRIVATE ${ARGN})

    set(Libraries ${ARGN})
    if("opane::opane" IN_LIST Libraries)
        opane_set_app_icon(${name} ${ExampleIcon})
    endif()

    # Lets an example find its own shader files, and lets hot reload watch the
    # real source rather than a copy.
    target_compile_definitions(${name} PRIVATE
        EXAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/${name}")

    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

    # SDL3 builds as a shared library, so its DLL has to sit next to the
    # executable. An example that links no shared library produces an empty
    # list, and copy_if_different with only a destination is an error, so the
    # command degrades to a no-op in that case.
    if(WIN32)
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E
                    "$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:${name}>>,copy_if_different,true>"
                    "$<TARGET_RUNTIME_DLLS:${name}>" "$<TARGET_FILE_DIR:${name}>"
            COMMAND_EXPAND_LISTS
            VERBATIM)
    endif()
endfunction()

# Widgets, theming, a custom shader, a post-process chain, and a custom element.
add_example(01-interface opane::opane)

# Text: one distance-field atlas from 8 to 200 pixels, characters beyond
# ASCII, a text shader, editing, and a scroll view.
add_example(02-text opane::opane)

# An interface with its own look: a frameless window with its own title bar,
# frosted glass, gradients, shadows and glows, transforms, a nine-slice frame,
# and skinned built-in widgets, all from a theme file that can be edited while
# the program runs.
add_example(03-custom-ui opane::opane)

# Automated interface checks: simulated input through SDL's event queue into a
# hidden window, covering layout, focus, every control, popups, menus, and
# docking.
add_example(04-ui-checks opane::opane)
