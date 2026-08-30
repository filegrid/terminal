if(NOT DEFINED INPUT_FILE OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE and OUTPUT_FILE are required.")
endif()

if(NOT EXISTS "${INPUT_FILE}")
    message(FATAL_ERROR "Version file not found: ${INPUT_FILE}")
endif()

file(READ "${INPUT_FILE}" _raw_version)
string(STRIP "${_raw_version}" _version)
if(_version STREQUAL "")
    message(FATAL_ERROR "Version file is empty: ${INPUT_FILE}")
endif()

get_filename_component(_output_dir "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_dir}")

set(_content "#pragma once\n\n#include <string_view>\n\nnamespace WorkspaceExt::Build\n{\n    inline constexpr std::wstring_view AboutVersion{ L\"${_version}\" };\n}\n")

set(_write_file TRUE)
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" _existing_content)
    if(_existing_content STREQUAL _content)
        set(_write_file FALSE)
    endif()
endif()

if(_write_file)
    file(WRITE "${OUTPUT_FILE}" "${_content}")
endif()
