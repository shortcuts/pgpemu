# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(COMPILE_OPTIONS
    "-Wall" "-Wextra" "-Wunused-parameter" "-Wunused-function" "-Wunused-variable"
    APPEND)
set_target_properties(${COMPONENT_LIB} PROPERTIES
    C_CLANG_TIDY
        "clang-tidy;
         -p=build;
         -header-filter=^${CMAKE_SOURCE_DIR}/main/;
         --extra-arg=-Wno-unknown-warning-option;
         --extra-arg=-Wno-unused-command-line-argument;"
)
project(pgpemu)
