# Library
file(GLOB_RECURSE sources "*.cpp" "*.h")
list(REMOVE_ITEM sources
    ${CMAKE_CURRENT_SOURCE_DIR}/cbmc_main.cpp
)
add_library(cbmc-lib ${sources})

generic_includes(cbmc-lib)

target_link_libraries(cbmc-lib
    analyses
    ansi-c
    assembler
    big-int
    cpp
    goto-checker
    goto-instrument-lib
    goto-programs
    goto-symex
    json
    json-symtab-language
    langapi
    linking
    pointer-analysis
    solvers
    statement-list
    util
    xml
)

add_if_library(cbmc-lib bv_refinement)

# Executable
add_executable(cbmc cbmc_main.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
# There is a cyclic dependency between analyses and ansi-c, which the
# Makefile-based build system resolves by using --start-group, --end-group.
# CMake lacks direct support (cf.
# https://gitlab.kitware.com/cmake/cmake/-/issues/21511), so we ensure all
# object files from libanalyses.a remain present.
target_link_libraries(cbmc
  cbmc-lib
  -Wl,--whole-archive -Wl,${CMAKE_BINARY_DIR}/lib/libanalyses.a -Wl,--no-whole-archive
)
else()
target_link_libraries(cbmc cbmc-lib)
endif()
install(TARGETS cbmc DESTINATION ${CMAKE_INSTALL_BINDIR})

# Man page
if(NOT WIN32)
  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/doc/man/
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    FILES_MATCHING PATTERN "cbmc*")
endif()

# bash completion
if(NOT WIN32)
  find_program(BASH_PROGRAM bash REQUIRED)
  add_custom_command(OUTPUT "${CBMC_ROOT_DIR}/scripts/bash-autocomplete/cbmc.sh"
    COMMAND "${BASH_PROGRAM}" "${CBMC_ROOT_DIR}/scripts/bash-autocomplete/extract_switches.sh" "${CMAKE_CXX_COMPILER}"
    DEPENDS $<TARGET_FILE:cbmc>
  )
  add_custom_target(cbmc.sh ALL
    DEPENDS "${CBMC_ROOT_DIR}/scripts/bash-autocomplete/cbmc.sh"
  )
  install(
    FILES ${CBMC_ROOT_DIR}/scripts/bash-autocomplete/cbmc.sh
    DESTINATION etc/bash_completion.d
    RENAME cbmc
  )
endif()
