cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED PORTABLE_VCPKG_ROOT OR NOT DEFINED PORTABLE_PLATFORM OR NOT DEFINED PORTABLE_OPENCONSOLE_ROOT)
    message(FATAL_ERROR "EnsureVcpkg.cmake requires PORTABLE_VCPKG_ROOT, PORTABLE_PLATFORM, and PORTABLE_OPENCONSOLE_ROOT.")
endif()

if(PORTABLE_PLATFORM STREQUAL "x64")
    set(_triplet "x64-windows-static")
elseif(PORTABLE_PLATFORM STREQUAL "x86")
    set(_triplet "x86-windows-static")
elseif(PORTABLE_PLATFORM STREQUAL "arm64")
    set(_triplet "arm64-windows-static")
else()
    message(FATAL_ERROR "Unsupported PORTABLE_PLATFORM: ${PORTABLE_PLATFORM}")
endif()

set(_vcpkg_exe "${PORTABLE_VCPKG_ROOT}/vcpkg.exe")
if(NOT EXISTS "${_vcpkg_exe}")
    set(_bootstrap_script "${PORTABLE_VCPKG_ROOT}/bootstrap-vcpkg.bat")
    if(NOT EXISTS "${_bootstrap_script}")
        message(FATAL_ERROR "Could not find vcpkg.exe or bootstrap-vcpkg.bat under ${PORTABLE_VCPKG_ROOT}.")
    endif()

    execute_process(
        COMMAND cmd /c "${_bootstrap_script}"
        RESULT_VARIABLE _bootstrap_result
        COMMAND_ECHO STDOUT
    )
    if(NOT _bootstrap_result EQUAL 0)
        message(FATAL_ERROR "vcpkg bootstrap failed with code ${_bootstrap_result}.")
    endif()
endif()

set(_install_root "${PORTABLE_OPENCONSOLE_ROOT}/obj/${PORTABLE_PLATFORM}/vcpkg")
set(_gsl_header "${_install_root}/${_triplet}/include/gsl/gsl_util")
if(EXISTS "${_gsl_header}")
    return()
endif()

execute_process(
    COMMAND "${_vcpkg_exe}" install
        --triplet "${_triplet}"
        "--x-manifest-root=${PORTABLE_OPENCONSOLE_ROOT}"
        "--x-install-root=${_install_root}"
        --x-feature=terminal
    RESULT_VARIABLE _install_result
    COMMAND_ECHO STDOUT
)
if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR "vcpkg install failed with code ${_install_result}.")
endif()

if(NOT EXISTS "${_gsl_header}")
    message(FATAL_ERROR "vcpkg install completed, but ${_gsl_header} is still missing.")
endif()
