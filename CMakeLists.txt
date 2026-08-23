cmake_minimum_required(VERSION 3.21)

set(PORTABLE_PLATFORM "x64" CACHE STRING "Portable build platform")
set_property(CACHE PORTABLE_PLATFORM PROPERTY STRINGS x64 x86 arm64)
set(PORTABLE_BRANDING "Dev" CACHE STRING "Windows Terminal branding")
set_property(CACHE PORTABLE_BRANDING PROPERTY STRINGS Dev Canary Preview Release)
set(PORTABLE_PGO_BUILD_MODE "None" CACHE STRING "PGO mode: None, Instrument, or Optimize")
set_property(CACHE PORTABLE_PGO_BUILD_MODE PROPERTY STRINGS None Instrument Optimize)
set(PORTABLE_PGO_DATABASE_ROOT "" CACHE PATH "Restored Microsoft.Internal.Windows.Terminal.PGODatabase package directory")
if(NOT PORTABLE_PGO_BUILD_MODE STREQUAL "None" AND NOT PORTABLE_PGO_BUILD_MODE STREQUAL "Instrument" AND NOT PORTABLE_PGO_BUILD_MODE STREQUAL "Optimize")
    message(FATAL_ERROR "PORTABLE_PGO_BUILD_MODE must be None, Instrument, or Optimize.")
endif()
string(TOUPPER "${PORTABLE_BRANDING}" _portable_branding_upper)
set(_portable_branding_definition "WT_BRANDING_${_portable_branding_upper}")
if(NOT _portable_branding_definition MATCHES "^WT_BRANDING_(DEV|CANARY|PREVIEW|RELEASE)$")
    message(FATAL_ERROR "PORTABLE_BRANDING must be one of Dev, Canary, Preview, or Release.")
endif()

if(CMAKE_GENERATOR MATCHES "Ninja")
    find_program(_portable_ninja_path NAMES ninja.exe REQUIRED)
    set(CMAKE_MAKE_PROGRAM "${_portable_ninja_path}" CACHE FILEPATH "Ninja executable" FORCE)
    set(_portable_rc_compiler "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/rc.exe")
    if(NOT EXISTS "${_portable_rc_compiler}")
        message(FATAL_ERROR "Could not locate the Windows SDK resource compiler at ${_portable_rc_compiler}.")
    endif()
    set(CMAKE_RC_COMPILER_INIT "${_portable_rc_compiler}")
    set(CMAKE_RC_COMPILER "${_portable_rc_compiler}" CACHE FILEPATH "Windows SDK resource compiler" FORCE)

    if(NOT DEFINED CMAKE_CXX_COMPILER)
        file(GLOB _portable_msvc_toolsets LIST_DIRECTORIES true "C:/Program Files/Microsoft Visual Studio/*/*/VC/Tools/MSVC/*")
        list(SORT _portable_msvc_toolsets COMPARE NATURAL ORDER DESCENDING)
        if(NOT _portable_msvc_toolsets)
            message(FATAL_ERROR "Could not locate an MSVC toolset under C:/Program Files/Microsoft Visual Studio.")
        endif()
        list(GET _portable_msvc_toolsets 0 _portable_msvc_toolset)
        set(_portable_cl_path "${_portable_msvc_toolset}/bin/Hostx64/${PORTABLE_PLATFORM}/cl.exe")
        if(NOT EXISTS "${_portable_cl_path}")
            message(FATAL_ERROR "Could not locate the MSVC ${PORTABLE_PLATFORM} compiler at ${_portable_cl_path}.")
        endif()
        set(CMAKE_CXX_COMPILER "${_portable_cl_path}" CACHE FILEPATH "MSVC C++ compiler" FORCE)
        set(CMAKE_C_COMPILER "${_portable_cl_path}" CACHE FILEPATH "MSVC C compiler" FORCE)
    endif()

    # Compiler identification must not require Developer Prompt LIB/INCLUDE variables.
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
endif()

project(TerminalPortableBuild LANGUAGES CXX RC)

add_compile_definitions("${_portable_branding_definition}")

enable_language(C)
unset(CMAKE_TRY_COMPILE_TARGET_TYPE)

if(CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
    set(CMAKE_CONFIGURATION_TYPES "Release;Debug;RelWithDebInfo;AuditMode;Fuzzing" CACHE STRING "Supported build configurations" FORCE)
    set(CMAKE_DEFAULT_BUILD_TYPE "Release" CACHE STRING "Default configuration for bare cmake --build" FORCE)
    set(CMAKE_DEFAULT_CONFIGS "Release" CACHE STRING "Default configurations for ALL target" FORCE)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "This CMake entrypoint only supports Windows portable builds.")
endif()

get_filename_component(_binary_dir_name "${CMAKE_BINARY_DIR}" NAME)
if(NOT _binary_dir_name STREQUAL "build")
    message(FATAL_ERROR "Configure this repository with -B build so the CMake build tree stays in the build directory.")
endif()

set(PORTABLE_DOTNET_SDK_VERSION "8.0.421" CACHE STRING "Pinned .NET SDK version")
set(PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION "10.0.22621.0" CACHE STRING "Windows SDK target version")
set(PORTABLE_MAKEPRI_PATH "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/MakePri.exe" CACHE FILEPATH "Path to MakePri.exe")

set(_workspace_root "${CMAKE_SOURCE_DIR}")
set(_openconsole_root "${CMAKE_SOURCE_DIR}/microsoft")
set(_portable_output_root "${_workspace_root}/bin")
set(_portable_package_root "${_portable_output_root}/msix")
set(_portable_terminal_package "${_portable_package_root}/CascadiaPackage_0.0.1.0_${PORTABLE_PLATFORM}.msix")
set(_portable_xaml_package "${_openconsole_root}/packages/Microsoft.UI.Xaml.2.8.4/tools/AppX/${PORTABLE_PLATFORM}/Release/Microsoft.UI.Xaml.2.8.appx")
set(_portable_xaml_pri "${_openconsole_root}/packages/Microsoft.UI.Xaml.2.8.4/lib/uap10.0/Microsoft.UI.Xaml.pri")
set(_ext_version_file "${_workspace_root}/VERSION")
set(_ext_generated_root "${_workspace_root}/src/generated")
set(_ext_generated_version_header "${_ext_generated_root}/ExtBuildVersion.h")
set(_ext_generate_version_script "${_workspace_root}/tools/cmake/GenerateExtBuildVersion.cmake")

if(NOT EXISTS "${_ext_version_file}")
    file(WRITE "${_ext_version_file}" "0.0.1\n")
endif()

file(TO_NATIVE_PATH "${_workspace_root}" _workspace_root_native)
file(TO_NATIVE_PATH "${_openconsole_root}" _openconsole_root_native)
file(TO_NATIVE_PATH "${_portable_output_root}" _portable_output_root_native)
file(TO_NATIVE_PATH "${_portable_package_root}" _portable_package_root_native)
file(TO_NATIVE_PATH "${_portable_terminal_package}" _portable_terminal_package_native)
file(TO_NATIVE_PATH "${_portable_xaml_package}" _portable_xaml_package_native)
file(TO_NATIVE_PATH "${_openconsole_root}/bin/${PORTABLE_PLATFORM}" _openconsole_bin_root_native)
string(REGEX REPLACE "[/\\\\]+$" "" _openconsole_bin_root_native_base "${_openconsole_bin_root_native}")
if(NOT EXISTS "${_portable_xaml_package}")
    message(FATAL_ERROR "Could not find the ${PORTABLE_PLATFORM} Microsoft.UI.Xaml package at ${_portable_xaml_package}.")
endif()
if(NOT EXISTS "${_portable_xaml_pri}")
    message(FATAL_ERROR "Could not find the Microsoft.UI.Xaml PRI at ${_portable_xaml_pri}.")
endif()
get_filename_component(_portable_compiler_target_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(_portable_compiler_host_dir "${_portable_compiler_target_dir}" DIRECTORY)
get_filename_component(_portable_compiler_bin_dir "${_portable_compiler_host_dir}" DIRECTORY)
get_filename_component(_portable_msvc_root "${_portable_compiler_bin_dir}" DIRECTORY)
get_filename_component(_portable_msvc_toolsets_root "${_portable_msvc_root}" DIRECTORY)
get_filename_component(_portable_tools_root "${_portable_msvc_toolsets_root}" DIRECTORY)
get_filename_component(_portable_vc_root "${_portable_tools_root}" DIRECTORY)
get_filename_component(_vs_install_root "${_portable_vc_root}" DIRECTORY)
set(_visual_studio_version "18.0")
set(_platform_toolset "v145")

set(_portable_msvc_include_dirs)
set(_portable_msvc_lib_dirs)
if(CMAKE_GENERATOR MATCHES "Ninja")
    set(_portable_sdk_root "C:/Program Files (x86)/Windows Kits/10")
    set(_portable_sdk_include_root "${_portable_sdk_root}/Include/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}")
    set(_portable_sdk_lib_root "${_portable_sdk_root}/Lib/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}")
    set(_portable_msvc_include_dirs
        "${_portable_msvc_root}/include"
        "${_portable_msvc_root}/atlmfc/include"
        "${_portable_sdk_include_root}/ucrt"
        "${_portable_sdk_include_root}/shared"
        "${_portable_sdk_include_root}/um"
        "${_portable_sdk_include_root}/winrt"
        "${_portable_sdk_include_root}/cppwinrt"
    )
    set(_portable_msvc_lib_dirs
        "${_portable_msvc_root}/lib/${PORTABLE_PLATFORM}"
        "${_portable_msvc_root}/atlmfc/lib/${PORTABLE_PLATFORM}"
        "${_portable_sdk_lib_root}/ucrt/${PORTABLE_PLATFORM}"
        "${_portable_sdk_lib_root}/um/${PORTABLE_PLATFORM}"
    )

    foreach(_portable_required_dir IN LISTS _portable_msvc_include_dirs _portable_msvc_lib_dirs)
        if(NOT IS_DIRECTORY "${_portable_required_dir}")
            message(FATAL_ERROR "Required MSVC/Windows SDK directory is missing: ${_portable_required_dir}")
        endif()
    endforeach()
endif()

set(_vcpkg_root "${_vs_install_root}/VC/vcpkg")
set(_target_framework_root_path "${_openconsole_root}/packages-ref/Microsoft.NETFramework.ReferenceAssemblies.net472.1.0.3/build/")
set(_net472_reference_path "${_target_framework_root_path}/.NETFramework/v4.7.2")
if(NOT IS_DIRECTORY "${_net472_reference_path}")
    message(FATAL_ERROR "Could not find .NET Framework 4.7.2 reference assemblies at ${_net472_reference_path}.")
endif()
file(TO_NATIVE_PATH "${_net472_reference_path}" _net472_reference_path_native)

find_program(_dotnet_path NAMES dotnet.exe REQUIRED)
if(NOT EXISTS "${PORTABLE_MAKEPRI_PATH}")
    message(FATAL_ERROR "Could not find MakePri.exe at ${PORTABLE_MAKEPRI_PATH}.")
endif()
get_filename_component(_portable_windows_sdk_bin_dir "${PORTABLE_MAKEPRI_PATH}" DIRECTORY)
set(_portable_makeappx_path "${_portable_windows_sdk_bin_dir}/MakeAppx.exe")
set(_portable_mt_path "${_portable_windows_sdk_bin_dir}/mt.exe")
set(_portable_midl_path "${_portable_windows_sdk_bin_dir}/midl.exe")
set(_portable_mdmerge_path "${_portable_windows_sdk_bin_dir}/mdmerge.exe")
set(_portable_fxc_path "${_portable_windows_sdk_bin_dir}/fxc.exe")
set(_portable_mc_path "${_portable_windows_sdk_bin_dir}/mc.exe")
if(PORTABLE_PLATFORM STREQUAL "x86")
    # MIDL calls 32-bit targets "win32", whereas the product layout uses x86.
    set(_portable_midl_environment win32)
    # COFF x86 exports stdcall symbols with their decorated spelling.
    set(_portable_dllmain_linker_symbol _DllMain@12)
else()
    set(_portable_midl_environment "${PORTABLE_PLATFORM}")
    set(_portable_dllmain_linker_symbol DllMain)
endif()
if(NOT EXISTS "${_portable_makeappx_path}")
    message(FATAL_ERROR "Could not find MakeAppx.exe at ${_portable_makeappx_path}.")
endif()
if(NOT EXISTS "${_portable_mt_path}")
    message(FATAL_ERROR "Could not find mt.exe at ${_portable_mt_path}.")
endif()
if(NOT EXISTS "${_portable_fxc_path}")
    message(FATAL_ERROR "Could not find fxc.exe at ${_portable_fxc_path}.")
endif()
if(NOT EXISTS "${_portable_midl_path}")
    message(FATAL_ERROR "Could not find midl.exe at ${_portable_midl_path}.")
endif()
if(NOT EXISTS "${_portable_mdmerge_path}")
    message(FATAL_ERROR "Could not find mdmerge.exe at ${_portable_mdmerge_path}.")
endif()
if(NOT EXISTS "${_portable_mc_path}")
    message(FATAL_ERROR "Could not find mc.exe at ${_portable_mc_path}.")
endif()

set(_dotnet_root "C:/Program Files/dotnet")
file(TO_NATIVE_PATH "${_dotnet_root}" _dotnet_root_native)
set(_dotnet_sdk_root "${_dotnet_root}/sdk/${PORTABLE_DOTNET_SDK_VERSION}")
set(_sdk_path "${_dotnet_sdk_root}/Sdks")
if(NOT EXISTS "${_sdk_path}")
    message(FATAL_ERROR "Could not find .NET SDK at ${_sdk_path}.")
endif()

set(_dotnet_csc_path "${_dotnet_sdk_root}/Roslyn/bincore/csc.dll")
set(_dotnet_bundled_versions_path "${_dotnet_sdk_root}/Microsoft.NETCoreSdk.BundledVersions.props")
if(NOT EXISTS "${_dotnet_csc_path}" OR NOT EXISTS "${_dotnet_bundled_versions_path}")
    message(FATAL_ERROR "The pinned .NET SDK ${PORTABLE_DOTNET_SDK_VERSION} is missing Roslyn or its bundled-version metadata.")
endif()
file(READ "${_dotnet_bundled_versions_path}" _dotnet_bundled_versions)
string(REGEX MATCH "<KnownFrameworkReference Include=\"Microsoft.NETCore.App\"[^>]*TargetFramework=\"net8.0\"[^>]*TargetingPackVersion=\"([0-9.]+)\"" _dotnet_net8_framework "${_dotnet_bundled_versions}")
if(NOT CMAKE_MATCH_1)
    message(FATAL_ERROR "Could not resolve the net8.0 targeting pack from ${_dotnet_bundled_versions_path}.")
endif()
set(_dotnet_net8_ref_dir "${_dotnet_root}/packs/Microsoft.NETCore.App.Ref/${CMAKE_MATCH_1}/ref/net8.0")
set(_dotnet_windowsdesktop_ref_dir "${_dotnet_root}/packs/Microsoft.WindowsDesktop.App.Ref/${CMAKE_MATCH_1}/ref/net8.0")
file(GLOB _dotnet_net8_references CONFIGURE_DEPENDS "${_dotnet_net8_ref_dir}/*.dll")
if(NOT _dotnet_net8_references)
    message(FATAL_ERROR "Could not find net8.0 reference assemblies under ${_dotnet_net8_ref_dir}.")
endif()
file(GLOB _dotnet_windowsdesktop_references CONFIGURE_DEPENDS "${_dotnet_windowsdesktop_ref_dir}/*.dll")
if(NOT _dotnet_windowsdesktop_references)
    message(FATAL_ERROR "Could not find net8.0 Windows Desktop reference assemblies under ${_dotnet_windowsdesktop_ref_dir}.")
endif()
file(GLOB _net472_references CONFIGURE_DEPENDS "${_net472_reference_path}/*.dll")
list(FILTER _net472_references EXCLUDE REGEX "System\\.EnterpriseServices\\.(Wrapper|Thunk)\\.dll$")
if(NOT _net472_references)
    message(FATAL_ERROR "Could not find .NET Framework 4.7.2 reference assemblies under ${_net472_reference_path}.")
endif()

file(TO_NATIVE_PATH "${_vcpkg_root}" _vcpkg_root_native)
file(TO_NATIVE_PATH "${_dotnet_csc_path}" _dotnet_csc_path_native)
file(TO_NATIVE_PATH "${PORTABLE_MAKEPRI_PATH}" _makepri_path_native)

file(GLOB _wil_package_dirs LIST_DIRECTORIES true "${_openconsole_root}/packages/Microsoft.Windows.ImplementationLibrary.*")
list(SORT _wil_package_dirs COMPARE NATURAL ORDER DESCENDING)
list(GET _wil_package_dirs 0 _wil_package_dir)
if(NOT _wil_package_dir)
    message(FATAL_ERROR "Could not locate a Microsoft.Windows.ImplementationLibrary package under ${_openconsole_root}/packages.")
endif()

set(_native_product_output_root "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>")
set(_native_product_compile_options
    /W4
    /WX
    /Zi
    /sdl
    /Oi
    /GS
    /guard:cf
    /Gy
    /fp:precise
    /Zc:wchar_t
    /Zc:forScope
    /EHsc
    /GR-
    /permissive-
    /utf-8
    /Zc:__cplusplus
    /Zc:__STDC__
    /Zc:enumTypes
    /Zc:inline
    /Zc:templateScope
    /Zc:throwingNew
    /diagnostics:caret
    /wd4201
    /wd4312
    /wd5105
    /wd26434
    /wd26445
    /wd26456
    /wd26478
    /wd26494
    /we4189
    /we4100
    /we4242
    /we4389
    /we4244
    /FC
)
set(_native_product_archive_compile_options ${_native_product_compile_options})
list(REMOVE_ITEM _native_product_archive_compile_options /Zi)
list(APPEND _native_product_archive_compile_options /Z7)

# Azure's named configurations are build contracts, not solution aliases.
# Keep them in the Ninja graph so their output layouts remain bin/<arch>/<mode>.
# The portable Debug configuration intentionally keeps the release CRT ABI
# (see the target-wide MSVC_RUNTIME_LIBRARY override below). CMake's default
# Debug flags add /RTC1, which instruments stack and CRT checks under a
# different runtime contract and causes the unpackaged WindowsTerminal host to
# terminate with STATUS_STACK_BUFFER_OVERRUN. Preserve debug symbols and the
# DBG source paths, but use the product optimization contract instead.
set(CMAKE_C_FLAGS_DEBUG "/Zi /O2 /Ob2 /DNDEBUG" CACHE STRING "Portable Debug C flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "/Zi /O2 /Ob2 /DNDEBUG" CACHE STRING "Portable Debug C++ flags" FORCE)

# The product link contract specifies /DEBUGTYPE:CV,FIXUP. MSVC's Debug
# defaults add /INCREMENTAL, which is incompatible with that format on recent
# toolsets and fails before the product graph can link. Keep Debug non-
# incremental just like the other portable configurations.
foreach(_portable_debug_linker_flags IN ITEMS
    CMAKE_EXE_LINKER_FLAGS_DEBUG
    CMAKE_SHARED_LINKER_FLAGS_DEBUG
    CMAKE_MODULE_LINKER_FLAGS_DEBUG)
    string(APPEND ${_portable_debug_linker_flags} " /INCREMENTAL:NO")
endforeach()
add_compile_options("$<$<CONFIG:AuditMode>:/analyze>")
add_compile_definitions("$<$<CONFIG:Fuzzing>:FUZZING_BUILD>")
add_compile_definitions("$<$<CONFIG:Debug>:DBG>")

function(add_native_console_tool target_name output_name)
    add_executable(${target_name} ${ARGN})
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_include_directories(${target_name} PRIVATE
        "${_wil_package_dir}/include"
        ${_portable_msvc_include_dirs}
    )
    target_link_directories(${target_name} PRIVATE ${_portable_msvc_lib_dirs})
    target_compile_definitions(${target_name} PRIVATE
        _CONSOLE
        _UNICODE
        UNICODE
        EXTERNAL_BUILD
    )
    target_compile_options(${target_name} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${_native_product_compile_options};/EHsc>"
    )
    target_link_libraries(${target_name} PRIVATE onecore.lib)
    target_link_options(${target_name} PRIVATE
        /SUBSYSTEM:CONSOLE
        /guard:cf
        /DEBUG:FULL
        /OPT:REF
        /OPT:ICF
        /RELEASE
        /DYNAMICBASE
        /NXCOMPAT
    )
    set_target_properties(${target_name} PROPERTIES
        OUTPUT_NAME "${output_name}"
        RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
        PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
    )
endfunction()

add_native_console_tool(ToolBufferSize buffersize
    "${_openconsole_root}/src/tools/buffersize/main.cpp")
add_native_console_tool(ToolCloseTest CloseTest
    "${_openconsole_root}/src/tools/closetest/closetest.cpp")
add_native_console_tool(ToolConEchoKey ConEchoKey
    "${_openconsole_root}/src/tools/echokey/main.cpp")
add_native_console_tool(ToolFontList FontList
    "${_openconsole_root}/src/tools/fontlist/main.cpp")
add_native_console_tool(ToolNihilist Nihilist
    "${_openconsole_root}/src/tools/nihilist/main.cpp")
add_native_console_tool(ToolScratch Scratch
    "${_openconsole_root}/src/tools/scratch/main.cpp")
add_native_console_tool(ToolRenderingTests RenderingTests
    "${_openconsole_root}/src/tools/RenderingTests/main.cpp")
add_native_console_tool(ToolU8U16Test U8U16Test
    "${_openconsole_root}/src/tools/U8U16Test/main.cpp"
    "${_openconsole_root}/src/tools/U8U16Test/U8U16Test.cpp")
add_native_console_tool(ToolConsoleMonitor ConsoleMonitor
    "${_openconsole_root}/src/tools/ConsoleMonitor/main.cpp"
    "${_openconsole_root}/src/tools/ConsoleMonitor/pch.cpp")
target_precompile_headers(ToolConsoleMonitor PRIVATE
    "${_openconsole_root}/src/tools/ConsoleMonitor/pch.h")
target_link_options(ToolConsoleMonitor PRIVATE
    /SUBSYSTEM:WINDOWS
    /MANIFEST:EMBED
    "/MANIFESTINPUT:${_openconsole_root}/src/tools/ConsoleMonitor/ConsoleMonitor.exe.manifest"
)
add_native_console_tool(ToolVtPipeTerm VtPipeTerm
    "${_openconsole_root}/src/tools/vtpipeterm/main.cpp")
target_include_directories(ToolVtPipeTerm PRIVATE
    "${_openconsole_root}/src/inc")
target_link_libraries(ToolVtPipeTerm PRIVATE
    ConTypes
    conptylib
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    ntdll.lib
)
set_target_properties(ToolVtPipeTerm PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

set(_echo_con_root "${_openconsole_root}/samples/ConPTY/EchoCon/EchoCon")
add_native_console_tool(SampleEchoCon EchoCon
    "${_echo_con_root}/EchoCon.cpp"
    "${_echo_con_root}/stdafx.cpp"
)
target_precompile_headers(SampleEchoCon PRIVATE "${_echo_con_root}/stdafx.h")
target_compile_options(SampleEchoCon PRIVATE /wd4189)

set(_console_bench_root "${_openconsole_root}/src/tools/ConsoleBench")
add_native_console_tool(ToolConsoleBench ConsoleBench
    "${_console_bench_root}/arena.cpp"
    "${_console_bench_root}/conhost.cpp"
    "${_console_bench_root}/main.cpp"
    "${_console_bench_root}/pch.cpp"
    "${_console_bench_root}/utils.cpp"
)
target_precompile_headers(ToolConsoleBench PRIVATE "${_console_bench_root}/pch.h")
target_include_directories(ToolConsoleBench PRIVATE "${_openconsole_root}/dep/Console")
target_link_libraries(ToolConsoleBench PRIVATE ntdll.lib version.lib)
target_link_options(ToolConsoleBench PRIVATE
    /GUARD:NO
    /MANIFEST:EMBED
    "/MANIFESTINPUT:${_console_bench_root}/ConsoleBench.exe.manifest"
)

set(_parser_fuzzer_root "${_openconsole_root}/src/terminal/parser/ft_fuzzer")
add_native_console_tool(TerminalParserFuzzer VTCommandFuzzer
    "${_parser_fuzzer_root}/stdafx.cpp"
    "${_parser_fuzzer_root}/VTCommandFuzzer.cpp"
)
target_precompile_headers(TerminalParserFuzzer PRIVATE "${_parser_fuzzer_root}/stdafx.h")
target_include_directories(TerminalParserFuzzer PRIVATE
    "${_openconsole_root}/packages/Microsoft.Taef.10.100.251104001/build/Include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
)
target_compile_definitions(TerminalParserFuzzer PRIVATE INLINE_TEST_METHOD_MARKUP UNIT_TESTING)
target_link_directories(TerminalParserFuzzer PRIVATE
    "${_openconsole_root}/packages/Microsoft.Taef.10.100.251104001/build/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(TerminalParserFuzzer PRIVATE
    shlwapi.lib Wex.Logger.lib Wex.Common.lib TE.Common.lib
)
set_target_properties(TerminalParserFuzzer PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

set(_parser_fuzz_wrapper_root "${_openconsole_root}/src/terminal/parser/ft_fuzzwrapper")
add_native_console_tool(TerminalParserFuzzWrapper ConTerm.Parser.FuzzWrapper
    "${_parser_fuzz_wrapper_root}/precomp.cpp"
    "${_parser_fuzz_wrapper_root}/main.cpp"
    "${_parser_fuzz_wrapper_root}/echoDispatch.cpp"
)
target_precompile_headers(TerminalParserFuzzWrapper PRIVATE
    "${_parser_fuzz_wrapper_root}/precomp.h"
)
target_include_directories(TerminalParserFuzzWrapper PRIVATE
    "${_openconsole_root}/packages/Microsoft.Taef.10.100.251104001/build/Include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
)
target_compile_definitions(TerminalParserFuzzWrapper PRIVATE INLINE_TEST_METHOD_MARKUP UNIT_TESTING)
target_link_directories(TerminalParserFuzzWrapper PRIVATE
    "${_openconsole_root}/packages/Microsoft.Taef.10.100.251104001/build/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(TerminalParserFuzzWrapper PRIVATE
    ConTypes
    ConTermParser
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    ntdll.lib
    Wex.Logger.lib
    Wex.Common.lib
    TE.Common.lib
)
set_target_properties(TerminalParserFuzzWrapper PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

add_executable(ToolBenchcat
    "${_openconsole_root}/src/tools/benchcat/main.cpp")
target_compile_features(ToolBenchcat PRIVATE cxx_std_20)
target_include_directories(ToolBenchcat PRIVATE ${_portable_msvc_include_dirs})
target_link_directories(ToolBenchcat PRIVATE ${_portable_msvc_lib_dirs})
target_compile_definitions(ToolBenchcat PRIVATE
    NODEFAULTLIB
    _UNICODE
    UNICODE
)
target_compile_options(ToolBenchcat PRIVATE
    /W4
    /WX
    /O2
    /Oi
    /Zl
    /GS-
    /sdl-
    /guard:cf-
    /EHs-c-
    /GR-
    /utf-8
)
target_link_libraries(ToolBenchcat PRIVATE onecore.lib)
target_link_options(ToolBenchcat PRIVATE
    /NODEFAULTLIB
    /ENTRY:main
    /SUBSYSTEM:CONSOLE
    /OPT:REF
    /OPT:ICF
    /RELEASE
    /DYNAMICBASE
    /NXCOMPAT
)
set_target_properties(ToolBenchcat PROPERTIES
    OUTPUT_NAME bc
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

function(add_native_product_shim target_name output_name source_file resource_file)
    add_executable(${target_name} WIN32 EXCLUDE_FROM_ALL
        "${source_file}"
        "${resource_file}"
    )
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_include_directories(${target_name} PRIVATE
        "${_wil_package_dir}/include"
        ${_portable_msvc_include_dirs}
    )
    target_link_directories(${target_name} PRIVATE ${_portable_msvc_lib_dirs})
    target_compile_definitions(${target_name} PRIVATE
        _UNICODE
        UNICODE
        WT_BRANDING_DEV
        _WINDOWS
        EXTERNAL_BUILD
        _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    )
    target_compile_options(${target_name} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${_native_product_compile_options}>"
    )
    target_link_libraries(${target_name} PRIVATE onecore.lib)
    target_link_options(${target_name} PRIVATE
        /guard:cf
        /NODEFAULTLIB:LIBUCRT.LIB
        /DEFAULTLIB:ucrt.lib
        /MANIFEST:EMBED
        "/MANIFESTUAC:level='asInvoker' uiAccess='false'"
        /DEBUG:FULL
        /OPT:REF
        /OPT:ICF
        /RELEASE
        /DYNAMICBASE
        /NXCOMPAT
        /DEBUGTYPE:CV,FIXUP
    )
    set_target_properties(${target_name} PROPERTIES
        OUTPUT_NAME "${output_name}"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
        RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
        PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
    )
endfunction()

add_native_product_shim(terminal-wt wt
    "${_openconsole_root}/src/cascadia/wt/shim.cpp"
    "${_openconsole_root}/src/cascadia/wt/wt.rc"
)
add_native_product_shim(terminal-elevate-shim elevate-shim
    "${_openconsole_root}/src/cascadia/ElevateShim/elevate-shim.cpp"
    "${_openconsole_root}/src/cascadia/ElevateShim/elevate-shim.rc"
)

set(_midi_audio_target MidiAudio)
set(_midi_audio_source_root "${_openconsole_root}/src/audio/midi")
add_library(${_midi_audio_target} STATIC EXCLUDE_FROM_ALL
    "${_midi_audio_source_root}/MidiAudio.cpp"
)
target_compile_features(${_midi_audio_target} PRIVATE cxx_std_20)
target_precompile_headers(${_midi_audio_target} PRIVATE "${_midi_audio_source_root}/precomp.h")
target_include_directories(${_midi_audio_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_midi_audio_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_midi_audio_target} PRIVATE
    ${_native_product_archive_compile_options}
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_midi_audio_target} PROPERTIES
    OUTPUT_NAME "MidiAudio"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_renderer_uia_target ConRenderUia)
set(_renderer_uia_source_root "${_openconsole_root}/src/renderer/uia")
add_library(${_renderer_uia_target} STATIC EXCLUDE_FROM_ALL
    "${_renderer_uia_source_root}/UiaRenderer.cpp"
)
target_compile_features(${_renderer_uia_target} PRIVATE cxx_std_20)
target_precompile_headers(${_renderer_uia_target} PRIVATE "${_renderer_uia_source_root}/precomp.h")
target_include_directories(${_renderer_uia_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_renderer_uia_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_renderer_uia_target} PRIVATE
    ${_native_product_archive_compile_options}
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_renderer_uia_target} PROPERTIES
    OUTPUT_NAME "ConRenderUia"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_internal_target ConInt)
set(_internal_source_root "${_openconsole_root}/src/internal")
add_library(${_internal_target} STATIC EXCLUDE_FROM_ALL
    "${_internal_source_root}/stubs.cpp"
)
target_compile_features(${_internal_target} PRIVATE cxx_std_20)
target_precompile_headers(${_internal_target} PRIVATE "${_internal_source_root}/precomp.h")
target_include_directories(${_internal_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_internal_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_internal_target} PRIVATE
    ${_native_product_archive_compile_options}
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_internal_target} PROPERTIES
    OUTPUT_NAME "ConInt"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_tsf_target ConTSF)
set(_tsf_source_root "${_openconsole_root}/src/tsf")
add_library(${_tsf_target} STATIC EXCLUDE_FROM_ALL
    "${_tsf_source_root}/Handle.cpp"
    "${_tsf_source_root}/Implementation.cpp"
)
target_compile_features(${_tsf_target} PRIVATE cxx_std_20)
target_precompile_headers(${_tsf_target} PRIVATE "${_tsf_source_root}/precomp.h")
target_include_directories(${_tsf_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_tsf_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_tsf_target} PRIVATE
    ${_native_product_archive_compile_options}
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_tsf_target} PROPERTIES
    OUTPUT_NAME "ConTSF"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_renderer_gdi_target ConRenderGdi)
set(_renderer_gdi_source_root "${_openconsole_root}/src/renderer/gdi")
add_library(${_renderer_gdi_target} STATIC EXCLUDE_FROM_ALL
    "${_renderer_gdi_source_root}/invalidate.cpp"
    "${_renderer_gdi_source_root}/math.cpp"
    "${_renderer_gdi_source_root}/paint.cpp"
    "${_renderer_gdi_source_root}/state.cpp"
)
target_compile_features(${_renderer_gdi_target} PRIVATE cxx_std_20)
target_precompile_headers(${_renderer_gdi_target} PRIVATE "${_renderer_gdi_source_root}/precomp.h")
target_include_directories(${_renderer_gdi_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_renderer_gdi_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_renderer_gdi_target} PRIVATE
    ${_native_product_archive_compile_options}
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_renderer_gdi_target} PROPERTIES
    OUTPUT_NAME "ConRenderGdi"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    STATIC_LIBRARY_OPTIONS "${_portable_sdk_lib_root}/um/${PORTABLE_PLATFORM}/usp10.lib"
)

set(_interactivity_base_target ConInteractivityBaseLib)
set(_interactivity_base_source_root "${_openconsole_root}/src/interactivity/base")
add_library(${_interactivity_base_target} STATIC EXCLUDE_FROM_ALL
    "${_interactivity_base_source_root}/ApiDetector.cpp"
    "${_interactivity_base_source_root}/EventSynthesis.cpp"
    "${_interactivity_base_source_root}/HostSignalInputThread.cpp"
    "${_interactivity_base_source_root}/InteractivityFactory.cpp"
    "${_interactivity_base_source_root}/PseudoConsoleWindowAccessibilityProvider.cpp"
    "${_interactivity_base_source_root}/RemoteConsoleControl.cpp"
    "${_interactivity_base_source_root}/ServiceLocator.cpp"
    "${_interactivity_base_source_root}/VtApiRedirection.cpp"
)
target_compile_features(${_interactivity_base_target} PRIVATE cxx_std_20)
target_precompile_headers(${_interactivity_base_target} PRIVATE "${_interactivity_base_source_root}/precomp.h")
target_include_directories(${_interactivity_base_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/interactivity/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_interactivity_base_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_interactivity_base_target} PRIVATE
    ${_native_product_archive_compile_options}
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_interactivity_base_target} PROPERTIES
    OUTPUT_NAME "ConInteractivityBaseLib"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_interactivity_win32_target ConInteractivityWin32Lib)
set(_interactivity_win32_source_root "${_openconsole_root}/src/interactivity/win32")
add_library(${_interactivity_win32_target} STATIC EXCLUDE_FROM_ALL
    "${_interactivity_win32_source_root}/Clipboard.cpp"
    "${_interactivity_win32_source_root}/ConsoleControl.cpp"
    "${_interactivity_win32_source_root}/ConsoleInputThread.cpp"
    "${_interactivity_win32_source_root}/ConsoleKeyInfo.cpp"
    "${_interactivity_win32_source_root}/Find.cpp"
    "${_interactivity_win32_source_root}/Icon.cpp"
    "${_interactivity_win32_source_root}/Menu.cpp"
    "${_interactivity_win32_source_root}/screenInfoUiaProvider.cpp"
    "${_interactivity_win32_source_root}/SystemConfigurationProvider.cpp"
    "${_interactivity_win32_source_root}/uiaTextRange.cpp"
    "${_interactivity_win32_source_root}/Window.cpp"
    "${_interactivity_win32_source_root}/WindowDpiApi.cpp"
    "${_interactivity_win32_source_root}/WindowIo.cpp"
    "${_interactivity_win32_source_root}/WindowMetrics.cpp"
    "${_interactivity_win32_source_root}/WindowProc.cpp"
    "${_interactivity_win32_source_root}/windowUiaProvider.cpp"
)
target_compile_features(${_interactivity_win32_target} PRIVATE cxx_std_20)
target_precompile_headers(${_interactivity_win32_target} PRIVATE "${_interactivity_win32_source_root}/precomp.h")
target_include_directories(${_interactivity_win32_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/interactivity/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_interactivity_win32_target} PRIVATE
    _LIB _WINDLL _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_interactivity_win32_target} PRIVATE
    ${_native_product_archive_compile_options}
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_interactivity_win32_target} PROPERTIES
    OUTPUT_NAME "ConInteractivityWin32Lib"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_renderer_base_target ConRenderBase)
set(_renderer_base_source_root "${_openconsole_root}/src/renderer/base")
add_library(${_renderer_base_target} STATIC EXCLUDE_FROM_ALL
    "${_renderer_base_source_root}/CSSLengthPercentage.cpp"
    "${_renderer_base_source_root}/FontInfo.cpp"
    "${_renderer_base_source_root}/FontInfoBase.cpp"
    "${_renderer_base_source_root}/FontInfoDesired.cpp"
    "${_renderer_base_source_root}/FontResource.cpp"
    "${_renderer_base_source_root}/RenderEngineBase.cpp"
    "${_renderer_base_source_root}/RenderSettings.cpp"
    "${_renderer_base_source_root}/renderer.cpp"
)
target_compile_features(${_renderer_base_target} PRIVATE cxx_std_20)
target_precompile_headers(${_renderer_base_target} PRIVATE "${_renderer_base_source_root}/precomp.h")
target_include_directories(${_renderer_base_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/renderer/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_renderer_base_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_renderer_base_target} PRIVATE
    ${_native_product_archive_compile_options}
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_renderer_base_target} PROPERTIES
    OUTPUT_NAME "ConRenderBase"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_terminal_input_target TerminalInput)
set(_terminal_input_source_root "${_openconsole_root}/src/terminal/input")
add_library(${_terminal_input_target} STATIC EXCLUDE_FROM_ALL
    "${_terminal_input_source_root}/mouseInput.cpp"
    "${_terminal_input_source_root}/terminalInput.cpp"
)
target_compile_features(${_terminal_input_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_input_target} PRIVATE "${_terminal_input_source_root}/precomp.h")
target_include_directories(${_terminal_input_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_terminal_input_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_terminal_input_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_terminal_input_target} PROPERTIES
    OUTPUT_NAME "TerminalInput"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_terminal_adapter_target ConTermAdapt)
set(_terminal_adapter_source_root "${_openconsole_root}/src/terminal/adapter")
add_library(${_terminal_adapter_target} STATIC EXCLUDE_FROM_ALL
    "${_terminal_adapter_source_root}/adaptDispatch.cpp"
    "${_terminal_adapter_source_root}/FontBuffer.cpp"
    "${_terminal_adapter_source_root}/InteractDispatch.cpp"
    "${_terminal_adapter_source_root}/MacroBuffer.cpp"
    "${_terminal_adapter_source_root}/PageManager.cpp"
    "${_terminal_adapter_source_root}/SixelParser.cpp"
    "${_terminal_adapter_source_root}/adaptDispatchGraphics.cpp"
    "${_terminal_adapter_source_root}/terminalOutput.cpp"
)
target_compile_features(${_terminal_adapter_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_adapter_target} PRIVATE "${_terminal_adapter_source_root}/precomp.h")
target_include_directories(${_terminal_adapter_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_terminal_adapter_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_terminal_adapter_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_terminal_adapter_target} PROPERTIES
    OUTPUT_NAME "ConTermAdapt"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_terminal_parser_target ConTermParser)
set(_terminal_parser_source_root "${_openconsole_root}/src/terminal/parser")
add_library(${_terminal_parser_target} STATIC EXCLUDE_FROM_ALL
    "${_terminal_parser_source_root}/InputStateMachineEngine.cpp"
    "${_terminal_parser_source_root}/OutputStateMachineEngine.cpp"
    "${_terminal_parser_source_root}/stateMachine.cpp"
    "${_terminal_parser_source_root}/tracing.cpp"
    "${_terminal_parser_source_root}/base64.cpp"
)
target_compile_features(${_terminal_parser_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_parser_target} PRIVATE "${_terminal_parser_source_root}/precomp.h")
target_include_directories(${_terminal_parser_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_terminal_parser_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_terminal_parser_target} PRIVATE
    ${_native_product_archive_compile_options}
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_terminal_parser_target} PROPERTIES
    OUTPUT_NAME "ConTermParser"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_props_library_target ConProps)
set(_props_library_source_root "${_openconsole_root}/src/propslib")
add_library(${_props_library_target} STATIC EXCLUDE_FROM_ALL
    "${_props_library_source_root}/DelegationConfig.cpp"
    "${_props_library_source_root}/RegistrySerialization.cpp"
    "${_props_library_source_root}/ShortcutSerialization.cpp"
    "${_props_library_source_root}/TrueTypeFontList.cpp"
)
target_compile_features(${_props_library_target} PRIVATE cxx_std_20)
target_precompile_headers(${_props_library_target} PRIVATE "${_props_library_source_root}/precomp.h")
target_include_directories(${_props_library_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_props_library_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_props_library_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_props_library_target} PROPERTIES
    OUTPUT_NAME "ConProps"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_buffer_output_target ConBufferOut)
set(_buffer_output_source_root "${_openconsole_root}/src/buffer/out")
add_library(${_buffer_output_target} STATIC EXCLUDE_FROM_ALL
    "${_buffer_output_source_root}/cursor.cpp"
    "${_buffer_output_source_root}/ImageSlice.cpp"
    "${_buffer_output_source_root}/OutputCell.cpp"
    "${_buffer_output_source_root}/OutputCellIterator.cpp"
    "${_buffer_output_source_root}/OutputCellRect.cpp"
    "${_buffer_output_source_root}/OutputCellView.cpp"
    "${_buffer_output_source_root}/Row.cpp"
    "${_buffer_output_source_root}/search.cpp"
    "${_buffer_output_source_root}/TextColor.cpp"
    "${_buffer_output_source_root}/TextAttribute.cpp"
    "${_buffer_output_source_root}/textBuffer.cpp"
    "${_buffer_output_source_root}/textBufferCellIterator.cpp"
    "${_buffer_output_source_root}/textBufferTextIterator.cpp"
    "${_buffer_output_source_root}/UTextAdapter.cpp"
)
target_compile_features(${_buffer_output_target} PRIVATE cxx_std_20)
target_precompile_headers(${_buffer_output_target} PRIVATE "${_buffer_output_source_root}/precomp.h")
target_include_directories(${_buffer_output_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_buffer_output_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_buffer_output_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_buffer_output_target} PROPERTIES
    OUTPUT_NAME "ConBufferOut"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_openconsole_proxy_target OpenConsoleProxy)
set(_openconsole_proxy_source_root "${_openconsole_root}/src/host/proxy")
set(_openconsole_proxy_generated_root "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/$<CONFIG>/OpenConsoleProxy")
set(_openconsole_proxy_midl_stamp "${CMAKE_BINARY_DIR}/openconsole-proxy-midl-$<CONFIG>.stamp")
set(_openconsole_proxy_generated_sources
    "${_openconsole_proxy_generated_root}/dlldata.c"
    "${_openconsole_proxy_generated_root}/IConsoleHandoff_i.c"
    "${_openconsole_proxy_generated_root}/IConsoleHandoff_p.c"
    "${_openconsole_proxy_generated_root}/ITerminalHandoff_i.c"
    "${_openconsole_proxy_generated_root}/ITerminalHandoff_p.c"
)
add_custom_command(
    OUTPUT "${_openconsole_proxy_midl_stamp}"
    BYPRODUCTS
        ${_openconsole_proxy_generated_sources}
        "${_openconsole_proxy_generated_root}/IConsoleHandoff.h"
        "${_openconsole_proxy_generated_root}/ITerminalHandoff.h"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_openconsole_proxy_generated_root}"
    COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
        "${_portable_midl_path}" /W1 /nologo /char signed /env ${_portable_midl_environment}
        /I "${_portable_sdk_include_root}/um" /I "${_portable_sdk_include_root}/shared"
        /out "${_openconsole_proxy_generated_root}" /h IConsoleHandoff.h
        /tlb "${_openconsole_proxy_generated_root}/OpenConsoleProxy.tlb" /target NT100
        "${_openconsole_proxy_source_root}/IConsoleHandoff.idl"
    COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
        "${_portable_midl_path}" /W1 /nologo /char signed /env ${_portable_midl_environment}
        /I "${_portable_sdk_include_root}/um" /I "${_portable_sdk_include_root}/shared"
        /out "${_openconsole_proxy_generated_root}" /h ITerminalHandoff.h
        /tlb "${_openconsole_proxy_generated_root}/OpenConsoleProxy.tlb" /target NT100
        "${_openconsole_proxy_source_root}/ITerminalHandoff.idl"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_openconsole_proxy_midl_stamp}"
    DEPENDS
        "${_openconsole_proxy_source_root}/IConsoleHandoff.idl"
        "${_openconsole_proxy_source_root}/ITerminalHandoff.idl"
    VERBATIM
    COMMENT "Generating OpenConsole proxy/stub sources with MIDL"
)
set_source_files_properties(${_openconsole_proxy_generated_sources} PROPERTIES
    GENERATED TRUE
)
add_library(${_openconsole_proxy_target} SHARED EXCLUDE_FROM_ALL
    ${_openconsole_proxy_generated_sources}
    "${_openconsole_proxy_midl_stamp}"
)
target_include_directories(${_openconsole_proxy_target} PRIVATE
    "${_openconsole_proxy_source_root}/.."
    ${_portable_msvc_include_dirs}
)
target_link_directories(${_openconsole_proxy_target} PRIVATE ${_portable_msvc_lib_dirs})
target_compile_definitions(${_openconsole_proxy_target} PRIVATE
    _USRDLL _WINDLL _UNICODE UNICODE REGISTER_PROXY_DLL WIN32
    "PROXY_CLSID_IS={0xDEC4804D,0x56D1,0x4F73,{0x9F,0xBE,0x68,0x28,0xE7,0xC8,0x5C,0x56}}"
    WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_openconsole_proxy_target} PRIVATE
    ${_native_product_compile_options}
    /Gz
    /std:c17
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_libraries(${_openconsole_proxy_target} PRIVATE
    onecoreuap_apiset.lib d3dcompiler.lib dwmapi.lib uxtheme.lib shlwapi.lib ntdll.lib
)
target_link_options(${_openconsole_proxy_target} PRIVATE
    "/DEF:${_openconsole_proxy_source_root}/OpenConsoleProxy.def"
    /NODEFAULTLIB:LIBUCRT.LIB /DEFAULTLIB:ucrt.lib
    /MANIFEST:EMBED "/MANIFESTUAC:level='asInvoker' uiAccess='false'"
    /DEBUG:FULL /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /RELEASE
    /DYNAMICBASE /NXCOMPAT /guard:cf /DEBUGTYPE:CV,FIXUP
)
set_target_properties(${_openconsole_proxy_target} PROPERTIES
    LINKER_LANGUAGE CXX
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_server_target ConServer)
set(_server_source_root "${_openconsole_root}/src/server")
add_library(${_server_target} STATIC EXCLUDE_FROM_ALL
    "${_server_source_root}/ApiDispatchers.cpp"
    "${_server_source_root}/ApiDispatchersInternal.cpp"
    "${_server_source_root}/ApiMessage.cpp"
    "${_server_source_root}/ApiMessageState.cpp"
    "${_server_source_root}/ApiSorter.cpp"
    "${_server_source_root}/ConDrvDeviceComm.cpp"
    "${_server_source_root}/ConsoleShimPolicy.cpp"
    "${_server_source_root}/DeviceHandle.cpp"
    "${_server_source_root}/Entrypoints.cpp"
    "${_server_source_root}/IoDispatchers.cpp"
    "${_server_source_root}/IoSorter.cpp"
    "${_server_source_root}/ObjectHandle.cpp"
    "${_server_source_root}/ObjectHeader.cpp"
    "${_server_source_root}/ProcessHandle.cpp"
    "${_server_source_root}/ProcessList.cpp"
    "${_server_source_root}/ProcessPolicy.cpp"
    "${_server_source_root}/WaitBlock.cpp"
    "${_server_source_root}/WaitQueue.cpp"
    "${_server_source_root}/WinNTControl.cpp"
)
add_dependencies(${_server_target} ${_openconsole_proxy_target})
target_compile_features(${_server_target} PRIVATE cxx_std_20)
target_precompile_headers(${_server_target} PRIVATE "${_server_source_root}/precomp.h")
target_include_directories(${_server_target} PRIVATE
    "${_openconsole_proxy_generated_root}"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_server_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_server_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_server_target} PROPERTIES
    OUTPUT_NAME "ConServer"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_host_library_target ConhostV2Lib)
set(_host_source_root "${_openconsole_root}/src/host")
set(_host_library_sources
    "${_host_source_root}/alias.cpp"
    "${_host_source_root}/cmdline.cpp"
    "${_host_source_root}/ConsoleArguments.cpp"
    "${_host_source_root}/readDataCooked.cpp"
    "${_host_source_root}/consoleInformation.cpp"
    "${_host_source_root}/dbcs.cpp"
    "${_host_source_root}/directio.cpp"
    "${_host_source_root}/getset.cpp"
    "${_host_source_root}/globals.cpp"
    "${_host_source_root}/handle.cpp"
    "${_host_source_root}/history.cpp"
    "${_host_source_root}/init.cpp"
    "${_host_source_root}/input.cpp"
    "${_host_source_root}/inputBuffer.cpp"
    "${_host_source_root}/inputKeyInfo.cpp"
    "${_host_source_root}/inputReadHandleData.cpp"
    "${_host_source_root}/misc.cpp"
    "${_host_source_root}/ntprivapi.cpp"
    "${_host_source_root}/output.cpp"
    "${_host_source_root}/outputStream.cpp"
    "${_host_source_root}/PtySignalInputThread.cpp"
    "${_host_source_root}/readData.cpp"
    "${_host_source_root}/readDataDirect.cpp"
    "${_host_source_root}/readDataRaw.cpp"
    "${_host_source_root}/registry.cpp"
    "${_host_source_root}/renderData.cpp"
    "${_host_source_root}/renderFontDefaults.cpp"
    "${_host_source_root}/screenInfo.cpp"
    "${_host_source_root}/scrolling.cpp"
    "${_host_source_root}/selection.cpp"
    "${_host_source_root}/selectionInput.cpp"
    "${_host_source_root}/selectionState.cpp"
    "${_host_source_root}/settings.cpp"
    "${_host_source_root}/srvinit.cpp"
    "${_host_source_root}/stream.cpp"
    "${_host_source_root}/tracing.cpp"
    "${_host_source_root}/utils.cpp"
    "${_host_source_root}/VtInputThread.cpp"
    "${_host_source_root}/VtIo.cpp"
    "${_host_source_root}/writeData.cpp"
    "${_host_source_root}/_output.cpp"
    "${_host_source_root}/_stream.cpp"
    "${_host_source_root}/AccessibilityNotifier.cpp"
)
add_library(${_host_library_target} STATIC EXCLUDE_FROM_ALL ${_host_library_sources})
add_dependencies(${_host_library_target} ${_openconsole_proxy_target})
target_compile_features(${_host_library_target} PRIVATE cxx_std_20)
target_precompile_headers(${_host_library_target} PRIVATE "${_host_source_root}/precomp.h")
target_include_directories(${_host_library_target} PRIVATE
    "${_openconsole_proxy_generated_root}"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_host_library_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_host_library_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_host_library_target} PROPERTIES
    OUTPUT_NAME "ConhostV2Lib"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_host_unittest_library_target ConhostV2Lib_unittest)
add_library(${_host_unittest_library_target} STATIC EXCLUDE_FROM_ALL ${_host_library_sources})
add_dependencies(${_host_unittest_library_target} ${_openconsole_proxy_target})
target_compile_features(${_host_unittest_library_target} PRIVATE cxx_std_20)
target_precompile_headers(${_host_unittest_library_target} PRIVATE "${_host_source_root}/precomp.h")
target_include_directories(${_host_unittest_library_target} PRIVATE
    "${_openconsole_proxy_generated_root}"
    "${_openconsole_root}/packages/Microsoft.Taef.10.100.251104001/build/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_host_unittest_library_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_host_unittest_library_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_host_unittest_library_target} PROPERTIES
    OUTPUT_NAME "ConhostV2Lib.unittest"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_conpty_library_target conptylib)
set(_conpty_source_root "${_openconsole_root}/src/winconpty")
add_library(${_conpty_library_target} STATIC EXCLUDE_FROM_ALL
    "${_conpty_source_root}/winconpty.cpp"
    "${_server_source_root}/DeviceHandle.cpp"
    "${_server_source_root}/WinNTControl.cpp"
)
target_compile_features(${_conpty_library_target} PRIVATE cxx_std_20)
target_precompile_headers(${_conpty_library_target} PRIVATE "${_conpty_source_root}/precomp.h")
target_include_directories(${_conpty_library_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_conpty_library_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_conpty_library_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_conpty_library_target} PROPERTIES
    OUTPUT_NAME "conptylib"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_winconpty_dll_target winconpty_dll)
add_library(${_winconpty_dll_target} SHARED
    "${_conpty_source_root}/precomp.cpp")
target_compile_features(${_winconpty_dll_target} PRIVATE cxx_std_20)
target_include_directories(${_winconpty_dll_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_winconpty_dll_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_winconpty_dll_target} PRIVATE
    ${_native_product_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_winconpty_dll_target} PRIVATE ${_portable_msvc_lib_dirs})
target_link_libraries(${_winconpty_dll_target} PRIVATE
    ${_conpty_library_target}
    ntdll.lib
)
target_link_options(${_winconpty_dll_target} PRIVATE
    "/DEF:${_conpty_source_root}/dll/winconpty.def"
    /guard:cf
    /DEBUG:FULL
    /OPT:REF
    /OPT:ICF
    /RELEASE
    /DYNAMICBASE
    /NXCOMPAT
)
set_target_properties(${_winconpty_dll_target} PROPERTIES
    OUTPUT_NAME "conpty"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_winconpty_feature_tests_target winconpty_feature_tests)
set(_winconpty_feature_tests_root "${_conpty_source_root}/ft_pty")
set(_taef_package_root "${_openconsole_root}/packages/Microsoft.Taef.10.100.251104001/build")
add_library(${_winconpty_feature_tests_target} SHARED
    "${_winconpty_feature_tests_root}/ConPtyTests.cpp"
    "${_winconpty_feature_tests_root}/precomp.cpp"
)
target_compile_features(${_winconpty_feature_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_winconpty_feature_tests_target} PRIVATE
    "${_winconpty_feature_tests_root}/precomp.h"
)
target_include_directories(${_winconpty_feature_tests_target} PRIVATE
    "${_winconpty_feature_tests_root}"
    "${_taef_package_root}/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_winconpty_feature_tests_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING CONPTY_IMPEXP=
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_winconpty_feature_tests_target} PRIVATE
    ${_native_product_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_winconpty_feature_tests_target} PRIVATE
    ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(${_winconpty_feature_tests_target} PRIVATE
    ${_conpty_library_target}
    Wex.Logger.lib
    Wex.Common.lib
    TE.Common.lib
    ntdll.lib
)
target_link_options(${_winconpty_feature_tests_target} PRIVATE
    /guard:cf
    /DEBUG:FULL
    /OPT:REF
    /OPT:ICF
    /RELEASE
    /DYNAMICBASE
    /NXCOMPAT
)
set_target_properties(${_winconpty_feature_tests_target} PROPERTIES
    OUTPUT_NAME "winconpty.Feature.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_parser_unit_tests_target ParserUnitTests)
set(_parser_unit_tests_root "${_terminal_parser_source_root}/ut_parser")
add_library(${_parser_unit_tests_target} SHARED
    "${_parser_unit_tests_root}/InputEngineTest.cpp"
    "${_parser_unit_tests_root}/OutputEngineTest.cpp"
    "${_parser_unit_tests_root}/StateMachineTest.cpp"
    "${_parser_unit_tests_root}/Base64Test.cpp"
    "${_terminal_parser_source_root}/precomp.cpp"
)
target_compile_features(${_parser_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_parser_unit_tests_target} PRIVATE
    "${_terminal_parser_source_root}/precomp.h"
)
target_include_directories(${_parser_unit_tests_target} PRIVATE
    "${_terminal_parser_source_root}"
    "${_taef_package_root}/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_parser_unit_tests_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_parser_unit_tests_target} PRIVATE
    ${_native_product_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_parser_unit_tests_target} PRIVATE
    ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(${_parser_unit_tests_target} PRIVATE
    TerminalInput
    ConTermAdapt
    ConTypes
    ConInteractivityBaseLib
    ConTermParser
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib ntdll.lib
    onecore.lib crypt32.lib bcrypt.lib icu.lib
)
target_link_options(${_parser_unit_tests_target} PRIVATE
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_parser_unit_tests_target} PROPERTIES
    OUTPUT_NAME "ConParser.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_types_unit_tests_target TypesUnitTests)
set(_types_unit_tests_root "${_openconsole_root}/src/types/ut_types")
set(_types_source_root "${_openconsole_root}/src/types")
add_library(${_types_unit_tests_target} SHARED
    "${_types_unit_tests_root}/CodepointWidthDetectorTests.cpp"
    "${_types_unit_tests_root}/UtilsTests.cpp"
    "${_types_unit_tests_root}/UuidTests.cpp"
    "${_types_source_root}/precomp.cpp"
)
target_compile_features(${_types_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_types_unit_tests_target} PRIVATE
    "${_types_source_root}/precomp.h"
)
target_include_directories(${_types_unit_tests_target} PRIVATE
    "${_types_source_root}"
    "${_taef_package_root}/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_types_unit_tests_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_types_unit_tests_target} PRIVATE
    ${_native_product_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_types_unit_tests_target} PRIVATE
    ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(${_types_unit_tests_target} PRIVATE
    ConTypes
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib ntdll.lib bcrypt.lib icu.lib
)
target_link_options(${_types_unit_tests_target} PRIVATE
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_types_unit_tests_target} PROPERTIES
    OUTPUT_NAME "Types.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_textbuffer_unit_tests_target TextBufferUnitTests)
set(_textbuffer_unit_tests_root "${_openconsole_root}/src/buffer/out/ut_textbuffer")
add_library(${_textbuffer_unit_tests_target} SHARED
    "${_textbuffer_unit_tests_root}/ReflowTests.cpp"
    "${_textbuffer_unit_tests_root}/TextColorTests.cpp"
    "${_textbuffer_unit_tests_root}/TextAttributeTests.cpp"
    "${_textbuffer_unit_tests_root}/UTextAdapterTests.cpp"
    "${_textbuffer_unit_tests_root}/precomp.cpp"
)
target_compile_features(${_textbuffer_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_textbuffer_unit_tests_target} PRIVATE
    "${_textbuffer_unit_tests_root}/precomp.h"
)
target_include_directories(${_textbuffer_unit_tests_target} PRIVATE
    "${_openconsole_root}/src/buffer/out"
    "${_taef_package_root}/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_textbuffer_unit_tests_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_textbuffer_unit_tests_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_textbuffer_unit_tests_target} PRIVATE
    ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(${_textbuffer_unit_tests_target} PRIVATE
    ConTypes ConRenderBase ConBufferOut
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib ntdll.lib
    onecore.lib crypt32.lib bcrypt.lib icu.lib
)
target_link_options(${_textbuffer_unit_tests_target} PRIVATE
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_textbuffer_unit_tests_target} PROPERTIES
    OUTPUT_NAME "TextBuffer.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_adapter_unit_tests_target AdapterUnitTests)
set(_adapter_unit_tests_root "${_terminal_adapter_source_root}/ut_adapter")
add_library(${_adapter_unit_tests_target} SHARED
    "${_adapter_unit_tests_root}/adapterTest.cpp"
    "${_adapter_unit_tests_root}/inputTest.cpp"
    "${_adapter_unit_tests_root}/kittyKeyboardProtocol.cpp"
    "${_adapter_unit_tests_root}/MouseInputTest.cpp"
    "${_terminal_adapter_source_root}/precomp.cpp"
)
target_compile_features(${_adapter_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_adapter_unit_tests_target} PRIVATE
    "${_terminal_adapter_source_root}/precomp.h"
)
target_include_directories(${_adapter_unit_tests_target} PRIVATE
    "${_terminal_adapter_source_root}"
    "${_taef_package_root}/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_adapter_unit_tests_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_adapter_unit_tests_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_adapter_unit_tests_target} PRIVATE
    ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(${_adapter_unit_tests_target} PRIVATE
    ConTypes ConRenderAtlas TerminalInput ConTermAdapt ConInteractivityWin32Lib
    ConhostV2Lib ConTSF ConRenderGdi ConInteractivityBaseLib ConRenderBase
    ConBufferOut ConServer ConTermParser ConProps
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib ntdll.lib
    onecore.lib crypt32.lib bcrypt.lib icu.lib winmm.lib imm32.lib
)
target_link_options(${_adapter_unit_tests_target} PRIVATE
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_adapter_unit_tests_target} PROPERTIES
    OUTPUT_NAME "ConAdapter.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_interactivity_win32_unit_tests_target InteractivityWin32UnitTests)
set(_interactivity_win32_unit_tests_root "${_interactivity_win32_source_root}/ut_interactivity_win32")
add_library(${_interactivity_win32_unit_tests_target} SHARED
    "${_interactivity_win32_unit_tests_root}/UiaTextRangeTests.cpp"
    "${_interactivity_win32_source_root}/precomp.cpp"
)
target_compile_features(${_interactivity_win32_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_interactivity_win32_unit_tests_target} PRIVATE
    "${_interactivity_win32_source_root}/precomp.h"
)
target_include_directories(${_interactivity_win32_unit_tests_target} PRIVATE
    "${_interactivity_win32_source_root}"
    "${_taef_package_root}/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_interactivity_win32_unit_tests_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_interactivity_win32_unit_tests_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_interactivity_win32_unit_tests_target} PRIVATE
    ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(${_interactivity_win32_unit_tests_target} PRIVATE
    ConTypes ConRenderAtlas TerminalInput ConTermAdapt ConInteractivityWin32Lib
    ConhostV2Lib MidiAudio ConInt ConTSF ConRenderGdi ConInteractivityBaseLib ConRenderBase
    ConBufferOut ConServer ConTermParser ConProps
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib ntdll.lib
    onecore.lib crypt32.lib bcrypt.lib icu.lib winmm.lib imm32.lib propsys.lib
    d2d1.lib dwrite.lib dxgi.lib d3d11.lib d3dcompiler.lib windowscodecs.lib
)
target_link_options(${_interactivity_win32_unit_tests_target} PRIVATE
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_interactivity_win32_unit_tests_target} PROPERTIES
    OUTPUT_NAME "Conhost.Interactivity.Win32.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_til_unit_tests_target TilUnitTests)
set(_til_unit_tests_root "${_openconsole_root}/src/til/ut_til")
set(_til_source_root "${_openconsole_root}/src/til")
add_library(${_til_unit_tests_target} SHARED
    "${_til_source_root}/precomp.cpp"
    "${_til_unit_tests_root}/BaseTests.cpp"
    "${_til_unit_tests_root}/CoalesceTests.cpp"
    "${_til_unit_tests_root}/ColorTests.cpp"
    "${_til_unit_tests_root}/EnumSetTests.cpp"
    "${_til_unit_tests_root}/EnvTests.cpp"
    "${_til_unit_tests_root}/FlatSetTests.cpp"
    "${_til_unit_tests_root}/GenerationalTests.cpp"
    "${_til_unit_tests_root}/HashTests.cpp"
    "${_til_unit_tests_root}/MathTests.cpp"
    "${_til_unit_tests_root}/mutex.cpp"
    "${_til_unit_tests_root}/OperatorTests.cpp"
    "${_til_unit_tests_root}/PointTests.cpp"
    "${_til_unit_tests_root}/RectangleTests.cpp"
    "${_til_unit_tests_root}/ReplaceTests.cpp"
    "${_til_unit_tests_root}/RunLengthEncodingTests.cpp"
    "${_til_unit_tests_root}/SizeTests.cpp"
    "${_til_unit_tests_root}/SmallVectorTests.cpp"
    "${_til_unit_tests_root}/SPSCTests.cpp"
    "${_til_unit_tests_root}/StaticMapTests.cpp"
    "${_til_unit_tests_root}/string.cpp"
    "${_til_unit_tests_root}/throttled_func.cpp"
    "${_til_unit_tests_root}/u8u16convertTests.cpp"
    "${_til_unit_tests_root}/UnicodeTests.cpp"
)
target_compile_features(${_til_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_til_unit_tests_target} PRIVATE "${_til_source_root}/precomp.h")
target_include_directories(${_til_unit_tests_target} PRIVATE
    "${_til_source_root}"
    "${_taef_package_root}/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_til_unit_tests_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_til_unit_tests_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_til_unit_tests_target} PRIVATE
    ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(${_til_unit_tests_target} PRIVATE
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib ntdll.lib onecore.lib
)
target_link_options(${_til_unit_tests_target} PRIVATE
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_til_unit_tests_target} PROPERTIES
    OUTPUT_NAME "til.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_host_unit_tests_target HostUnitTests)
set(_host_unit_tests_root "${_openconsole_root}/src/host/ut_host")
set(_host_source_root "${_openconsole_root}/src/host")
add_library(${_host_unit_tests_target} SHARED
    "${_host_unit_tests_root}/AliasTests.cpp"
    "${_host_unit_tests_root}/ApiRoutinesTests.cpp"
    "${_host_unit_tests_root}/ClipboardTests.cpp"
    "${_host_unit_tests_root}/ConsoleArgumentsTests.cpp"
    "${_host_unit_tests_root}/HistoryTests.cpp"
    "${_host_unit_tests_root}/InitTests.cpp"
    "${_host_unit_tests_root}/ObjectTests.cpp"
    "${_host_unit_tests_root}/OutputCellIteratorTests.cpp"
    "${_host_unit_tests_root}/ScreenBufferTests.cpp"
    "${_host_unit_tests_root}/SearchTests.cpp"
    "${_host_unit_tests_root}/SelectionTests.cpp"
    "${_host_unit_tests_root}/TextBufferIteratorTests.cpp"
    "${_host_unit_tests_root}/TextBufferTests.cpp"
    "${_host_unit_tests_root}/TitleTests.cpp"
    "${_host_unit_tests_root}/UtilsTests.cpp"
    "${_host_unit_tests_root}/InputBufferTests.cpp"
    "${_host_unit_tests_root}/ViewportTests.cpp"
    "${_host_unit_tests_root}/VtIoTests.cpp"
    "${_host_source_root}/precomp.cpp"
)
target_compile_features(${_host_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_host_unit_tests_target} PRIVATE "${_host_source_root}/precomp.h")
target_include_directories(${_host_unit_tests_target} PRIVATE
    "${_host_source_root}"
    "${_taef_package_root}/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_host_unit_tests_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_host_unit_tests_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_host_unit_tests_target} PRIVATE
    ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(${_host_unit_tests_target} PRIVATE
    ConhostV2Lib_unittest ConTypes ConRenderAtlas TerminalInput ConTermAdapt
    ConInteractivityWin32Lib ConInt ConTSF ConRenderGdi ConInteractivityBaseLib
    ConRenderBase ConBufferOut ConServer ConTermParser ConProps MidiAudio
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib ntdll.lib
    onecore.lib crypt32.lib bcrypt.lib icu.lib winmm.lib imm32.lib propsys.lib
    d2d1.lib dwrite.lib dxgi.lib d3d11.lib d3dcompiler.lib windowscodecs.lib
)
target_link_options(${_host_unit_tests_target} PRIVATE
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_host_unit_tests_target} PROPERTIES
    OUTPUT_NAME "Conhost.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_host_feature_tests_target HostFeatureTests)
set(_host_feature_tests_root "${_host_source_root}/ft_host")
add_library(${_host_feature_tests_target} SHARED
    "${_host_feature_tests_root}/API_AliasTests.cpp"
    "${_host_feature_tests_root}/API_BufferTests.cpp"
    "${_host_feature_tests_root}/API_CursorTests.cpp"
    "${_host_feature_tests_root}/API_DimensionsTests.cpp"
    "${_host_feature_tests_root}/API_FileTests.cpp"
    "${_host_feature_tests_root}/API_FillOutputTests.cpp"
    "${_host_feature_tests_root}/API_FontTests.cpp"
    "${_host_feature_tests_root}/API_InputTests.cpp"
    "${_host_feature_tests_root}/API_ModeTests.cpp"
    "${_host_feature_tests_root}/API_MultipleInflightMessageTests.cpp"
    "${_host_feature_tests_root}/API_OutputTests.cpp"
    "${_host_feature_tests_root}/API_RgbColorTests.cpp"
    "${_host_feature_tests_root}/API_TitleTests.cpp"
    "${_host_feature_tests_root}/API_PolicyTests.cpp"
    "${_host_feature_tests_root}/CanaryTests.cpp"
    "${_host_feature_tests_root}/CJK_DbcsTests.cpp"
    "${_host_feature_tests_root}/Common.cpp"
    "${_host_feature_tests_root}/InitTests.cpp"
    "${_host_feature_tests_root}/Message_KeyPressTests.cpp"
    "${_host_feature_tests_root}/OneCoreDelay.cpp"
    "${_host_feature_tests_root}/precomp.cpp"
    "${_host_feature_tests_root}/Host.Tests.Feature.rc"
)
target_compile_features(${_host_feature_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_host_feature_tests_target} PRIVATE "${_host_feature_tests_root}/precomp.h")
target_include_directories(${_host_feature_tests_target} PRIVATE
    "${_host_feature_tests_root}"
    "${_taef_package_root}/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_host_feature_tests_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING CONPTY_IMPEXP=
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_host_feature_tests_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_host_feature_tests_target} PRIVATE
    ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(${_host_feature_tests_target} PRIVATE
    ConTypes ${_conpty_library_target}
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib ntdll.lib
    onecore.lib crypt32.lib bcrypt.lib icu.lib
)
target_link_options(${_host_feature_tests_target} PRIVATE
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_host_feature_tests_target} PROPERTIES
    OUTPUT_NAME "ConHost.Feature.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_host_fuzz_wrapper_target HostFuzzWrapper)
set(_host_fuzz_wrapper_root "${_host_source_root}/ft_fuzzer")
if(PORTABLE_PLATFORM STREQUAL "x64")
    set(_host_fuzz_asan_runtime "clang_rt.asan_dynamic-x86_64.lib")
    set(_host_fuzz_asan_runtime_dll "clang_rt.asan_dynamic-x86_64.dll")
    set(_host_fuzz_runtime "clang_rt.fuzzer_MT-x86_64.lib")
elseif(PORTABLE_PLATFORM STREQUAL "x86")
    set(_host_fuzz_asan_runtime "clang_rt.asan_dynamic-i386.lib")
    set(_host_fuzz_asan_runtime_dll "clang_rt.asan_dynamic-i386.dll")
    set(_host_fuzz_runtime "clang_rt.fuzzer_MT-i386.lib")
elseif(PORTABLE_PLATFORM STREQUAL "arm64")
    set(_host_fuzz_asan_runtime "clang_rt.asan_dynamic-aarch64.lib")
    set(_host_fuzz_asan_runtime_dll "clang_rt.asan_dynamic-aarch64.dll")
    set(_host_fuzz_runtime "clang_rt.fuzzer_MT-aarch64.lib")
else()
    message(FATAL_ERROR "Unsupported fuzzer platform: ${PORTABLE_PLATFORM}")
endif()
add_executable(${_host_fuzz_wrapper_target}
    "${_host_source_root}/precomp.cpp"
    "${_host_fuzz_wrapper_root}/fuzzmain.cpp"
)
target_compile_features(${_host_fuzz_wrapper_target} PRIVATE cxx_std_20)
target_precompile_headers(${_host_fuzz_wrapper_target} PRIVATE "${_host_source_root}/precomp.h")
target_include_directories(${_host_fuzz_wrapper_target} PRIVATE
    "${_host_source_root}"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_host_fuzz_wrapper_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _CONSOLE EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_host_fuzz_wrapper_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor
    "$<$<CONFIG:Fuzzing>:/fsanitize=address>"
    "$<$<CONFIG:Fuzzing>:/fsanitize-coverage=inline-bool-flag>"
    "$<$<CONFIG:Fuzzing>:/fsanitize-coverage=edge>"
    "$<$<CONFIG:Fuzzing>:/fsanitize-coverage=trace-cmp>"
    "$<$<CONFIG:Fuzzing>:/fsanitize-coverage=trace-div>"
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_host_fuzz_wrapper_target} PRIVATE ${_portable_msvc_lib_dirs})
target_link_libraries(${_host_fuzz_wrapper_target} PRIVATE
    ConTypes ConRenderAtlas TerminalInput ConTermAdapt ConInteractivityWin32Lib
    ConhostV2Lib MidiAudio ConInt ConTSF ConRenderGdi ConInteractivityBaseLib
    ConRenderBase ConBufferOut ConServer ConTermParser ConProps
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    ntdll.lib onecore.lib crypt32.lib bcrypt.lib icu.lib winmm.lib imm32.lib propsys.lib
    d2d1.lib dwrite.lib dxgi.lib d3d11.lib d3dcompiler.lib windowscodecs.lib
    "$<$<CONFIG:Fuzzing>:libsancov.lib>"
    "$<$<CONFIG:Fuzzing>:${_host_fuzz_runtime}>"
    "$<$<CONFIG:Fuzzing>:${_host_fuzz_asan_runtime}>"
)
target_link_options(${_host_fuzz_wrapper_target} PRIVATE
    /SUBSYSTEM:CONSOLE /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_host_fuzz_wrapper_target} PROPERTIES
    OUTPUT_NAME "OpenConsoleFuzzer"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)
set(_host_fuzz_asan_runtime_dll_path "${_portable_msvc_root}/bin/Hostx64/${PORTABLE_PLATFORM}/${_host_fuzz_asan_runtime_dll}")
if(NOT EXISTS "${_host_fuzz_asan_runtime_dll_path}")
    message(FATAL_ERROR "Required ASan runtime is missing: ${_host_fuzz_asan_runtime_dll_path}")
endif()
add_custom_command(TARGET ${_host_fuzz_wrapper_target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_host_fuzz_asan_runtime_dll_path}"
        "$<TARGET_FILE_DIR:${_host_fuzz_wrapper_target}>/${_host_fuzz_asan_runtime_dll}"
    VERBATIM)

set(_propsheet_target PropsheetDll)
set(_propsheet_source_root "${_openconsole_root}/src/propsheet")
set(_propsheet_generated_root "${CMAKE_CURRENT_BINARY_DIR}/generated/propsheet")
set(_propsheet_generated_header "${_propsheet_generated_root}/strid.h")
set(_propsheet_generated_resource "${_propsheet_generated_root}/strid.rc")
add_custom_command(
    OUTPUT "${_propsheet_generated_header}" "${_propsheet_generated_resource}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_propsheet_generated_root}"
    COMMAND "${_portable_mc_path}" /h "${_propsheet_generated_root}" /r "${_propsheet_generated_root}"
        "${_propsheet_source_root}/strid.mc"
    DEPENDS "${_propsheet_source_root}/strid.mc"
    VERBATIM
)
add_custom_target(PropsheetMessages DEPENDS
    "${_propsheet_generated_header}"
    "${_propsheet_generated_resource}"
)
add_library(${_propsheet_target} SHARED
    "${_propsheet_source_root}/console.cpp"
    "${_propsheet_source_root}/globals.cpp"
    "${_propsheet_source_root}/dbcs.cpp"
    "${_propsheet_source_root}/dll.cpp"
    "${_propsheet_source_root}/fontdlg.cpp"
    "${_propsheet_source_root}/init.cpp"
    "${_propsheet_source_root}/misc.cpp"
    "${_propsheet_source_root}/preview.cpp"
    "${_propsheet_source_root}/PropSheetHandler.cpp"
    "${_propsheet_source_root}/OptionsPage.cpp"
    "${_propsheet_source_root}/LayoutPage.cpp"
    "${_propsheet_source_root}/ColorsPage.cpp"
    "${_propsheet_source_root}/ColorControl.cpp"
    "${_propsheet_source_root}/TerminalPropsheetPage.cpp"
    "${_propsheet_source_root}/registry.cpp"
    "${_propsheet_source_root}/util.cpp"
    "${_propsheet_source_root}/precomp.cpp"
    "${_propsheet_source_root}/console.rc"
)
add_dependencies(${_propsheet_target} PropsheetMessages)
target_compile_features(${_propsheet_target} PRIVATE cxx_std_20)
target_precompile_headers(${_propsheet_target} PRIVATE "${_propsheet_source_root}/precomp.h")
target_include_directories(${_propsheet_target} PRIVATE
    "${_propsheet_generated_root}"
    "${_propsheet_source_root}"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/host"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_propsheet_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_propsheet_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_propsheet_target} PRIVATE ${_portable_msvc_lib_dirs})
target_link_libraries(${_propsheet_target} PRIVATE
    ConInt ConProps comctl32.lib RuntimeObject.lib Shlwapi.lib propsys.lib ntdll.lib onecore.lib
)
target_link_options(${_propsheet_target} PRIVATE
    "/DEF:${_propsheet_source_root}/console.def"
    "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_propsheet_target} PROPERTIES
    OUTPUT_NAME "console"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_host_executable_target HostExecutable)
set(_host_executable_root "${_host_source_root}/exe")
add_executable(${_host_executable_target}
    "${_host_source_root}/precomp.cpp"
    "${_host_executable_root}/CConsoleHandoff.cpp"
    "${_host_executable_root}/exemain.cpp"
    "${_host_executable_root}/Host.EXE.rc"
    "${_host_executable_root}/openconsole.exe.manifest"
)
target_compile_features(${_host_executable_target} PRIVATE cxx_std_20)
target_precompile_headers(${_host_executable_target} PRIVATE "${_host_source_root}/precomp.h")
target_include_directories(${_host_executable_target} PRIVATE
    "${_host_source_root}"
    "${_openconsole_proxy_generated_root}"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_host_executable_target} PRIVATE
    _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _CONSOLE EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_host_executable_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
target_link_directories(${_host_executable_target} PRIVATE ${_portable_msvc_lib_dirs})
target_link_libraries(${_host_executable_target} PRIVATE
    ConTypes ConRenderAtlas TerminalInput ConTermAdapt ConInteractivityWin32Lib
    ConhostV2Lib MidiAudio ConInt ConTSF ConRenderGdi ConInteractivityBaseLib
    ConRenderBase ConBufferOut ConServer ConTermParser ConProps
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    delayimp.lib ntdll.lib onecore.lib crypt32.lib bcrypt.lib icu.lib winmm.lib imm32.lib propsys.lib
    d2d1.lib dwrite.lib dxgi.lib d3d11.lib d3dcompiler.lib windowscodecs.lib
)
target_link_options(${_host_executable_target} PRIVATE
    /SUBSYSTEM:WINDOWS /DELAYLOAD:icu.dll
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_host_executable_target} PROPERTIES
    OUTPUT_NAME "OpenConsole"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_renderer_atlas_target ConRenderAtlas)
set(_renderer_atlas_source_root "${_openconsole_root}/src/renderer/atlas")
set(_renderer_atlas_generated_root "${_native_product_output_root}/RendererAtlas")
set(_renderer_atlas_shader_headers
    "${_renderer_atlas_generated_root}/custom_shader_ps.h"
    "${_renderer_atlas_generated_root}/custom_shader_vs.h"
    "${_renderer_atlas_generated_root}/shader_ps.h"
    "${_renderer_atlas_generated_root}/shader_vs.h"
)
add_custom_command(
    OUTPUT "${_renderer_atlas_generated_root}/custom_shader_ps.h"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_renderer_atlas_generated_root}"
    COMMAND "${_portable_fxc_path}" /nologo /E main /WX /T ps_4_0 /all_resources_bound
        /Fh "${_renderer_atlas_generated_root}/custom_shader_ps.h" /Vn custom_shader_ps
        /Zi /O3 /Zsb /Fd "${_renderer_atlas_generated_root}/" /Qstrip_debug /Qstrip_reflect
        "${_renderer_atlas_source_root}/custom_shader_ps.hlsl"
    DEPENDS "${_renderer_atlas_source_root}/custom_shader_ps.hlsl"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_renderer_atlas_generated_root}/custom_shader_vs.h"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_renderer_atlas_generated_root}"
    COMMAND "${_portable_fxc_path}" /nologo /E main /WX /T vs_4_0 /all_resources_bound
        /Fh "${_renderer_atlas_generated_root}/custom_shader_vs.h" /Vn custom_shader_vs
        /Zi /O3 /Zsb /Fd "${_renderer_atlas_generated_root}/" /Qstrip_debug /Qstrip_reflect
        "${_renderer_atlas_source_root}/custom_shader_vs.hlsl"
    DEPENDS "${_renderer_atlas_source_root}/custom_shader_vs.hlsl"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_renderer_atlas_generated_root}/shader_ps.h"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_renderer_atlas_generated_root}"
    COMMAND "${_portable_fxc_path}" /nologo /E main /WX /T ps_4_0 /all_resources_bound
        /Fh "${_renderer_atlas_generated_root}/shader_ps.h" /Vn shader_ps
        /Zi /O3 /Zsb /Fd "${_renderer_atlas_generated_root}/" /Qstrip_debug /Qstrip_reflect
        "${_renderer_atlas_source_root}/shader_ps.hlsl"
    DEPENDS
        "${_renderer_atlas_source_root}/shader_ps.hlsl"
        "${_renderer_atlas_source_root}/dwrite_helpers.hlsl"
        "${_renderer_atlas_source_root}/shader_common.hlsl"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_renderer_atlas_generated_root}/shader_vs.h"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_renderer_atlas_generated_root}"
    COMMAND "${_portable_fxc_path}" /nologo /E main /WX /T vs_4_0 /all_resources_bound
        /Fh "${_renderer_atlas_generated_root}/shader_vs.h" /Vn shader_vs
        /Zi /O3 /Zsb /Fd "${_renderer_atlas_generated_root}/" /Qstrip_debug /Qstrip_reflect
        "${_renderer_atlas_source_root}/shader_vs.hlsl"
    DEPENDS
        "${_renderer_atlas_source_root}/shader_vs.hlsl"
        "${_renderer_atlas_source_root}/shader_common.hlsl"
    VERBATIM
)
set_source_files_properties(${_renderer_atlas_shader_headers} PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_library(${_renderer_atlas_target} STATIC EXCLUDE_FROM_ALL
    "${_renderer_atlas_source_root}/AtlasEngine.cpp"
    "${_renderer_atlas_source_root}/AtlasEngine.api.cpp"
    "${_renderer_atlas_source_root}/AtlasEngine.r.cpp"
    "${_renderer_atlas_source_root}/Backend.cpp"
    "${_renderer_atlas_source_root}/BackendD2D.cpp"
    "${_renderer_atlas_source_root}/BackendD3D.cpp"
    "${_renderer_atlas_source_root}/BuiltinGlyphs.cpp"
    "${_renderer_atlas_source_root}/dwrite_helpers.cpp"
    "${_renderer_atlas_source_root}/DWriteTextAnalysis.cpp"
    "${_renderer_atlas_source_root}/stb_rect_pack.cpp"
    "${_renderer_atlas_source_root}/wic.cpp"
    ${_renderer_atlas_shader_headers}
)
target_compile_features(${_renderer_atlas_target} PRIVATE cxx_std_20)
target_precompile_headers(${_renderer_atlas_target} PRIVATE "${_renderer_atlas_source_root}/pch.h")
target_include_directories(${_renderer_atlas_target} PRIVATE
    "${_openconsole_root}/oss/stb"
    "${_renderer_atlas_generated_root}"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_renderer_atlas_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_renderer_atlas_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_renderer_atlas_target} PROPERTIES
    OUTPUT_NAME "ConRenderAtlas"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_types_target ConTypes)
set(_types_source_root "${_openconsole_root}/src/types")
add_library(${_types_target} STATIC EXCLUDE_FROM_ALL
    "${_types_source_root}/CodepointWidthDetector.cpp"
    "${_types_source_root}/ColorFix.cpp"
    "${_types_source_root}/convert.cpp"
    "${_types_source_root}/colorTable.cpp"
    "${_types_source_root}/GlyphWidth.cpp"
    "${_types_source_root}/ScreenInfoUiaProviderBase.cpp"
    "${_types_source_root}/sgrStack.cpp"
    "${_types_source_root}/ThemeUtils.cpp"
    "${_types_source_root}/UiaTextRangeBase.cpp"
    "${_types_source_root}/UiaTracing.cpp"
    "${_types_source_root}/TermControlUiaTextRange.cpp"
    "${_types_source_root}/TermControlUiaProvider.cpp"
    "${_types_source_root}/Viewport.cpp"
    "${_types_source_root}/utils.cpp"
)
target_compile_features(${_types_target} PRIVATE cxx_std_20)
target_precompile_headers(${_types_target} PRIVATE "${_types_source_root}/precomp.h")
target_include_directories(${_types_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_types_target} PRIVATE
    _LIB _UNICODE UNICODE WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_types_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_types_target} PROPERTIES
    OUTPUT_NAME "ConTypes"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

file(GLOB _themehelpers_package_dirs LIST_DIRECTORIES true "${_openconsole_root}/packages/Microsoft.Internal.Windows.Terminal.ThemeHelpers.*")
list(SORT _themehelpers_package_dirs COMPARE NATURAL ORDER DESCENDING)
list(GET _themehelpers_package_dirs 0 _themehelpers_package_dir)
if(NOT _themehelpers_package_dir)
    message(FATAL_ERROR "Could not locate a Microsoft.Internal.Windows.Terminal.ThemeHelpers package under ${_openconsole_root}/packages.")
endif()

set(_msvc_toolset_dir "${_portable_msvc_root}")

set(_workspace_extension_target WorkspaceExtension)
set(_workspace_extension_output_root "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/WorkspaceExtension")
set(_workspace_extension_obj_root "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/$<CONFIG>/WorkspaceExtension")
set(_workspace_extension_generated_root "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/$<CONFIG>/WorkspaceExtension/Generated Files")
set(_workspace_extension_platform_winmd "${_msvc_toolset_dir}/lib/x86/store/references/platform.winmd")
set(_workspace_extension_til_feature_header "${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h")
set(_workspace_extension_themehelpers_lib "${_themehelpers_package_dir}/runtimes/win10-x64/lib/uap10.0/TerminalThemeHelpers.lib")
set(_workspace_extension_vcpkg_lib_root "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib")
set(_workspace_extension_vcpkg_manual_lib_root "${_workspace_extension_vcpkg_lib_root}/manual-link")
set(_workspace_extension_release_root "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>")

file(GLOB _workspace_extension_vcpkg_libs "${_workspace_extension_vcpkg_lib_root}/*.lib")

set(_shell_extension_target WindowsTerminalShellExt)
set(_shell_extension_source_root "${_openconsole_root}/src/cascadia/ShellExtension")
set(_shell_extension_output_root "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/WindowsTerminalShellExt")
set(_cppwinrt_path "${_openconsole_root}/packages/Microsoft.Windows.CppWinRT.2.0.250303.1/bin/cppwinrt.exe")
set(_native_cppwinrt_projection_root "${CMAKE_BINARY_DIR}/native-cppwinrt-projection")
set(_native_cppwinrt_projection_base "${_native_cppwinrt_projection_root}/winrt/base.h")
if(NOT EXISTS "${_cppwinrt_path}")
    message(FATAL_ERROR "Could not find the pinned C++/WinRT generator at ${_cppwinrt_path}.")
endif()
add_custom_command(
    OUTPUT "${_native_cppwinrt_projection_base}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_native_cppwinrt_projection_root}"
    COMMAND "${_cppwinrt_path}"
        -input "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -output "${_native_cppwinrt_projection_root}"
        -base
    DEPENDS "${_cppwinrt_path}"
    VERBATIM
    COMMENT "Generating the native C++/WinRT platform projection"
)

set(_winrt_utils_target WinRTUtils)
set(_winrt_utils_source_root "${_openconsole_root}/src/cascadia/WinRTUtils")
add_library(${_winrt_utils_target} STATIC EXCLUDE_FROM_ALL
    "${_winrt_utils_source_root}/LibraryResources.cpp"
    "${_winrt_utils_source_root}/ScopedResourceLoader.cpp"
    "${_winrt_utils_source_root}/Utils.cpp"
    "${_native_cppwinrt_projection_base}"
)
target_compile_features(${_winrt_utils_target} PRIVATE cxx_std_20)
target_precompile_headers(${_winrt_utils_target} PRIVATE "${_winrt_utils_source_root}/pch.h")
target_include_directories(${_winrt_utils_target} PRIVATE
    "${_native_cppwinrt_projection_root}"
    "${_wil_package_dir}/include"
    "${_winrt_utils_source_root}"
    "${_winrt_utils_source_root}/inc"
    "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_winrt_utils_target} PRIVATE
    _LIB _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__
)
target_compile_options(${_winrt_utils_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    /bigobj
    "/FU${_workspace_extension_platform_winmd}"
    "/FI${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h"
)
set_target_properties(${_winrt_utils_target} PROPERTIES
    OUTPUT_NAME "WinRTUtils"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}/WinRTUtils"
)

set(_terminal_core_target TerminalCore)
set(_terminal_core_source_root "${_openconsole_root}/src/cascadia/TerminalCore")
set(_terminal_core_output_root "${_native_product_output_root}/TerminalCore")
set(_terminal_core_winmd "${_terminal_core_output_root}/Microsoft.Terminal.Core.winmd")
set(_terminal_core_pri "${_terminal_core_output_root}/Microsoft.Terminal.Core.pri")
set(_terminal_core_unmerged_root "${CMAKE_BINARY_DIR}/terminal-core/$<CONFIG>/unmerged")
set(_terminal_core_unmerged_winmd "${_terminal_core_unmerged_root}/ICoreSettings.winmd")
set(_terminal_core_projection_root "${CMAKE_BINARY_DIR}/terminal-core-projection")
set(_terminal_core_projection_header "${_terminal_core_projection_root}/winrt/Microsoft.Terminal.Core.h")
set(_terminal_core_foundation_winmd "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Windows.Foundation.FoundationContract/4.0.0.0/Windows.Foundation.FoundationContract.winmd")
file(TO_NATIVE_PATH "${_terminal_core_source_root}/ICoreSettings.idl" _terminal_core_idl_native)
file(TO_NATIVE_PATH "${_terminal_core_unmerged_winmd}" _terminal_core_unmerged_winmd_native)
file(TO_NATIVE_PATH "${_portable_sdk_include_root}/winrt" _terminal_core_sdk_winrt_native)
file(TO_NATIVE_PATH "${_portable_sdk_include_root}/shared" _terminal_core_sdk_shared_native)
file(TO_NATIVE_PATH "${_portable_sdk_include_root}/um" _terminal_core_sdk_um_native)
file(TO_NATIVE_PATH "${_terminal_core_foundation_winmd}" _terminal_core_foundation_winmd_native)
file(TO_NATIVE_PATH "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Windows.Foundation.FoundationContract/4.0.0.0" _terminal_core_metadata_dir_native)
add_custom_command(
    OUTPUT "${_terminal_core_winmd}" "${_terminal_core_projection_header}"
    BYPRODUCTS
        "${_terminal_core_unmerged_winmd}"
        "${_terminal_core_projection_root}/winrt/impl/Microsoft.Terminal.Core.0.h"
        "${_terminal_core_projection_root}/winrt/impl/Microsoft.Terminal.Core.1.h"
        "${_terminal_core_projection_root}/winrt/impl/Microsoft.Terminal.Core.2.h"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_core_output_root}" "${_terminal_core_projection_root}" "${_terminal_core_unmerged_root}"
    COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
        "${_portable_midl_path}"
        /I "${_terminal_core_sdk_winrt_native}"
        /I "${_terminal_core_sdk_shared_native}"
        /I "${_terminal_core_sdk_um_native}"
        /reference "${_terminal_core_foundation_winmd_native}"
        /metadata_dir "${_terminal_core_metadata_dir_native}"
        /winrt /W1 /nologo /char signed /env ${_portable_midl_environment}
        /winmd "${_terminal_core_unmerged_winmd_native}"
        /h NUL /dlldata NUL /iid NUL /proxy NUL /notlb /client none /server none
        /enum_class /ns_prefix /target NT60 /nomidl
        "${_terminal_core_idl_native}"
    COMMAND "${_portable_mdmerge_path}" -i "${_terminal_core_unmerged_root}" -o "${_terminal_core_output_root}"
        -metadata_dir "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Windows.Foundation.FoundationContract/4.0.0.0"
        -partial
    COMMAND "${_cppwinrt_path}"
        -input "${_terminal_core_winmd}"
        -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -output "${_terminal_core_projection_root}"
    DEPENDS
        "${_terminal_core_source_root}/ICoreSettings.idl"
        "${_terminal_core_foundation_winmd}"
        "${_portable_mdmerge_path}" "${_cppwinrt_path}"
    VERBATIM
    COMMENT "Generating TerminalCore WinMD and C++/WinRT projection"
)
set(_terminal_core_priconfig "${CMAKE_BINARY_DIR}/terminal-core/$<CONFIG>/priconfig.xml")
add_custom_command(
    OUTPUT "${_terminal_core_pri}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_core_output_root}"
    COMMAND "${PORTABLE_MAKEPRI_PATH}" createconfig /cf "${_terminal_core_priconfig}" /dq en-US /pv 10.0.0 /o
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new /pr "${_terminal_core_source_root}" /cf "${_terminal_core_priconfig}" /of "${_terminal_core_pri}" /in Microsoft.Terminal.Core /o
    VERBATIM
    COMMENT "Generating Microsoft.Terminal.Core.pri"
)
set_source_files_properties("${_terminal_core_projection_header}" "${_terminal_core_pri}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_library(${_terminal_core_target} STATIC EXCLUDE_FROM_ALL
    "${_terminal_core_source_root}/TerminalRenderData.cpp"
    "${_terminal_core_source_root}/TerminalSelection.cpp"
    "${_terminal_core_source_root}/TerminalApi.cpp"
    "${_terminal_core_source_root}/Terminal.cpp"
    "${_terminal_core_projection_header}"
    "${_terminal_core_pri}"
    "${_native_cppwinrt_projection_base}"
)
target_compile_features(${_terminal_core_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_core_target} PRIVATE "${_terminal_core_source_root}/pch.h")
target_include_directories(${_terminal_core_target} PRIVATE
    "${_terminal_core_projection_root}"
    "${_native_cppwinrt_projection_root}"
    "${_wil_package_dir}/include"
    "${_terminal_core_source_root}"
    "${_terminal_core_source_root}/lib"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc"
    "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_terminal_core_target} PRIVATE
    _LIB _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__
)
target_compile_options(${_terminal_core_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    /bigobj
    "/FU${_workspace_extension_platform_winmd}"
    "/FI${_workspace_extension_til_feature_header}"
)
set_target_properties(${_terminal_core_target} PROPERTIES
    OUTPUT_NAME "TerminalCore"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_terminal_core_output_root}"
)

set(_terminal_core_unit_tests_target TerminalCoreUnitTests)
set(_terminal_core_unit_tests_root "${_openconsole_root}/src/cascadia/UnitTests_TerminalCore")
add_library(${_terminal_core_unit_tests_target} SHARED
    "${_terminal_core_unit_tests_root}/ScreenSizeLimitsTest.cpp"
    "${_terminal_core_unit_tests_root}/SelectionTest.cpp"
    "${_terminal_core_unit_tests_root}/InputTest.cpp"
    "${_terminal_core_unit_tests_root}/pch.cpp"
    "${_terminal_core_unit_tests_root}/TerminalApiTest.cpp"
    "${_terminal_core_unit_tests_root}/TerminalBufferTests.cpp"
    "${_terminal_core_unit_tests_root}/ScrollTest.cpp"
    "${_terminal_core_unit_tests_root}/TilWinRtHelpersTests.cpp"
)
target_compile_features(${_terminal_core_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_core_unit_tests_target} PRIVATE "${_terminal_core_unit_tests_root}/pch.h")
target_include_directories(${_terminal_core_unit_tests_target} PRIVATE
    "${_terminal_core_unit_tests_root}"
    "${_openconsole_root}/src/cascadia"
    "${_terminal_core_source_root}"
    "${_terminal_core_projection_root}"
    "${_native_cppwinrt_projection_root}"
    "${_taef_package_root}/Include"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc"
    "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/host"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_terminal_core_unit_tests_target} PRIVATE
    _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING __WRL_NO_DEFAULT_LIB__
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_terminal_core_unit_tests_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor /bigobj
    "/FU${_workspace_extension_platform_winmd}"
    "/FI${_workspace_extension_til_feature_header}"
)
target_link_directories(${_terminal_core_unit_tests_target} PRIVATE
    ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}"
)
target_link_libraries(${_terminal_core_unit_tests_target} PRIVATE
    TerminalCore ConTypes ConRenderAtlas TerminalInput ConTermAdapt
    ConRenderBase ConBufferOut ConTermParser
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/lib/fmt.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib ntdll.lib
    WindowsApp.lib onecore.lib crypt32.lib bcrypt.lib icu.lib winmm.lib imm32.lib
    d2d1.lib dwrite.lib dxgi.lib d3d11.lib d3dcompiler.lib windowscodecs.lib
)
target_link_options(${_terminal_core_unit_tests_target} PRIVATE
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
set_target_properties(${_terminal_core_unit_tests_target} PROPERTIES
    OUTPUT_NAME "Terminal.Core.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}"
)

set(_terminal_connection_target TerminalConnection)
set(_terminal_connection_source_root "${_openconsole_root}/src/cascadia/TerminalConnection")
set(_terminal_connection_output_root "${_native_product_output_root}/TerminalConnection")
set(_terminal_connection_generation_root "${CMAKE_BINARY_DIR}/terminal-connection/$<CONFIG>")
set(_terminal_connection_unmerged_root "${_terminal_connection_generation_root}/unmerged")
set(_terminal_connection_projection_root "${_terminal_connection_generation_root}/projection")
set(_terminal_connection_winmd "${_terminal_connection_output_root}/Microsoft.Terminal.TerminalConnection.winmd")
set(_terminal_connection_pri "${_terminal_connection_output_root}/Microsoft.Terminal.TerminalConnection.pri")
set(_terminal_connection_module_cpp "${_terminal_connection_projection_root}/module.g.cpp")
set(_terminal_connection_projection_header "${_terminal_connection_projection_root}/winrt/Microsoft.Terminal.TerminalConnection.h")
file(GLOB_RECURSE _native_product_sdk_contract_winmds CONFIGURE_DEPENDS
    "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/*.winmd")
set(_native_product_sdk_midl_reference_args)
set(_native_product_sdk_mdmerge_metadata_args)
foreach(_native_product_sdk_contract_winmd IN LISTS _native_product_sdk_contract_winmds)
    file(TO_NATIVE_PATH "${_native_product_sdk_contract_winmd}" _native_product_sdk_contract_winmd_native)
    list(APPEND _native_product_sdk_midl_reference_args /reference "${_native_product_sdk_contract_winmd_native}")
    get_filename_component(_native_product_sdk_contract_dir "${_native_product_sdk_contract_winmd}" DIRECTORY)
    list(APPEND _native_product_sdk_mdmerge_metadata_args -metadata_dir "${_native_product_sdk_contract_dir}")
endforeach()
set(_terminal_connection_metadata_root "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Windows.Foundation.FoundationContract/4.0.0.0")
set(_terminal_connection_idls
    "${_terminal_connection_source_root}/ConnectionInformation.idl"
    "${_terminal_connection_source_root}/ITerminalConnection.idl"
    "${_terminal_connection_source_root}/ConptyConnection.idl"
    "${_terminal_connection_source_root}/EchoConnection.idl"
    "${_terminal_connection_source_root}/AzureConnection.idl"
)
file(TO_NATIVE_PATH "${_terminal_connection_source_root}" _terminal_connection_source_root_native)
file(TO_NATIVE_PATH "${_portable_sdk_include_root}/winrt" _terminal_connection_sdk_winrt_native)
file(TO_NATIVE_PATH "${_portable_sdk_include_root}/shared" _terminal_connection_sdk_shared_native)
file(TO_NATIVE_PATH "${_portable_sdk_include_root}/um" _terminal_connection_sdk_um_native)
file(TO_NATIVE_PATH "${_terminal_connection_metadata_root}" _terminal_connection_metadata_root_native)

set(_terminal_connection_unmerged_winmds)
foreach(_terminal_connection_idl IN LISTS _terminal_connection_idls)
    get_filename_component(_terminal_connection_idl_name "${_terminal_connection_idl}" NAME_WE)
    set(_terminal_connection_unmerged_winmd "${_terminal_connection_unmerged_root}/${_terminal_connection_idl_name}.winmd")
    file(TO_NATIVE_PATH "${_terminal_connection_idl}" _terminal_connection_idl_native)
    file(TO_NATIVE_PATH "${_terminal_connection_unmerged_winmd}" _terminal_connection_unmerged_winmd_native)
    add_custom_command(
        OUTPUT "${_terminal_connection_unmerged_winmd}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_connection_unmerged_root}"
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
            "${_portable_midl_path}"
            /I "${_terminal_connection_sdk_winrt_native}"
            /I "${_terminal_connection_sdk_shared_native}"
            /I "${_terminal_connection_sdk_um_native}"
            /I "${_terminal_connection_source_root_native}"
            ${_native_product_sdk_midl_reference_args}
            /metadata_dir "${_terminal_connection_metadata_root_native}"
            /winrt /W1 /nologo /char signed /env ${_portable_midl_environment}
            /winmd "${_terminal_connection_unmerged_winmd_native}"
            /h NUL /dlldata NUL /iid NUL /proxy NUL /notlb /client none /server none
            /enum_class /ns_prefix /target NT60 /nomidl
            "${_terminal_connection_idl_native}"
        DEPENDS ${_terminal_connection_idls} ${_native_product_sdk_contract_winmds} "${_portable_midl_path}"
        VERBATIM
        COMMENT "Generating ${_terminal_connection_idl_name}.winmd"
    )
    list(APPEND _terminal_connection_unmerged_winmds "${_terminal_connection_unmerged_winmd}")
endforeach()

add_custom_command(
    OUTPUT "${_terminal_connection_winmd}" "${_terminal_connection_module_cpp}" "${_terminal_connection_projection_header}"
    BYPRODUCTS
        "${_terminal_connection_projection_root}/AzureConnection.g.h"
        "${_terminal_connection_projection_root}/AzureConnection.g.cpp"
        "${_terminal_connection_projection_root}/ConnectionInformation.g.h"
        "${_terminal_connection_projection_root}/ConnectionInformation.g.cpp"
        "${_terminal_connection_projection_root}/ConptyConnection.g.h"
        "${_terminal_connection_projection_root}/ConptyConnection.g.cpp"
        "${_terminal_connection_projection_root}/EchoConnection.g.h"
        "${_terminal_connection_projection_root}/EchoConnection.g.cpp"
        "${_terminal_connection_projection_root}/winrt/fast_forward.h"
        "${_terminal_connection_projection_root}/winrt/impl/Microsoft.Terminal.TerminalConnection.0.h"
        "${_terminal_connection_projection_root}/winrt/impl/Microsoft.Terminal.TerminalConnection.1.h"
        "${_terminal_connection_projection_root}/winrt/impl/Microsoft.Terminal.TerminalConnection.2.h"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_connection_output_root}" "${_terminal_connection_projection_root}"
    COMMAND "${_portable_mdmerge_path}" -i "${_terminal_connection_unmerged_root}" -o "${_terminal_connection_output_root}"
        ${_native_product_sdk_mdmerge_metadata_args} -partial
    COMMAND "${_cppwinrt_path}"
        -input "${_terminal_connection_winmd}"
        -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -output "${_terminal_connection_projection_root}"
        -component "${_terminal_connection_projection_root}/sources"
        -overwrite
        -name Microsoft.Terminal.TerminalConnection
        -pch pch.h
        -prefix
        -optimize
    DEPENDS ${_terminal_connection_unmerged_winmds} "${_portable_mdmerge_path}" "${_cppwinrt_path}"
    VERBATIM
    COMMENT "Merging TerminalConnection WinMD and generating its component projection"
)

file(GLOB _terminal_connection_resw CONFIGURE_DEPENDS "${_terminal_connection_source_root}/Resources/*/Resources.resw")
set(_terminal_connection_resources_resfiles "")
foreach(_terminal_connection_resource IN LISTS _terminal_connection_resw)
    file(TO_NATIVE_PATH "${_terminal_connection_resource}" _terminal_connection_resource_native)
    string(APPEND _terminal_connection_resources_resfiles "${_terminal_connection_resource_native}\n")
endforeach()
set(_terminal_connection_resources_list "${_terminal_connection_generation_root}/resources.resfiles")
set(_terminal_connection_priconfig "${_terminal_connection_generation_root}/priconfig.xml")
file(TO_NATIVE_PATH "${_terminal_connection_resources_list}" _terminal_connection_resources_list_native)
file(GENERATE OUTPUT "${_terminal_connection_resources_list}" CONTENT "${_terminal_connection_resources_resfiles}")
file(GENERATE OUTPUT "${_terminal_connection_priconfig}" CONTENT
"<?xml version=\"1.0\" encoding=\"utf-8\"?>
<resources targetOsVersion=\"10.0.0\" majorVersion=\"1\">
  <index root=\"\\\" startIndexAt=\"${_terminal_connection_resources_list_native}\">
    <default>
      <qualifier name=\"Language\" value=\"en-US\" />
      <qualifier name=\"Contrast\" value=\"standard\" />
      <qualifier name=\"Scale\" value=\"200\" />
      <qualifier name=\"HomeRegion\" value=\"001\" />
      <qualifier name=\"TargetSize\" value=\"256\" />
      <qualifier name=\"LayoutDirection\" value=\"LTR\" />
      <qualifier name=\"DXFeatureLevel\" value=\"DX9\" />
      <qualifier name=\"Configuration\" value=\"\" />
      <qualifier name=\"AlternateForm\" value=\"\" />
      <qualifier name=\"Platform\" value=\"UAP\" />
    </default>
    <indexer-config type=\"RESW\" convertDotsToSlashes=\"true\" initialPath=\"Microsoft.Terminal.TerminalConnection\" />
    <indexer-config type=\"RESFILES\" qualifierDelimiter=\".\" />
  </index>
</resources>
")
add_custom_command(
    OUTPUT "${_terminal_connection_pri}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_connection_output_root}"
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new /pr "${_terminal_connection_source_root}" /cf "${_terminal_connection_priconfig}" /of "${_terminal_connection_pri}" /in Microsoft.Terminal.TerminalConnection /o
    DEPENDS ${_terminal_connection_resw} "${_terminal_connection_resources_list}" "${_terminal_connection_priconfig}"
    VERBATIM
    COMMENT "Generating Microsoft.Terminal.TerminalConnection.pri"
)

set_source_files_properties("${_terminal_connection_winmd}" "${_terminal_connection_pri}" "${_terminal_connection_projection_header}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_library(${_terminal_connection_target} SHARED
    "${_terminal_connection_source_root}/CTerminalHandoff.cpp"
    "${_terminal_connection_source_root}/init.cpp"
    "${_terminal_connection_source_root}/ConnectionInformation.cpp"
    "${_terminal_connection_source_root}/AzureConnection.cpp"
    "${_terminal_connection_source_root}/pch.cpp"
    "${_terminal_connection_source_root}/EchoConnection.cpp"
    "${_terminal_connection_source_root}/ConptyConnection.cpp"
    "${_terminal_connection_module_cpp}"
    "${_terminal_connection_source_root}/TerminalConnection.def"
    "${_terminal_connection_winmd}"
    "${_terminal_connection_pri}"
    "${_native_cppwinrt_projection_base}"
)
target_compile_features(${_terminal_connection_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_connection_target} PRIVATE "${_terminal_connection_source_root}/pch.h")
target_include_directories(${_terminal_connection_target} PRIVATE
    "${_terminal_connection_projection_root}" "${_native_cppwinrt_projection_root}" "${_openconsole_proxy_generated_root}"
    "${_wil_package_dir}/include" "${_terminal_connection_source_root}"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/cascadia/inc" "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include" ${_portable_msvc_include_dirs}
)
target_link_directories(${_terminal_connection_target} PRIVATE "${_workspace_extension_vcpkg_lib_root}" "${_workspace_extension_vcpkg_manual_lib_root}" ${_portable_msvc_lib_dirs})
target_compile_definitions(${_terminal_connection_target} PRIVATE
    CONPTY_IMPEXP= _USRDLL _WINDLL _WINRT_DLL _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__
)
target_compile_options(${_terminal_connection_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor /bigobj
    /wd5104 /wd28204 /wd4201 /wd4312 /wd5105 /wd26434 /wd26445 /wd26456 /wd26478 /wd26494
    "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}"
)
target_link_libraries(${_terminal_connection_target} PRIVATE
    ConTypes WinRTUtils conptylib
    "${_workspace_extension_vcpkg_lib_root}/fmt.lib"
    onecoreuap_apiset.lib d3dcompiler.lib dwmapi.lib uxtheme.lib shlwapi.lib ntdll.lib WindowsApp.lib
)
target_link_options(${_terminal_connection_target} PRIVATE
    /SUBSYSTEM:WINDOWS /MANIFEST:NO /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
)
add_dependencies(${_terminal_connection_target} ${_openconsole_proxy_target})
set_target_properties(${_terminal_connection_target} PROPERTIES
    OUTPUT_NAME "TerminalConnection"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_terminal_connection_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_terminal_connection_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_terminal_connection_output_root}"
    PDB_OUTPUT_DIRECTORY "${_terminal_connection_output_root}"
)

set(_ui_helpers_target UIHelpers)
set(_ui_helpers_source_root "${_openconsole_root}/src/cascadia/UIHelpers")
set(_ui_helpers_output_root "${_native_product_output_root}/Microsoft.Terminal.UI")
set(_ui_helpers_generation_root "${CMAKE_BINARY_DIR}/ui-helpers/$<CONFIG>")
set(_ui_helpers_unmerged_root "${_ui_helpers_generation_root}/unmerged")
set(_ui_helpers_projection_root "${_ui_helpers_generation_root}/projection")
set(_ui_helpers_winmd "${_ui_helpers_output_root}/Microsoft.Terminal.UI.winmd")
set(_ui_helpers_pri "${_ui_helpers_output_root}/Microsoft.Terminal.UI.pri")
set(_ui_helpers_module_cpp "${_ui_helpers_projection_root}/module.g.cpp")
set(_ui_helpers_projection_header "${_ui_helpers_projection_root}/winrt/Microsoft.Terminal.UI.h")
set(_ui_helpers_mux_header "${_ui_helpers_projection_root}/winrt/Microsoft.UI.Xaml.Controls.h")
set(_ui_helpers_webview2_header "${_ui_helpers_projection_root}/winrt/Microsoft.Web.WebView2.Core.h")
set(_ui_helpers_mux_winmd "${_openconsole_root}/packages/Microsoft.UI.Xaml.2.8.4/lib/uap10.0/Microsoft.UI.Xaml.winmd")
set(_ui_helpers_webview2_winmd "${_openconsole_root}/packages/Microsoft.Web.WebView2.1.0.1661.34/lib/Microsoft.Web.WebView2.Core.winmd")
set(_ui_helpers_idls
    "${_ui_helpers_source_root}/Converters.idl"
    "${_ui_helpers_source_root}/IconPathConverter.idl"
    "${_ui_helpers_source_root}/IDirectKeyListener.idl"
    "${_ui_helpers_source_root}/ResourceString.idl"
    "${_ui_helpers_source_root}/TextMenuFlyout.idl"
)
file(TO_NATIVE_PATH "${_ui_helpers_source_root}" _ui_helpers_source_root_native)
file(TO_NATIVE_PATH "${_ui_helpers_mux_winmd}" _ui_helpers_mux_winmd_native)
set(_ui_helpers_unmerged_winmds)
foreach(_ui_helpers_idl IN LISTS _ui_helpers_idls)
    get_filename_component(_ui_helpers_idl_name "${_ui_helpers_idl}" NAME_WE)
    set(_ui_helpers_unmerged_winmd "${_ui_helpers_unmerged_root}/${_ui_helpers_idl_name}.winmd")
    file(TO_NATIVE_PATH "${_ui_helpers_idl}" _ui_helpers_idl_native)
    file(TO_NATIVE_PATH "${_ui_helpers_unmerged_winmd}" _ui_helpers_unmerged_winmd_native)
    add_custom_command(
        OUTPUT "${_ui_helpers_unmerged_winmd}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ui_helpers_unmerged_root}"
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
            "${_portable_midl_path}"
            /I "${_terminal_connection_sdk_winrt_native}"
            /I "${_terminal_connection_sdk_shared_native}"
            /I "${_terminal_connection_sdk_um_native}"
            /I "${_ui_helpers_source_root_native}"
            ${_native_product_sdk_midl_reference_args}
            /reference "${_ui_helpers_mux_winmd_native}"
            /metadata_dir "${_terminal_connection_metadata_root_native}"
            /winrt /W1 /nologo /char signed /env ${_portable_midl_environment}
            /winmd "${_ui_helpers_unmerged_winmd_native}"
            /h NUL /dlldata NUL /iid NUL /proxy NUL /notlb /client none /server none
            /enum_class /ns_prefix /target NT60 /nomidl
            "${_ui_helpers_idl_native}"
        DEPENDS ${_ui_helpers_idls} ${_native_product_sdk_contract_winmds} "${_ui_helpers_mux_winmd}" "${_portable_midl_path}"
        VERBATIM
        COMMENT "Generating UIHelpers ${_ui_helpers_idl_name}.winmd"
    )
    list(APPEND _ui_helpers_unmerged_winmds "${_ui_helpers_unmerged_winmd}")
endforeach()
add_custom_command(
    OUTPUT "${_ui_helpers_winmd}" "${_ui_helpers_module_cpp}" "${_ui_helpers_projection_header}" "${_ui_helpers_mux_header}" "${_ui_helpers_webview2_header}"
    BYPRODUCTS
        "${_ui_helpers_projection_root}/Converters.g.h" "${_ui_helpers_projection_root}/Converters.g.cpp"
        "${_ui_helpers_projection_root}/IconPathConverter.g.h" "${_ui_helpers_projection_root}/IconPathConverter.g.cpp"
        "${_ui_helpers_projection_root}/ResourceString.g.h" "${_ui_helpers_projection_root}/ResourceString.g.cpp"
        "${_ui_helpers_projection_root}/TextMenuFlyout.g.h" "${_ui_helpers_projection_root}/TextMenuFlyout.g.cpp"
        "${_ui_helpers_projection_root}/winrt/impl/Microsoft.Terminal.UI.0.h"
        "${_ui_helpers_projection_root}/winrt/impl/Microsoft.Terminal.UI.1.h"
        "${_ui_helpers_projection_root}/winrt/impl/Microsoft.Terminal.UI.2.h"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ui_helpers_output_root}" "${_ui_helpers_projection_root}"
    COMMAND "${_portable_mdmerge_path}" -i "${_ui_helpers_unmerged_root}" -o "${_ui_helpers_output_root}"
        ${_native_product_sdk_mdmerge_metadata_args}
        -metadata_dir "${_openconsole_root}/packages/Microsoft.UI.Xaml.2.8.4/lib/uap10.0"
        -partial
    COMMAND "${_cppwinrt_path}" -input "${_ui_helpers_webview2_winmd}" -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}" -output "${_ui_helpers_projection_root}"
    COMMAND "${_cppwinrt_path}" -input "${_ui_helpers_mux_winmd}" -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}" -reference "${_ui_helpers_webview2_winmd}" -output "${_ui_helpers_projection_root}"
    COMMAND "${_cppwinrt_path}"
        -input "${_ui_helpers_winmd}" -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}" -reference "${_ui_helpers_mux_winmd}" -reference "${_ui_helpers_webview2_winmd}"
        -output "${_ui_helpers_projection_root}" -component "${_ui_helpers_projection_root}/sources"
        -overwrite -name Microsoft.Terminal.UI -pch pch.h -prefix -optimize
    DEPENDS ${_ui_helpers_unmerged_winmds} "${_ui_helpers_mux_winmd}" "${_ui_helpers_webview2_winmd}" "${_portable_mdmerge_path}" "${_cppwinrt_path}"
    VERBATIM
    COMMENT "Merging UIHelpers WinMD and generating its component projection"
)

file(GLOB _ui_helpers_resw CONFIGURE_DEPENDS "${_ui_helpers_source_root}/Resources/*/Resources.resw")
set(_ui_helpers_resources_resfiles "")
foreach(_ui_helpers_resource IN LISTS _ui_helpers_resw)
    file(TO_NATIVE_PATH "${_ui_helpers_resource}" _ui_helpers_resource_native)
    string(APPEND _ui_helpers_resources_resfiles "${_ui_helpers_resource_native}\n")
endforeach()
set(_ui_helpers_resources_list "${_ui_helpers_generation_root}/resources.resfiles")
set(_ui_helpers_priconfig "${_ui_helpers_generation_root}/priconfig.xml")
file(TO_NATIVE_PATH "${_ui_helpers_resources_list}" _ui_helpers_resources_list_native)
file(GENERATE OUTPUT "${_ui_helpers_resources_list}" CONTENT "${_ui_helpers_resources_resfiles}")
file(GENERATE OUTPUT "${_ui_helpers_priconfig}" CONTENT
"<?xml version=\"1.0\" encoding=\"utf-8\"?>
<resources targetOsVersion=\"10.0.0\" majorVersion=\"1\">
  <index root=\"\\\" startIndexAt=\"${_ui_helpers_resources_list_native}\">
    <default>
      <qualifier name=\"Language\" value=\"en-US\" /><qualifier name=\"Contrast\" value=\"standard\" />
      <qualifier name=\"Scale\" value=\"200\" /><qualifier name=\"HomeRegion\" value=\"001\" />
      <qualifier name=\"TargetSize\" value=\"256\" /><qualifier name=\"LayoutDirection\" value=\"LTR\" />
      <qualifier name=\"DXFeatureLevel\" value=\"DX9\" /><qualifier name=\"Configuration\" value=\"\" />
      <qualifier name=\"AlternateForm\" value=\"\" /><qualifier name=\"Platform\" value=\"UAP\" />
    </default>
    <indexer-config type=\"RESW\" convertDotsToSlashes=\"true\" initialPath=\"Microsoft.Terminal.UI\" />
    <indexer-config type=\"RESFILES\" qualifierDelimiter=\".\" />
  </index>
</resources>
")
add_custom_command(
    OUTPUT "${_ui_helpers_pri}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ui_helpers_output_root}"
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new /pr "${_ui_helpers_source_root}" /cf "${_ui_helpers_priconfig}" /of "${_ui_helpers_pri}" /in Microsoft.Terminal.UI /o
    DEPENDS ${_ui_helpers_resw} "${_ui_helpers_resources_list}" "${_ui_helpers_priconfig}"
    VERBATIM
    COMMENT "Generating Microsoft.Terminal.UI.pri"
)
set(_ui_helpers_svg_stamp "${_ui_helpers_generation_root}/workspace-svg.stamp")
add_custom_command(
    OUTPUT "${_ui_helpers_svg_stamp}"
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${_workspace_root}/tools/Generate-WorkspaceSvgIcons.ps1" -OutputRoot "${_workspace_root}/bin/res/workspace-icons"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_workspace_root}/bin/res/workspace-icons" "${_ui_helpers_output_root}/res/workspace-icons"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_ui_helpers_svg_stamp}"
    DEPENDS "${_workspace_root}/tools/Generate-WorkspaceSvgIcons.ps1"
    VERBATIM
    COMMENT "Generating and copying UIHelpers workspace SVG resources"
)

set_source_files_properties("${_ui_helpers_winmd}" "${_ui_helpers_pri}" "${_ui_helpers_projection_header}" "${_ui_helpers_mux_header}" "${_ui_helpers_svg_stamp}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_library(${_ui_helpers_target} SHARED
    "${_ui_helpers_source_root}/init.cpp" "${_ui_helpers_source_root}/pch.cpp"
    "${_ui_helpers_source_root}/Converters.cpp" "${_ui_helpers_source_root}/IconPathConverter.cpp"
    "${_ui_helpers_source_root}/ResourceString.cpp" "${_ui_helpers_source_root}/TextMenuFlyout.cpp"
    "${_ui_helpers_module_cpp}" "${_ui_helpers_source_root}/Microsoft.Terminal.UI.def"
    "${_ui_helpers_winmd}" "${_ui_helpers_pri}" "${_ui_helpers_svg_stamp}" "${_native_cppwinrt_projection_base}"
)
target_compile_features(${_ui_helpers_target} PRIVATE cxx_std_20)
target_precompile_headers(${_ui_helpers_target} PRIVATE "${_ui_helpers_source_root}/pch.h")
target_include_directories(${_ui_helpers_target} PRIVATE
    "${_ui_helpers_projection_root}" "${_native_cppwinrt_projection_root}" "${_wil_package_dir}/include" "${_ui_helpers_source_root}"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/cascadia/inc" "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include" ${_portable_msvc_include_dirs}
)
target_link_directories(${_ui_helpers_target} PRIVATE "${_workspace_extension_vcpkg_lib_root}" "${_workspace_extension_vcpkg_manual_lib_root}" ${_portable_msvc_lib_dirs})
target_compile_definitions(${_ui_helpers_target} PRIVATE
    _USRDLL _WINDLL _WINRT_DLL _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__
)
target_compile_options(${_ui_helpers_target} PRIVATE
    ${_native_product_compile_options} /Zc:preprocessor /bigobj
    /wd5104 /wd28204 /wd4201 /wd4312 /wd5105 /wd26434 /wd26445 /wd26456 /wd26478 /wd26494
    "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}"
)
target_link_libraries(${_ui_helpers_target} PRIVATE
    ConTypes WinRTUtils "${_workspace_extension_vcpkg_lib_root}/fmt.lib"
    onecoreuap_apiset.lib d3dcompiler.lib dwmapi.lib uxtheme.lib shlwapi.lib ntdll.lib WindowsApp.lib user32.lib shell32.lib
)
target_link_options(${_ui_helpers_target} PRIVATE /SUBSYSTEM:WINDOWS /MANIFEST:NO /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT)
set_target_properties(${_ui_helpers_target} PROPERTIES
    OUTPUT_NAME "Microsoft.Terminal.UI" MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_ui_helpers_output_root}" LIBRARY_OUTPUT_DIRECTORY "${_ui_helpers_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_ui_helpers_output_root}" PDB_OUTPUT_DIRECTORY "${_ui_helpers_output_root}"
)

set(_xaml_compiler_host_source "${_openconsole_root}/tools/XamlCompilerHost/XamlCompilerHost.cs")
set(_xaml_compiler_host_root "${CMAKE_BINARY_DIR}/xaml-compiler-host")
set(_xaml_compiler_host "${_xaml_compiler_host_root}/XamlCompilerHost.exe")
set(_xaml_compiler_task "${_portable_sdk_root}/bin/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/XamlCompiler/Microsoft.Windows.UI.Xaml.Build.Tasks.dll")
set(_xaml_framework_runtime_dir "C:/Windows/Microsoft.NET/Framework64/v4.0.30319")
set(_xaml_framework_reference "${_xaml_framework_runtime_dir}/Microsoft.Build.Framework.dll")
set(_xaml_compiler_host_rsp "${_xaml_compiler_host_root}/XamlCompilerHost.rsp")
set(_xaml_compiler_host_rsp_content "/target:exe\n/optimize+\n/langversion:latest\n/out:${_xaml_compiler_host}\n/reference:${_xaml_framework_reference}\n")
foreach(_xaml_reference IN LISTS _net472_references)
    string(APPEND _xaml_compiler_host_rsp_content "/reference:${_xaml_reference}\n")
endforeach()
string(APPEND _xaml_compiler_host_rsp_content "${_xaml_compiler_host_source}\n")
file(GENERATE OUTPUT "${_xaml_compiler_host_rsp}" CONTENT "${_xaml_compiler_host_rsp_content}")
file(GLOB_RECURSE _xaml_sdk_contract_winmds CONFIGURE_DEPENDS "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/*.winmd")
add_custom_command(
    OUTPUT "${_xaml_compiler_host}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_xaml_compiler_host_root}"
    COMMAND "${_dotnet_path}" "${_dotnet_csc_path}" /noconfig "@${_xaml_compiler_host_rsp}"
    DEPENDS "${_xaml_compiler_host_source}" "${_xaml_compiler_host_rsp}" "${_dotnet_csc_path}" "${_xaml_framework_reference}" ${_net472_references}
    VERBATIM
    COMMENT "Building the direct Windows SDK XAML compiler host"
)

set(_terminal_control_lib_target TerminalControlLib)
set(_terminal_control_target TerminalControl)
set(_terminal_control_source_root "${_openconsole_root}/src/cascadia/TerminalControl")
set(_terminal_control_lib_output_root "${_native_product_output_root}/Microsoft.Terminal.Control.Lib")
set(_terminal_control_output_root "${_native_product_output_root}/Microsoft.Terminal.Control")
set(_terminal_control_generation_root "${CMAKE_BINARY_DIR}/terminal-control/$<CONFIG>")
set(_terminal_control_unmerged_root "${_terminal_control_generation_root}/unmerged")
set(_terminal_control_projection_root "${_terminal_control_generation_root}/projection")
set(_terminal_control_component_root "${_terminal_control_projection_root}/sources")
set(_terminal_control_reference_projection_root "${_terminal_control_generation_root}/references")
set(_terminal_control_reference_projection_header "${_terminal_control_reference_projection_root}/winrt/Microsoft.Terminal.UI.h")
set(_terminal_control_winmd "${_terminal_control_lib_output_root}/Microsoft.Terminal.Control.winmd")
set(_terminal_control_dll_winmd "${_terminal_control_output_root}/Microsoft.Terminal.Control.winmd")
set(_terminal_control_module_cpp "${_terminal_control_projection_root}/module.g.cpp")
set(_terminal_control_projection_header "${_terminal_control_projection_root}/winrt/Microsoft.Terminal.Control.h")

add_custom_command(
    OUTPUT "${_terminal_control_reference_projection_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_control_reference_projection_root}"
    COMMAND "${_cppwinrt_path}"
        -input "${_terminal_core_winmd}" -input "${_terminal_connection_winmd}" -input "${_ui_helpers_winmd}"
        -input "${_ui_helpers_mux_winmd}" -input "${_ui_helpers_webview2_winmd}"
        -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -output "${_terminal_control_reference_projection_root}"
    DEPENDS "${_terminal_core_winmd}" "${_terminal_connection_winmd}" "${_ui_helpers_winmd}"
        "${_ui_helpers_mux_winmd}" "${_ui_helpers_webview2_winmd}" "${_cppwinrt_path}"
    VERBATIM
    COMMENT "Generating TerminalControl dependency reference projection"
)
set(_terminal_control_xaml_metadata_idl "${_terminal_control_generation_root}/XamlMetaDataProvider.idl")
file(GENERATE OUTPUT "${_terminal_control_xaml_metadata_idl}" CONTENT
"// Generated by CMake for the Windows XAML metadata provider.\nnamespace Microsoft.Terminal.Control\n{\n    runtimeclass XamlMetaDataProvider : [default] Windows.UI.Xaml.Markup.IXamlMetadataProvider\n    {\n        XamlMetaDataProvider();\n    }\n}\n")
set(_terminal_control_idls
    "${_terminal_control_source_root}/ControlCore.idl"
    "${_terminal_control_source_root}/ControlInteractivity.idl"
    "${_terminal_control_source_root}/ScrollBarVisualStateManager.idl"
    "${_terminal_control_source_root}/ICoreState.idl"
    "${_terminal_control_source_root}/KeyChord.idl"
    "${_terminal_control_source_root}/EventArgs.idl"
    "${_terminal_control_source_root}/IKeyBindings.idl"
    "${_terminal_control_source_root}/IControlSettings.idl"
    "${_terminal_control_source_root}/IControlAppearance.idl"
    "${_terminal_control_source_root}/SearchBoxControl.idl"
    "${_terminal_control_source_root}/XamlLights.idl"
    "${_terminal_control_source_root}/TermControl.idl"
    "${_terminal_control_source_root}/TermControlAutomationPeer.idl"
    "${_terminal_control_source_root}/InteractivityAutomationPeer.idl"
    "${_terminal_control_source_root}/IMouseWheelListener.idl"
    "${_terminal_control_xaml_metadata_idl}"
)
set(_terminal_control_product_reference_winmds
    "${_terminal_core_winmd}"
    "${_terminal_connection_winmd}"
    "${_ui_helpers_winmd}"
    "${_ui_helpers_mux_winmd}"
    "${_ui_helpers_webview2_winmd}"
)
set(_terminal_control_xaml_sdk_contract_names
    Windows.AI.MachineLearning.MachineLearningContract
    Windows.AI.MachineLearning.Preview.MachineLearningPreviewContract
    Windows.ApplicationModel.Calls.Background.CallsBackgroundContract
    Windows.ApplicationModel.Calls.CallsPhoneContract
    Windows.ApplicationModel.Calls.CallsVoipContract
    Windows.ApplicationModel.CommunicationBlocking.CommunicationBlockingContract
    Windows.ApplicationModel.SocialInfo.SocialInfoContract
    Windows.ApplicationModel.StartupTaskContract
    Windows.Devices.Custom.CustomDeviceContract
    Windows.Devices.DevicesLowLevelContract
    Windows.Devices.Printers.PrintersContract
    Windows.Devices.SmartCards.SmartCardBackgroundTriggerContract
    Windows.Devices.SmartCards.SmartCardEmulatorContract
    Windows.Foundation.FoundationContract
    Windows.Foundation.UniversalApiContract
    Windows.Gaming.XboxLive.StorageApiContract
    Windows.Graphics.Printing3D.Printing3DContract
    Windows.Networking.Connectivity.WwanContract
    Windows.Networking.Sockets.ControlChannelTriggerContract
    Windows.Security.Isolation.Isolatedwindowsenvironmentcontract
    Windows.Services.Maps.GuidanceContract
    Windows.Services.Maps.LocalSearchContract
    Windows.Services.Store.StoreContract
    Windows.Services.TargetedContent.TargetedContentContract
    Windows.Storage.Provider.CloudFilesContract
    Windows.System.Profile.ProfileHardwareTokenContract
    Windows.System.Profile.ProfileRetailInfoContract
    Windows.System.Profile.ProfileSharedModeContract
    Windows.System.Profile.SystemManufacturers.SystemManufacturersContract
    Windows.System.SystemManagementContract
    Windows.UI.UIAutomation.UIAutomationContract
    Windows.UI.ViewManagement.ViewManagementViewScalingContract
    Windows.UI.Xaml.Core.Direct.XamlDirectContract
)
set(_terminal_control_xaml_sdk_contract_winmds)
foreach(_terminal_control_sdk_contract IN LISTS _xaml_sdk_contract_winmds)
    get_filename_component(_terminal_control_sdk_contract_name "${_terminal_control_sdk_contract}" NAME_WLE)
    if(_terminal_control_sdk_contract_name IN_LIST _terminal_control_xaml_sdk_contract_names)
        list(APPEND _terminal_control_xaml_sdk_contract_winmds "${_terminal_control_sdk_contract}")
    endif()
endforeach()
list(LENGTH _terminal_control_xaml_sdk_contract_winmds _terminal_control_xaml_sdk_contract_count)
if(NOT _terminal_control_xaml_sdk_contract_count EQUAL 33)
    message(FATAL_ERROR "TerminalControl requires exactly 33 resolved Windows SDK XAML contracts; found ${_terminal_control_xaml_sdk_contract_count}")
endif()
set(_terminal_control_reference_winmds
    ${_terminal_control_product_reference_winmds}
    ${_terminal_control_xaml_sdk_contract_winmds}
)
set(_terminal_control_midl_metadata_root "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Windows.Foundation.FoundationContract/4.0.0.0")
file(TO_NATIVE_PATH "${_terminal_control_midl_metadata_root}" _terminal_control_midl_metadata_root_native)
file(TO_NATIVE_PATH "${_terminal_control_source_root}" _terminal_control_source_root_native)
set(_terminal_control_midl_reference_args)
set(_terminal_control_mdmerge_metadata_args)
foreach(_terminal_control_reference IN LISTS _terminal_control_reference_winmds)
    file(TO_NATIVE_PATH "${_terminal_control_reference}" _terminal_control_reference_native)
    list(APPEND _terminal_control_midl_reference_args /reference "${_terminal_control_reference_native}")
    get_filename_component(_terminal_control_reference_dir "${_terminal_control_reference}" DIRECTORY)
    list(APPEND _terminal_control_mdmerge_metadata_args -metadata_dir "${_terminal_control_reference_dir}")
endforeach()
set(_terminal_control_unmerged_winmds)
foreach(_terminal_control_idl IN LISTS _terminal_control_idls)
    get_filename_component(_terminal_control_idl_name "${_terminal_control_idl}" NAME_WE)
    set(_terminal_control_unmerged_winmd "${_terminal_control_unmerged_root}/${_terminal_control_idl_name}.winmd")
    file(TO_NATIVE_PATH "${_terminal_control_idl}" _terminal_control_idl_native)
    file(TO_NATIVE_PATH "${_terminal_control_unmerged_winmd}" _terminal_control_unmerged_winmd_native)
    add_custom_command(
        OUTPUT "${_terminal_control_unmerged_winmd}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_control_unmerged_root}"
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
            "${_portable_midl_path}"
            /I "${_terminal_connection_sdk_winrt_native}" /I "${_terminal_connection_sdk_shared_native}"
            /I "${_terminal_connection_sdk_um_native}" /I "${_terminal_control_source_root_native}"
            ${_terminal_control_midl_reference_args}
            /metadata_dir "${_terminal_control_midl_metadata_root_native}"
            /winrt /W1 /nologo /char signed /env ${_portable_midl_environment}
            /winmd "${_terminal_control_unmerged_winmd_native}"
            /h NUL /dlldata NUL /iid NUL /proxy NUL /notlb /client none /server none
            /enum_class /ns_prefix /target NT60 /nomidl "${_terminal_control_idl_native}"
        DEPENDS ${_terminal_control_idls} ${_terminal_control_reference_winmds} "${_portable_midl_path}"
        VERBATIM
        COMMENT "Generating TerminalControl ${_terminal_control_idl_name}.winmd"
    )
    list(APPEND _terminal_control_unmerged_winmds "${_terminal_control_unmerged_winmd}")
endforeach()
add_custom_command(
    OUTPUT "${_terminal_control_winmd}" "${_terminal_control_module_cpp}" "${_terminal_control_projection_header}"
    BYPRODUCTS "${_terminal_control_component_root}/XamlMetaDataProvider.cpp"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_control_lib_output_root}" "${_terminal_control_projection_root}"
    COMMAND "${_portable_mdmerge_path}" -i "${_terminal_control_unmerged_root}" -o "${_terminal_control_lib_output_root}"
        ${_terminal_control_mdmerge_metadata_args}
        -partial
    COMMAND "${_cppwinrt_path}"
        -input "${_terminal_control_winmd}" -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -reference "${_terminal_core_winmd}" -reference "${_terminal_connection_winmd}" -reference "${_ui_helpers_winmd}"
        -reference "${_ui_helpers_mux_winmd}" -reference "${_ui_helpers_webview2_winmd}"
        -output "${_terminal_control_projection_root}" -component "${_terminal_control_component_root}"
        -overwrite -name Microsoft.Terminal.Control -pch pch.h -prefix -optimize
    DEPENDS ${_terminal_control_unmerged_winmds} ${_terminal_control_reference_winmds} "${_portable_mdmerge_path}" "${_cppwinrt_path}"
    VERBATIM
    COMMENT "Merging TerminalControl WinMD and generating its component projection"
)

set(_terminal_control_xaml_pages
    "${_terminal_control_source_root}/SearchBoxControl.xaml"
    "${_terminal_control_source_root}/TermControl.xaml"
)
set(_terminal_control_xaml_headers
    "${_terminal_control_source_root}/pch.h"
    "${_terminal_control_source_root}/ControlCore.h"
    "${_terminal_control_source_root}/ControlInteractivity.h"
    "${_terminal_control_source_root}/ScrollBarVisualStateManager.h"
    "${_terminal_control_source_root}/KeyChord.h"
    "${_terminal_control_source_root}/EventArgs.h"
    "${_terminal_control_source_root}/SearchBoxControl.h"
    "${_terminal_control_source_root}/XamlLights.h"
    "${_terminal_control_source_root}/TermControl.h"
    "${_terminal_control_source_root}/TermControlAutomationPeer.h"
    "${_terminal_control_source_root}/InteractivityAutomationPeer.h"
    "${_terminal_control_source_root}/XamlUiaTextRange.h"
    "${_terminal_control_source_root}/HwndTerminal.hpp"
    "${_terminal_control_source_root}/HwndTerminalAutomationPeer.hpp"
)
set(_terminal_control_xaml_product_reference_args)
foreach(_terminal_control_reference IN LISTS _terminal_control_product_reference_winmds)
    list(APPEND _terminal_control_xaml_product_reference_args --winmd-reference "${_terminal_control_reference}")
endforeach()
set(_terminal_control_xaml_sdk_reference_args)
foreach(_terminal_control_reference IN LISTS _terminal_control_xaml_sdk_contract_winmds)
    list(APPEND _terminal_control_xaml_sdk_reference_args --system-reference "${_terminal_control_reference}")
endforeach()
set(_terminal_control_xaml_facade_winmd
    "${_portable_sdk_root}/UnionMetadata/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Facade/Windows.winmd")
set(_terminal_control_xaml_page_args)
foreach(_terminal_control_page IN LISTS _terminal_control_xaml_pages)
    list(APPEND _terminal_control_xaml_page_args --page "${_terminal_control_page}")
endforeach()
set(_terminal_control_xaml_header_args)
set(_terminal_control_xaml_idl_header_names
    ControlCore ControlInteractivity ScrollBarVisualStateManager KeyChord EventArgs XamlLights
    TermControlAutomationPeer InteractivityAutomationPeer
)
foreach(_terminal_control_header IN LISTS _terminal_control_xaml_headers)
    get_filename_component(_terminal_control_header_file "${_terminal_control_header}" NAME)
    get_filename_component(_terminal_control_header_name "${_terminal_control_header}" NAME_WE)
    set(_terminal_control_header_parent "")
    if(_terminal_control_header_name STREQUAL "SearchBoxControl" OR _terminal_control_header_name STREQUAL "TermControl")
        set(_terminal_control_header_parent "${_terminal_control_header_name}.xaml")
    elseif(_terminal_control_header_name IN_LIST _terminal_control_xaml_idl_header_names)
        set(_terminal_control_header_parent "${_terminal_control_header_name}.idl")
    endif()
    list(APPEND _terminal_control_xaml_header_args --cl-include "${_terminal_control_header_file}#${_terminal_control_header_parent}#${_terminal_control_header}")
endforeach()
set(_terminal_control_xaml_common_args
    --framework-runtime-dir "${_xaml_framework_runtime_dir}"
    --task-assembly "${_xaml_compiler_task}"
    --language CppWinRT
    --output "${_terminal_control_projection_root}"
    --output-type staticlibrary
    --root-namespace Microsoft.Terminal.Control
    --windows-sdk-path "${_portable_sdk_root}"
    --vc-install-dir "${_portable_vc_root}"
    --target-platform-min-version 10.0.18362.0
    --project-path "${_terminal_control_source_root}/CMakeLists.txt"
    --project-name TerminalControlLib
    --pri-index-name Microsoft.Terminal.Control
    ${_terminal_control_xaml_page_args}
    --include "${_terminal_control_source_root}"
)
set(_terminal_control_xaml_typeinfo "${_terminal_control_projection_root}/XamlTypeInfo.g.cpp")
set(_terminal_control_xaml_typeinfo_impl "${_terminal_control_projection_root}/XamlTypeInfo.Impl.g.cpp")
set(_terminal_control_xaml_provider_cpp "${_terminal_control_projection_root}/XamlMetaDataProvider.cpp")
file(GENERATE OUTPUT "${_terminal_control_xaml_provider_cpp}" CONTENT
"// This file is generated by the build to support Xaml apps\n#include \"pch.h\"\n#include \"XamlMetaDataProvider.h\"\n#include \"XamlMetaDataProvider.g.cpp\"\n")
set(_terminal_control_search_xbf "${_terminal_control_projection_root}/SearchBoxControl.xbf")
set(_terminal_control_term_xbf "${_terminal_control_projection_root}/TermControl.xbf")
add_custom_command(
    OUTPUT
        "${_terminal_control_projection_root}/SearchBoxControl.xaml.g.h"
        "${_terminal_control_projection_root}/TermControl.xaml.g.h"
        "${_terminal_control_xaml_typeinfo}" "${_terminal_control_xaml_typeinfo_impl}"
        "${_terminal_control_search_xbf}" "${_terminal_control_term_xbf}"
    BYPRODUCTS
        "${_terminal_control_projection_root}/XamlBindingInfo.xaml.g.h"
        "${_terminal_control_projection_root}/XamlBindingInfo.xaml.g.hpp"
        "${_terminal_control_projection_root}/XamlTypeInfo.xaml.g.h"
        "${_terminal_control_projection_root}/XamlMetaDataProvider.h"
        "${_terminal_control_projection_root}/SearchBoxControl.xaml.g.hpp"
        "${_terminal_control_projection_root}/TermControl.xaml.g.hpp"
        "${_terminal_control_projection_root}/SearchBoxControl.xaml"
        "${_terminal_control_projection_root}/TermControl.xaml"
        "${_terminal_control_projection_root}/XamlSaveStateFile.xml"
    COMMAND "${_xaml_compiler_host}" ${_terminal_control_xaml_common_args}
        ${_terminal_control_xaml_product_reference_args} ${_terminal_control_xaml_sdk_reference_args}
        --reference "${_terminal_control_xaml_facade_winmd}"
        --compile-mode RealBuildPass1
    COMMAND "${_xaml_compiler_host}" ${_terminal_control_xaml_common_args}
        ${_terminal_control_xaml_product_reference_args} ${_terminal_control_xaml_sdk_reference_args}
        --reference "${_terminal_control_xaml_facade_winmd}"
        --compile-mode RealBuildPass2
        --platform-xml-dir "${_portable_sdk_root}/Platforms/UAP/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        --local-assembly "${_terminal_control_winmd}"
        ${_terminal_control_xaml_header_args}
    DEPENDS "${_xaml_compiler_host}" "${_terminal_control_winmd}" ${_terminal_control_xaml_pages} ${_terminal_control_xaml_headers}
        ${_terminal_control_product_reference_winmds} ${_terminal_control_xaml_sdk_contract_winmds}
        "${_terminal_control_xaml_facade_winmd}"
    VERBATIM
    COMMENT "Generating TerminalControl XAML code and XBF without MSBuild"
)
add_custom_target(TerminalControlXaml DEPENDS
    "${_terminal_control_xaml_typeinfo}" "${_terminal_control_xaml_typeinfo_impl}"
    "${_terminal_control_search_xbf}" "${_terminal_control_term_xbf}"
    "${_terminal_control_projection_root}/SearchBoxControl.xaml.g.hpp"
    "${_terminal_control_projection_root}/TermControl.xaml.g.hpp"
)

set(_terminal_control_pri "${_terminal_control_lib_output_root}/Microsoft.Terminal.Control.pri")
set(_terminal_control_pri_root "${_terminal_control_generation_root}/pri-root")
file(GLOB _terminal_control_resw CONFIGURE_DEPENDS "${_terminal_control_source_root}/Resources/*/Resources.resw")
set(_terminal_control_resources_resfiles "")
foreach(_terminal_control_resource IN LISTS _terminal_control_resw)
    file(TO_NATIVE_PATH "${_terminal_control_resource}" _terminal_control_resource_native)
    string(APPEND _terminal_control_resources_resfiles "${_terminal_control_resource_native}\n")
endforeach()
set(_terminal_control_staged_search_xbf "${_terminal_control_pri_root}/Microsoft.Terminal.Control/SearchBoxControl.xbf")
set(_terminal_control_staged_term_xbf "${_terminal_control_pri_root}/Microsoft.Terminal.Control/TermControl.xbf")
set(_terminal_control_resources_list "${_terminal_control_generation_root}/resources.resfiles")
set(_terminal_control_embed_list "${_terminal_control_pri_root}/embed.resfiles")
set(_terminal_control_priconfig "${_terminal_control_generation_root}/priconfig.xml")
file(TO_NATIVE_PATH "${_terminal_control_resources_list}" _terminal_control_resources_list_native)
file(GENERATE OUTPUT "${_terminal_control_resources_list}" CONTENT "${_terminal_control_resources_resfiles}")
file(GENERATE OUTPUT "${_terminal_control_embed_list}" CONTENT "Microsoft.Terminal.Control\\SearchBoxControl.xbf\nMicrosoft.Terminal.Control\\TermControl.xbf\n")
file(GENERATE OUTPUT "${_terminal_control_priconfig}" CONTENT
"<?xml version=\"1.0\" encoding=\"utf-8\"?>
<resources targetOsVersion=\"10.0.0\" majorVersion=\"1\">
  <index root=\"\\\" startIndexAt=\"${_terminal_control_resources_list_native}\">
    <default>
      <qualifier name=\"Language\" value=\"en-US\" /><qualifier name=\"Contrast\" value=\"standard\" />
      <qualifier name=\"Scale\" value=\"200\" /><qualifier name=\"HomeRegion\" value=\"001\" />
      <qualifier name=\"TargetSize\" value=\"256\" /><qualifier name=\"LayoutDirection\" value=\"LTR\" />
      <qualifier name=\"DXFeatureLevel\" value=\"DX9\" /><qualifier name=\"Configuration\" value=\"\" />
      <qualifier name=\"AlternateForm\" value=\"\" /><qualifier name=\"Platform\" value=\"UAP\" />
    </default>
    <indexer-config type=\"RESW\" convertDotsToSlashes=\"true\" initialPath=\"Microsoft.Terminal.Control\" />
    <indexer-config type=\"RESFILES\" qualifierDelimiter=\".\" />
  </index>
  <index root=\"${_terminal_control_pri_root}\" startIndexAt=\"embed.resfiles\">
    <default>
      <qualifier name=\"Language\" value=\"en-US\" /><qualifier name=\"Contrast\" value=\"standard\" />
      <qualifier name=\"Scale\" value=\"200\" /><qualifier name=\"HomeRegion\" value=\"001\" />
      <qualifier name=\"TargetSize\" value=\"256\" /><qualifier name=\"LayoutDirection\" value=\"LTR\" />
      <qualifier name=\"DXFeatureLevel\" value=\"DX9\" /><qualifier name=\"Configuration\" value=\"\" />
      <qualifier name=\"AlternateForm\" value=\"\" /><qualifier name=\"Platform\" value=\"UAP\" />
    </default>
    <indexer-config type=\"RESFILES\" qualifierDelimiter=\".\" />
    <indexer-config type=\"EMBEDFILES\" />
  </index>
</resources>
")
add_custom_command(
    OUTPUT "${_terminal_control_pri}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_control_lib_output_root}" "${_terminal_control_pri_root}/Microsoft.Terminal.Control"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_terminal_control_search_xbf}" "${_terminal_control_staged_search_xbf}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_terminal_control_term_xbf}" "${_terminal_control_staged_term_xbf}"
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new /pr "${_terminal_control_pri_root}" /cf "${_terminal_control_priconfig}" /of "${_terminal_control_pri}" /in Microsoft.Terminal.Control /o
    DEPENDS ${_terminal_control_resw} "${_terminal_control_search_xbf}" "${_terminal_control_term_xbf}"
        "${_terminal_control_resources_list}" "${_terminal_control_embed_list}" "${_terminal_control_priconfig}"
    VERBATIM
    COMMENT "Generating Microsoft.Terminal.Control.pri"
)

set(_terminal_control_sources
    "${_terminal_control_source_root}/pch.cpp"
    "${_terminal_control_source_root}/ControlCore.cpp"
    "${_terminal_control_source_root}/ControlInteractivity.cpp"
    "${_terminal_control_source_root}/ScrollBarVisualStateManager.cpp"
    "${_terminal_control_source_root}/EventArgs.cpp"
    "${_terminal_control_source_root}/init.cpp"
    "${_terminal_control_source_root}/KeyChord.cpp"
    "${_terminal_control_source_root}/SearchBoxControl.cpp"
    "${_terminal_control_source_root}/XamlLights.cpp"
    "${_terminal_control_source_root}/TermControl.cpp"
    "${_terminal_control_source_root}/TermControlAutomationPeer.cpp"
    "${_terminal_control_source_root}/InteractivityAutomationPeer.cpp"
    "${_terminal_control_source_root}/XamlUiaTextRange.cpp"
    "${_terminal_control_source_root}/HwndTerminal.cpp"
    "${_terminal_control_source_root}/HwndTerminalAutomationPeer.cpp"
    "${_terminal_control_module_cpp}"
    "${_terminal_control_xaml_provider_cpp}"
    "${_terminal_control_xaml_typeinfo}"
    "${_terminal_control_xaml_typeinfo_impl}"
)
set_source_files_properties(
    "${_terminal_control_module_cpp}" "${_terminal_control_xaml_provider_cpp}"
    "${_terminal_control_xaml_typeinfo}" "${_terminal_control_xaml_typeinfo_impl}"
    "${_terminal_control_search_xbf}" "${_terminal_control_term_xbf}"
    PROPERTIES GENERATED TRUE
)
set_source_files_properties("${_terminal_control_pri}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_library(${_terminal_control_lib_target} STATIC ${_terminal_control_sources} "${_terminal_control_search_xbf}" "${_terminal_control_term_xbf}" "${_terminal_control_pri}" "${_terminal_control_reference_projection_header}")
add_dependencies(${_terminal_control_lib_target} TerminalControlXaml)
target_compile_features(${_terminal_control_lib_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_control_lib_target} PRIVATE "${_terminal_control_source_root}/pch.h")
target_include_directories(${_terminal_control_lib_target} PRIVATE
    "${_terminal_control_reference_projection_root}" "${_terminal_control_projection_root}" "${_terminal_control_source_root}"
    "${_terminal_core_projection_root}" "${_terminal_connection_projection_root}" "${_ui_helpers_projection_root}"
    "${_native_cppwinrt_projection_root}"
    "${_wil_package_dir}/include" "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/types/inc" "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include" ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_terminal_control_lib_target} PRIVATE
    _LIB _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__
)
target_compile_options(${_terminal_control_lib_target} PRIVATE
    ${_native_product_archive_compile_options} /W3 /Zc:preprocessor /bigobj /wd4702
    /wd4471 /wd5104 /wd28204 /wd4201 /wd4312 /wd5105 /wd26434 /wd26445 /wd26456 /wd26478 /wd26494
    "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}"
)
set_target_properties(${_terminal_control_lib_target} PROPERTIES
    OUTPUT_NAME "Microsoft.Terminal.ControlLib" MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE ARCHIVE_OUTPUT_DIRECTORY "${_terminal_control_lib_output_root}"
)

add_library(${_terminal_control_target} SHARED "${_terminal_control_source_root}/dll/pch.cpp" "${_terminal_control_source_root}/dll/Microsoft.Terminal.Control.def")
target_compile_features(${_terminal_control_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_control_target} PRIVATE "${_terminal_control_source_root}/dll/pch.h")
target_include_directories(${_terminal_control_target} PRIVATE
    "${_terminal_control_reference_projection_root}" "${_terminal_control_projection_root}" "${_terminal_control_source_root}"
    "${_terminal_core_projection_root}" "${_terminal_connection_projection_root}" "${_ui_helpers_projection_root}"
    "${_native_cppwinrt_projection_root}"
    "${_wil_package_dir}/include" "${_terminal_control_source_root}/dll"
    "${_openconsole_root}/src/cascadia/inc" "${_openconsole_root}/src/types/inc" "${_openconsole_root}/src/cascadia/WinRTUtils/inc"
    "${_openconsole_root}/src/inc" "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include" ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_terminal_control_target} PRIVATE
    _USRDLL _WINDLL _WINRT_DLL _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__
)
target_compile_options(${_terminal_control_target} PRIVATE
    ${_native_product_compile_options} /W3 /Zc:preprocessor /bigobj
    /wd4471 /wd5104 /wd28204 /wd4201 /wd4312 /wd5105 /wd26434 /wd26445 /wd26456 /wd26478 /wd26494
    "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}"
)
target_link_directories(${_terminal_control_target} PRIVATE "${_workspace_extension_vcpkg_lib_root}" "${_workspace_extension_vcpkg_manual_lib_root}" ${_portable_msvc_lib_dirs})
target_link_libraries(${_terminal_control_target} PRIVATE
    ${_terminal_control_lib_target} ${_ui_helpers_target} TerminalCore ConTypes WinRTUtils ConRenderAtlas TerminalInput ConTermAdapt MidiAudio
    ConRenderUia ConTSF ConRenderBase ConBufferOut ConTermParser "${_workspace_extension_vcpkg_lib_root}/fmt.lib"
    delayimp.lib Uiautomationcore.lib onecoreuap.lib onecoreuap_apiset.lib d3dcompiler.lib dwmapi.lib uxtheme.lib shlwapi.lib ntdll.lib WindowsApp.lib
)
target_link_options(${_terminal_control_target} PRIVATE
    /SUBSYSTEM:WINDOWS /MANIFEST:NO /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT
    "/INCLUDE:${_portable_dllmain_linker_symbol}" /DELAYLOAD:uiautomationcore.dll /DELAYLOAD:icu.dll
)
add_custom_command(TARGET ${_terminal_control_target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_terminal_control_winmd}" "${_terminal_control_dll_winmd}"
)
set_target_properties(${_terminal_control_target} PROPERTIES
    OUTPUT_NAME "Microsoft.Terminal.Control" MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_terminal_control_output_root}" LIBRARY_OUTPUT_DIRECTORY "${_terminal_control_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_terminal_control_output_root}" PDB_OUTPUT_DIRECTORY "${_terminal_control_output_root}"
)

set(_terminal_settings_model_lib_target TerminalSettingsModelLib)
set(_terminal_settings_model_target TerminalSettingsModel)
set(_terminal_settings_model_source_root "${_openconsole_root}/src/cascadia/TerminalSettingsModel")
set(_terminal_settings_model_lib_output_root "${_native_product_output_root}/Microsoft.Terminal.Settings.Model.Lib")
set(_terminal_settings_model_output_root "${_native_product_output_root}/Microsoft.Terminal.Settings.Model")
set(_terminal_settings_model_generation_root "${CMAKE_BINARY_DIR}/terminal-settings-model/$<CONFIG>")
set(_terminal_settings_model_unmerged_root "${_terminal_settings_model_generation_root}/unmerged")
set(_terminal_settings_model_projection_root "${_terminal_settings_model_generation_root}/projection")
set(_terminal_settings_model_component_root "${_terminal_settings_model_projection_root}/sources")
set(_terminal_settings_model_reference_root "${_terminal_settings_model_generation_root}/references")
set(_terminal_settings_model_winmd "${_terminal_settings_model_lib_output_root}/Microsoft.Terminal.Settings.Model.winmd")
set(_terminal_settings_model_dll_winmd "${_terminal_settings_model_output_root}/Microsoft.Terminal.Settings.Model.winmd")
set(_terminal_settings_model_module_cpp "${_terminal_settings_model_projection_root}/module.g.cpp")
set(_terminal_settings_model_projection_header "${_terminal_settings_model_projection_root}/winrt/Microsoft.Terminal.Settings.Model.h")
set(_terminal_settings_model_reference_header "${_terminal_settings_model_reference_root}/winrt/Microsoft.Terminal.Control.h")
set(_terminal_settings_model_idls
    ActionArgs.idl ActionMap.idl ApplicationState.idl CascadiaSettings.idl ColorScheme.idl NewTabMenuEntry.idl
    Theme.idl Command.idl DefaultTerminal.idl GlobalAppSettings.idl Profile.idl EnumMappings.idl TerminalWarnings.idl
    KeyChordSerialization.idl AppearanceConfig.idl IAppearanceConfig.idl ISettingsModelObject.idl FontConfig.idl
)
list(TRANSFORM _terminal_settings_model_idls PREPEND "${_terminal_settings_model_source_root}/")
set(_terminal_settings_model_reference_winmds
    "${_terminal_control_winmd}" "${_terminal_core_winmd}" "${_terminal_connection_winmd}"
    "${_ui_helpers_mux_winmd}" "${_ui_helpers_webview2_winmd}" ${_terminal_control_xaml_sdk_contract_winmds}
)
set(_terminal_settings_model_midl_reference_args)
set(_terminal_settings_model_mdmerge_metadata_args)
foreach(_terminal_settings_model_reference IN LISTS _terminal_settings_model_reference_winmds)
    file(TO_NATIVE_PATH "${_terminal_settings_model_reference}" _terminal_settings_model_reference_native)
    list(APPEND _terminal_settings_model_midl_reference_args /reference "${_terminal_settings_model_reference_native}")
    get_filename_component(_terminal_settings_model_reference_dir "${_terminal_settings_model_reference}" DIRECTORY)
    list(APPEND _terminal_settings_model_mdmerge_metadata_args -metadata_dir "${_terminal_settings_model_reference_dir}")
endforeach()
file(TO_NATIVE_PATH "${_terminal_settings_model_source_root}" _terminal_settings_model_source_root_native)
set(_terminal_settings_model_unmerged_winmds)
foreach(_terminal_settings_model_idl IN LISTS _terminal_settings_model_idls)
    get_filename_component(_terminal_settings_model_idl_name "${_terminal_settings_model_idl}" NAME_WE)
    set(_terminal_settings_model_unmerged_winmd "${_terminal_settings_model_unmerged_root}/${_terminal_settings_model_idl_name}.winmd")
    file(TO_NATIVE_PATH "${_terminal_settings_model_idl}" _terminal_settings_model_idl_native)
    file(TO_NATIVE_PATH "${_terminal_settings_model_unmerged_winmd}" _terminal_settings_model_unmerged_winmd_native)
    add_custom_command(
        OUTPUT "${_terminal_settings_model_unmerged_winmd}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_settings_model_unmerged_root}"
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
            "${_portable_midl_path}"
            /I "${_terminal_connection_sdk_winrt_native}" /I "${_terminal_connection_sdk_shared_native}"
            /I "${_terminal_connection_sdk_um_native}" /I "${_terminal_settings_model_source_root_native}"
            ${_terminal_settings_model_midl_reference_args}
            /metadata_dir "${_terminal_control_midl_metadata_root_native}"
            /winrt /W1 /nologo /char signed /env ${_portable_midl_environment}
            /winmd "${_terminal_settings_model_unmerged_winmd_native}"
            /h NUL /dlldata NUL /iid NUL /proxy NUL /notlb /client none /server none
            /enum_class /ns_prefix /target NT60 /nomidl "${_terminal_settings_model_idl_native}"
        DEPENDS ${_terminal_settings_model_idls} ${_terminal_settings_model_reference_winmds} "${_portable_midl_path}"
        VERBATIM
        COMMENT "Generating Settings Model ${_terminal_settings_model_idl_name}.winmd"
    )
    list(APPEND _terminal_settings_model_unmerged_winmds "${_terminal_settings_model_unmerged_winmd}")
endforeach()
add_custom_command(
    OUTPUT "${_terminal_settings_model_reference_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_settings_model_reference_root}"
    COMMAND "${_cppwinrt_path}"
        -input "${_terminal_control_winmd}" -input "${_terminal_core_winmd}" -input "${_terminal_connection_winmd}"
        -input "${_ui_helpers_mux_winmd}" -input "${_ui_helpers_webview2_winmd}"
        -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}" -output "${_terminal_settings_model_reference_root}"
    DEPENDS "${_terminal_control_winmd}" "${_terminal_core_winmd}" "${_terminal_connection_winmd}"
        "${_ui_helpers_mux_winmd}" "${_ui_helpers_webview2_winmd}" "${_cppwinrt_path}"
    VERBATIM
    COMMENT "Generating Settings Model dependency reference projection"
)
add_custom_command(
    OUTPUT "${_terminal_settings_model_winmd}" "${_terminal_settings_model_module_cpp}" "${_terminal_settings_model_projection_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_settings_model_lib_output_root}" "${_terminal_settings_model_projection_root}"
    COMMAND "${_portable_mdmerge_path}" -i "${_terminal_settings_model_unmerged_root}" -o "${_terminal_settings_model_lib_output_root}"
        ${_terminal_settings_model_mdmerge_metadata_args} -partial
    COMMAND "${_cppwinrt_path}"
        -input "${_terminal_settings_model_winmd}" -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -reference "${_terminal_control_winmd}" -reference "${_terminal_core_winmd}" -reference "${_terminal_connection_winmd}"
        -reference "${_ui_helpers_mux_winmd}" -reference "${_ui_helpers_webview2_winmd}"
        -output "${_terminal_settings_model_projection_root}" -component "${_terminal_settings_model_component_root}"
        -overwrite -name Microsoft.Terminal.Settings.Model -pch pch.h -prefix -optimize
    DEPENDS ${_terminal_settings_model_unmerged_winmds} ${_terminal_settings_model_reference_winmds}
        "${_portable_mdmerge_path}" "${_cppwinrt_path}"
    VERBATIM
    COMMENT "Merging Settings Model WinMD and generating its component projection"
)
set(_terminal_settings_model_sources
    VisualStudioGenerator.cpp DefaultTerminal.cpp init.cpp pch.cpp ActionAndArgs.cpp ActionArgs.cpp ActionMap.cpp
    ActionMapSerialization.cpp AzureCloudShellGenerator.cpp ApplicationState.cpp CascadiaSettings.cpp CascadiaSettingsSerialization.cpp
    ColorScheme.cpp Theme.cpp Command.cpp DynamicProfileUtils.cpp FileUtils.cpp GlobalAppSettings.cpp KeyChordSerialization.cpp
    PowershellCoreProfileGenerator.cpp Profile.cpp AppearanceConfig.cpp FontConfig.cpp EnumMappings.cpp NewTabMenuEntry.cpp
    SeparatorEntry.cpp ActionEntry.cpp FolderEntry.cpp ProfileEntry.cpp ProfileCollectionEntry.cpp RemainingProfilesEntry.cpp
    MatchProfilesEntry.cpp MediaResourceSupport.cpp VsDevCmdGenerator.cpp VsDevShellGenerator.cpp VsSetupConfiguration.cpp
    WslDistroGenerator.cpp SshHostGenerator.cpp Workspace.cpp TestHooks.cpp
)
list(TRANSFORM _terminal_settings_model_sources PREPEND "${_terminal_settings_model_source_root}/")
list(APPEND _terminal_settings_model_sources "${_terminal_settings_model_module_cpp}"
    "${_workspace_root}/src/glue/chat/WorkspaceDiagnosticLog.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatTextHelpers.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceStoragePaths.cpp")
set(_terminal_settings_model_json_root "${_terminal_settings_model_generation_root}/json")
set(_terminal_settings_model_defaults "${_terminal_settings_model_json_root}/defaults.json")
set(_terminal_settings_model_user_defaults "${_terminal_settings_model_json_root}/userDefaults.json")
set(_terminal_settings_model_color_selection "${_terminal_settings_model_json_root}/enableColorSelection.json")
add_custom_command(
    OUTPUT "${_terminal_settings_model_defaults}" "${_terminal_settings_model_user_defaults}" "${_terminal_settings_model_color_selection}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_settings_model_json_root}"
    COMMAND pwsh.exe -NoProfile -ExecutionPolicy Unrestricted "${_openconsole_root}/tools/CompressJson.ps1" -JsonFile "${_terminal_settings_model_source_root}/defaults.json" -OutPath "${_terminal_settings_model_defaults}"
    COMMAND pwsh.exe -NoProfile -ExecutionPolicy Unrestricted "${_openconsole_root}/tools/CompressJson.ps1" -JsonFile "${_terminal_settings_model_source_root}/userDefaults.json" -OutPath "${_terminal_settings_model_user_defaults}"
    COMMAND pwsh.exe -NoProfile -ExecutionPolicy Unrestricted "${_openconsole_root}/tools/CompressJson.ps1" -JsonFile "${_terminal_settings_model_source_root}/enableColorSelection.json" -OutPath "${_terminal_settings_model_color_selection}"
    DEPENDS "${_terminal_settings_model_source_root}/defaults.json" "${_terminal_settings_model_source_root}/userDefaults.json"
        "${_terminal_settings_model_source_root}/enableColorSelection.json" "${_openconsole_root}/tools/CompressJson.ps1"
    VERBATIM
    COMMENT "Compressing Settings Model embedded JSON"
)
set(_terminal_settings_model_rc "${_terminal_settings_model_generation_root}/Microsoft.Terminal.Settings.Model.rc")
file(GENERATE OUTPUT "${_terminal_settings_model_rc}" CONTENT
"#include \"${_terminal_settings_model_source_root}/resource.h\"\nIDR_DEFAULTS RCDATA \"${_terminal_settings_model_defaults}\"\nIDR_USER_DEFAULTS RCDATA \"${_terminal_settings_model_user_defaults}\"\nIDR_ENABLE_COLOR_SELECTION RCDATA \"${_terminal_settings_model_color_selection}\"\n")
file(GLOB _terminal_settings_model_resw CONFIGURE_DEPENDS "${_terminal_settings_model_source_root}/Resources/*/Resources.resw")
set(_terminal_settings_model_resources_resfiles "")
foreach(_terminal_settings_model_resource IN LISTS _terminal_settings_model_resw)
    file(TO_NATIVE_PATH "${_terminal_settings_model_resource}" _terminal_settings_model_resource_native)
    string(APPEND _terminal_settings_model_resources_resfiles "${_terminal_settings_model_resource_native}\n")
endforeach()
set(_terminal_settings_model_pri "${_terminal_settings_model_lib_output_root}/Microsoft.Terminal.Settings.Model.pri")
set(_terminal_settings_model_resources_list "${_terminal_settings_model_generation_root}/resources.resfiles")
set(_terminal_settings_model_priconfig "${_terminal_settings_model_generation_root}/priconfig.xml")
file(TO_NATIVE_PATH "${_terminal_settings_model_resources_list}" _terminal_settings_model_resources_list_native)
file(GENERATE OUTPUT "${_terminal_settings_model_resources_list}" CONTENT "${_terminal_settings_model_resources_resfiles}")
file(GENERATE OUTPUT "${_terminal_settings_model_priconfig}" CONTENT
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources targetOsVersion=\"10.0.0\" majorVersion=\"1\"><index root=\"\\\" startIndexAt=\"${_terminal_settings_model_resources_list_native}\"><default><qualifier name=\"Language\" value=\"en-US\" /><qualifier name=\"Contrast\" value=\"standard\" /><qualifier name=\"Scale\" value=\"200\" /><qualifier name=\"HomeRegion\" value=\"001\" /><qualifier name=\"TargetSize\" value=\"256\" /><qualifier name=\"LayoutDirection\" value=\"LTR\" /><qualifier name=\"DXFeatureLevel\" value=\"DX9\" /><qualifier name=\"Configuration\" value=\"\" /><qualifier name=\"AlternateForm\" value=\"\" /><qualifier name=\"Platform\" value=\"UAP\" /></default><indexer-config type=\"RESW\" convertDotsToSlashes=\"true\" initialPath=\"Microsoft.Terminal.Settings.Model\" /><indexer-config type=\"RESFILES\" qualifierDelimiter=\".\" /></index></resources>\n")
add_custom_command(
    OUTPUT "${_terminal_settings_model_pri}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_settings_model_lib_output_root}"
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new /pr "${_terminal_settings_model_source_root}" /cf "${_terminal_settings_model_priconfig}" /of "${_terminal_settings_model_pri}" /in Microsoft.Terminal.Settings.Model /o
    DEPENDS ${_terminal_settings_model_resw} "${_terminal_settings_model_resources_list}" "${_terminal_settings_model_priconfig}"
    VERBATIM COMMENT "Generating Microsoft.Terminal.Settings.Model.pri"
)
set_source_files_properties("${_terminal_settings_model_projection_header}" "${_terminal_settings_model_reference_header}" "${_terminal_settings_model_pri}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_library(${_terminal_settings_model_lib_target} STATIC ${_terminal_settings_model_sources}
    "${_terminal_settings_model_projection_header}" "${_terminal_settings_model_reference_header}" "${_terminal_settings_model_pri}")
target_compile_features(${_terminal_settings_model_lib_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_settings_model_lib_target} PRIVATE "${_terminal_settings_model_source_root}/pch.h")
target_include_directories(${_terminal_settings_model_lib_target} PRIVATE
    "${_terminal_settings_model_reference_root}" "${_terminal_settings_model_projection_root}" "${_terminal_settings_model_source_root}"
    "${_openconsole_root}/src/cascadia" "${_native_cppwinrt_projection_root}" "${_wil_package_dir}/include"
    "${_openconsole_root}/packages/Microsoft.VisualStudio.Setup.Configuration.Native.2.3.2262/lib/native/include"
    "${_workspace_root}/src/glue/chat" "${_workspace_root}/src/core/chat"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/cascadia/inc" "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include" ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_terminal_settings_model_lib_target} PRIVATE _LIB _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__)
target_compile_options(${_terminal_settings_model_lib_target} PRIVATE ${_native_product_archive_compile_options} /Zc:preprocessor /bigobj /W3 /wd4702 /wd5104 /wd28204 /wd4201 /wd4312 /wd5105 /wd26434 /wd26445 /wd26456 /wd26478 /wd26494 "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
set_target_properties(${_terminal_settings_model_lib_target} PROPERTIES OUTPUT_NAME "Microsoft.Terminal.Settings.Model.Lib" INTERPROCEDURAL_OPTIMIZATION FALSE MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" ARCHIVE_OUTPUT_DIRECTORY "${_terminal_settings_model_lib_output_root}")
set_source_files_properties("${_terminal_settings_model_rc}" PROPERTIES GENERATED TRUE)
add_library(${_terminal_settings_model_target} SHARED "${_terminal_settings_model_rc}" "${_terminal_settings_model_source_root}/dll/Microsoft.Terminal.Settings.Model.def" "${_terminal_settings_model_defaults}" "${_terminal_settings_model_user_defaults}" "${_terminal_settings_model_color_selection}")
target_link_directories(${_terminal_settings_model_target} PRIVATE "${_workspace_extension_vcpkg_lib_root}" "${_workspace_extension_vcpkg_manual_lib_root}" ${_portable_msvc_lib_dirs})
target_link_libraries(${_terminal_settings_model_target} PRIVATE
    ${_terminal_settings_model_lib_target} ConTypes WinRTUtils ConInt ConProps
    "${_workspace_extension_vcpkg_lib_root}/fmt.lib"
    "${_workspace_extension_vcpkg_lib_root}/jsoncpp.lib"
    "${_openconsole_root}/packages/Microsoft.VisualStudio.Setup.Configuration.Native.2.3.2262/lib/native/v141/${PORTABLE_PLATFORM}/Microsoft.VisualStudio.Setup.Configuration.Native.lib"
    user32.lib onecoreuap.lib WindowsApp.lib ntdll.lib shlwapi.lib pathcch.lib propsys.lib
)
target_link_options(${_terminal_settings_model_target} PRIVATE /SUBSYSTEM:WINDOWS /MANIFEST:NO /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT "/INCLUDE:${_portable_dllmain_linker_symbol}")
add_custom_command(TARGET ${_terminal_settings_model_target} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_terminal_settings_model_winmd}" "${_terminal_settings_model_dll_winmd}")
set_target_properties(${_terminal_settings_model_target} PROPERTIES OUTPUT_NAME "Microsoft.Terminal.Settings.Model" MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" RUNTIME_OUTPUT_DIRECTORY "${_terminal_settings_model_output_root}" LIBRARY_OUTPUT_DIRECTORY "${_terminal_settings_model_output_root}" ARCHIVE_OUTPUT_DIRECTORY "${_terminal_settings_model_output_root}" PDB_OUTPUT_DIRECTORY "${_terminal_settings_model_output_root}")

set(_terminal_settings_adapter_target TerminalSettingsAppAdapterLib)
set(_terminal_settings_adapter_source_root "${_openconsole_root}/src/cascadia/TerminalSettingsAppAdapterLib")
set(_terminal_settings_adapter_projection_root "${CMAKE_BINARY_DIR}/terminal-settings-adapter-projection")
set(_terminal_settings_adapter_projection_header "${_terminal_settings_adapter_projection_root}/winrt/Microsoft.Terminal.Settings.Model.h")
set(_terminal_settings_adapter_winmds
    "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/Microsoft.Terminal.Control/Microsoft.Terminal.Control.winmd"
    "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/TerminalCore/Microsoft.Terminal.Core.winmd"
    "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/Microsoft.Terminal.Settings.Model/Microsoft.Terminal.Settings.Model.winmd"
    "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/TerminalConnection/Microsoft.Terminal.TerminalConnection.winmd"
    "${_openconsole_root}/packages/Microsoft.UI.Xaml.2.8.4/lib/uap10.0/Microsoft.UI.Xaml.winmd"
    "${_openconsole_root}/packages/Microsoft.Web.WebView2.1.0.1661.34/lib/Microsoft.Web.WebView2.Core.winmd"
)
add_custom_command(
    OUTPUT "${_terminal_settings_adapter_projection_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_settings_adapter_projection_root}"
    COMMAND "${_cppwinrt_path}"
        -input "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/Microsoft.Terminal.Control/Microsoft.Terminal.Control.winmd"
        -input "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/TerminalCore/Microsoft.Terminal.Core.winmd"
        -input "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/Microsoft.Terminal.Settings.Model/Microsoft.Terminal.Settings.Model.winmd"
        -input "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/TerminalConnection/Microsoft.Terminal.TerminalConnection.winmd"
        -input "${_openconsole_root}/packages/Microsoft.UI.Xaml.2.8.4/lib/uap10.0/Microsoft.UI.Xaml.winmd"
        -input "${_openconsole_root}/packages/Microsoft.Web.WebView2.1.0.1661.34/lib/Microsoft.Web.WebView2.Core.winmd"
        -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -output "${_terminal_settings_adapter_projection_root}"
    DEPENDS ${_terminal_settings_adapter_winmds} "${_cppwinrt_path}"
    VERBATIM
    COMMENT "Generating the TerminalSettingsAppAdapterLib C++/WinRT reference projection"
)
set_source_files_properties("${_terminal_settings_adapter_projection_header}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
add_library(${_terminal_settings_adapter_target} STATIC EXCLUDE_FROM_ALL
    "${_terminal_settings_adapter_source_root}/TerminalSettings.cpp"
    "${_terminal_settings_adapter_projection_header}"
    "${_native_cppwinrt_projection_base}"
)
target_compile_features(${_terminal_settings_adapter_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_settings_adapter_target} PRIVATE "${_terminal_settings_adapter_source_root}/pch.h")
target_include_directories(${_terminal_settings_adapter_target} PRIVATE
    "${_terminal_settings_adapter_projection_root}"
    "${_native_cppwinrt_projection_root}"
    "${_wil_package_dir}/include"
    "${_terminal_settings_adapter_source_root}"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc"
    "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_terminal_settings_adapter_target} PRIVATE
    _LIB _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__
)
target_compile_options(${_terminal_settings_adapter_target} PRIVATE
    ${_native_product_archive_compile_options}
    /Zc:preprocessor
    /bigobj
    /wd5104 /wd28204 /wd4201 /wd4312 /wd5105 /wd26434 /wd26445 /wd26456 /wd26478 /wd26494
    "/FU${_workspace_extension_platform_winmd}"
    "/FI${_workspace_extension_til_feature_header}"
)
set_target_properties(${_terminal_settings_adapter_target} PROPERTIES
    OUTPUT_NAME "TerminalSettingsAppAdapterLib"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION FALSE
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}/TerminalSettingsAppAdapterLib"
)
add_dependencies(${_terminal_settings_adapter_target} ${_terminal_settings_model_target})

set(_ui_markdown_target UIMarkdown)
set(_ui_markdown_source_root "${_openconsole_root}/src/cascadia/UIMarkdown")
set(_ui_markdown_output_root "${_native_product_output_root}/Microsoft.Terminal.UI.Markdown")
set(_ui_markdown_generation_root "${CMAKE_BINARY_DIR}/ui-markdown/$<CONFIG>")
set(_ui_markdown_unmerged_root "${_ui_markdown_generation_root}/unmerged")
set(_ui_markdown_projection_root "${_ui_markdown_generation_root}/projection")
set(_ui_markdown_component_root "${_ui_markdown_projection_root}/sources")
set(_ui_markdown_reference_root "${_ui_markdown_generation_root}/references")
set(_ui_markdown_winmd "${_ui_markdown_output_root}/Microsoft.Terminal.UI.Markdown.winmd")
set(_ui_markdown_module_cpp "${_ui_markdown_projection_root}/module.g.cpp")
set(_ui_markdown_projection_header "${_ui_markdown_projection_root}/winrt/Microsoft.Terminal.UI.Markdown.h")
set(_ui_markdown_reference_header "${_ui_markdown_reference_root}/winrt/Microsoft.Terminal.UI.h")
set(_ui_markdown_xaml_metadata_idl "${_ui_markdown_generation_root}/XamlMetaDataProvider.idl")
file(GENERATE OUTPUT "${_ui_markdown_xaml_metadata_idl}" CONTENT
"// Generated by CMake for the Windows XAML metadata provider.\nnamespace Microsoft.Terminal.UI.Markdown\n{\n    runtimeclass XamlMetaDataProvider : [default] Windows.UI.Xaml.Markup.IXamlMetadataProvider\n    {\n        XamlMetaDataProvider();\n    }\n}\n")
set(_ui_markdown_idls
    "${_ui_markdown_source_root}/Builder.idl"
    "${_ui_markdown_source_root}/CodeBlock.idl"
    "${_ui_markdown_xaml_metadata_idl}"
)
set(_ui_markdown_product_reference_winmds "${_ui_helpers_winmd}" "${_ui_helpers_mux_winmd}" "${_ui_helpers_webview2_winmd}")
set(_ui_markdown_reference_winmds ${_ui_markdown_product_reference_winmds} ${_terminal_control_xaml_sdk_contract_winmds})
set(_ui_markdown_midl_metadata_root "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Windows.Foundation.FoundationContract/4.0.0.0")
file(TO_NATIVE_PATH "${_ui_markdown_midl_metadata_root}" _ui_markdown_midl_metadata_root_native)
set(_ui_markdown_midl_reference_args)
set(_ui_markdown_mdmerge_metadata_args)
foreach(_ui_markdown_reference IN LISTS _ui_markdown_reference_winmds)
    file(TO_NATIVE_PATH "${_ui_markdown_reference}" _ui_markdown_reference_native)
    list(APPEND _ui_markdown_midl_reference_args /reference "${_ui_markdown_reference_native}")
    get_filename_component(_ui_markdown_reference_dir "${_ui_markdown_reference}" DIRECTORY)
    list(APPEND _ui_markdown_mdmerge_metadata_args -metadata_dir "${_ui_markdown_reference_dir}")
endforeach()
file(TO_NATIVE_PATH "${_ui_markdown_source_root}" _ui_markdown_source_root_native)
set(_ui_markdown_unmerged_winmds)
foreach(_ui_markdown_idl IN LISTS _ui_markdown_idls)
    get_filename_component(_ui_markdown_idl_name "${_ui_markdown_idl}" NAME_WE)
    set(_ui_markdown_unmerged_winmd "${_ui_markdown_unmerged_root}/${_ui_markdown_idl_name}.winmd")
    file(TO_NATIVE_PATH "${_ui_markdown_idl}" _ui_markdown_idl_native)
    file(TO_NATIVE_PATH "${_ui_markdown_unmerged_winmd}" _ui_markdown_unmerged_winmd_native)
    add_custom_command(
        OUTPUT "${_ui_markdown_unmerged_winmd}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ui_markdown_unmerged_root}"
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
            "${_portable_midl_path}"
            /I "${_terminal_connection_sdk_winrt_native}" /I "${_terminal_connection_sdk_shared_native}"
            /I "${_terminal_connection_sdk_um_native}" /I "${_ui_markdown_source_root_native}"
            ${_ui_markdown_midl_reference_args}
            /metadata_dir "${_ui_markdown_midl_metadata_root_native}"
            /winrt /W1 /nologo /char signed /env ${_portable_midl_environment}
            /winmd "${_ui_markdown_unmerged_winmd_native}"
            /h NUL /dlldata NUL /iid NUL /proxy NUL /notlb /client none /server none
            /enum_class /ns_prefix /target NT60 /nomidl "${_ui_markdown_idl_native}"
        DEPENDS ${_ui_markdown_idls} ${_ui_markdown_reference_winmds} "${_portable_midl_path}"
        VERBATIM COMMENT "Generating UIMarkdown ${_ui_markdown_idl_name}.winmd"
    )
    list(APPEND _ui_markdown_unmerged_winmds "${_ui_markdown_unmerged_winmd}")
endforeach()
add_custom_command(
    OUTPUT "${_ui_markdown_winmd}" "${_ui_markdown_module_cpp}" "${_ui_markdown_projection_header}"
    BYPRODUCTS "${_ui_markdown_component_root}/XamlMetaDataProvider.cpp"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ui_markdown_output_root}" "${_ui_markdown_projection_root}"
    COMMAND "${_portable_mdmerge_path}" -i "${_ui_markdown_unmerged_root}" -o "${_ui_markdown_output_root}"
        ${_ui_markdown_mdmerge_metadata_args} -partial
    COMMAND "${_cppwinrt_path}"
        -input "${_ui_markdown_winmd}" -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -reference "${_ui_helpers_winmd}" -reference "${_ui_helpers_mux_winmd}" -reference "${_ui_helpers_webview2_winmd}"
        -output "${_ui_markdown_projection_root}" -component "${_ui_markdown_component_root}"
        -overwrite -name Microsoft.Terminal.UI.Markdown -pch pch.h -prefix -optimize
    DEPENDS ${_ui_markdown_unmerged_winmds} ${_ui_markdown_reference_winmds} "${_portable_mdmerge_path}" "${_cppwinrt_path}"
    VERBATIM COMMENT "Merging UIMarkdown WinMD and generating its component projection"
)
add_custom_command(
    OUTPUT "${_ui_markdown_reference_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ui_markdown_reference_root}"
    COMMAND "${_cppwinrt_path}" -input "${_ui_helpers_winmd}" -input "${_ui_helpers_mux_winmd}"
        -input "${_ui_helpers_webview2_winmd}" -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -output "${_ui_markdown_reference_root}"
    DEPENDS ${_ui_markdown_product_reference_winmds} "${_cppwinrt_path}"
    VERBATIM COMMENT "Generating UIMarkdown dependency reference projection"
)
set(_ui_markdown_page "${_ui_markdown_source_root}/CodeBlock.xaml")
set(_ui_markdown_xaml_hpp "${_ui_markdown_projection_root}/CodeBlock.xaml.g.hpp")
set(_ui_markdown_xbf "${_ui_markdown_projection_root}/CodeBlock.xbf")
set(_ui_markdown_xaml_typeinfo "${_ui_markdown_projection_root}/XamlTypeInfo.g.cpp")
set(_ui_markdown_xaml_typeinfo_impl "${_ui_markdown_projection_root}/XamlTypeInfo.Impl.g.cpp")
set(_ui_markdown_xaml_provider_cpp "${_ui_markdown_projection_root}/XamlMetaDataProvider.cpp")
file(GENERATE OUTPUT "${_ui_markdown_xaml_provider_cpp}" CONTENT
"// This file is generated by the build to support Xaml apps\n#include \"pch.h\"\n#include \"XamlMetaDataProvider.h\"\n#include \"XamlMetaDataProvider.g.cpp\"\n")
set(_ui_markdown_xaml_sdk_reference_args)
foreach(_ui_markdown_reference IN LISTS _terminal_control_xaml_sdk_contract_winmds)
    list(APPEND _ui_markdown_xaml_sdk_reference_args --system-reference "${_ui_markdown_reference}")
endforeach()
set(_ui_markdown_xaml_common_args
    --framework-runtime-dir "${_xaml_framework_runtime_dir}" --task-assembly "${_xaml_compiler_task}" --language CppWinRT
    --output "${_ui_markdown_projection_root}" --output-type dynamiclibrary
    --root-namespace Microsoft.Terminal.UI.Markdown --windows-sdk-path "${_portable_sdk_root}"
    --vc-install-dir "${_portable_vc_root}" --target-platform-min-version 10.0.18362.0
    --project-path "${_ui_markdown_source_root}/CMakeLists.txt"
    --project-name Microsoft.Terminal.UI.Markdown --pri-index-name Microsoft.Terminal.UI.Markdown
    --page "${_ui_markdown_page}" --include "${_ui_markdown_source_root}"
    --winmd-reference "${_ui_helpers_winmd}" --winmd-reference "${_ui_helpers_mux_winmd}"
    ${_ui_markdown_xaml_sdk_reference_args} --reference "${_terminal_control_xaml_facade_winmd}"
)
add_custom_command(
    OUTPUT "${_ui_markdown_xaml_hpp}" "${_ui_markdown_xbf}" "${_ui_markdown_xaml_typeinfo}" "${_ui_markdown_xaml_typeinfo_impl}"
    BYPRODUCTS "${_ui_markdown_projection_root}/XamlBindingInfo.xaml.g.hpp"
        "${_ui_markdown_projection_root}/XamlMetaDataProvider.h" "${_ui_markdown_projection_root}/XamlSaveStateFile.xml"
    COMMAND "${_xaml_compiler_host}" ${_ui_markdown_xaml_common_args} --compile-mode RealBuildPass1
    COMMAND "${_xaml_compiler_host}" ${_ui_markdown_xaml_common_args} --compile-mode RealBuildPass2
        --platform-xml-dir "${_portable_sdk_root}/Platforms/UAP/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        --local-assembly "${_ui_markdown_winmd}"
        --cl-include "Builder.h#Builder.idl#${_ui_markdown_source_root}/Builder.h"
        --cl-include "CodeBlock.h#CodeBlock.xaml#${_ui_markdown_source_root}/CodeBlock.h"
        --cl-include "MarkdownToXaml.h##${_ui_markdown_source_root}/MarkdownToXaml.h"
        --cl-include "pch.h##${_ui_markdown_source_root}/pch.h"
    DEPENDS "${_xaml_compiler_host}" "${_ui_markdown_winmd}" "${_ui_markdown_page}"
        "${_ui_markdown_source_root}/Builder.h" "${_ui_markdown_source_root}/CodeBlock.h"
        "${_ui_markdown_source_root}/MarkdownToXaml.h" "${_ui_markdown_source_root}/pch.h"
        ${_ui_markdown_reference_winmds} "${_terminal_control_xaml_facade_winmd}"
    VERBATIM COMMENT "Generating UIMarkdown XAML code and XBF without MSBuild"
)
add_custom_target(UIMarkdownXaml DEPENDS "${_ui_markdown_xaml_hpp}" "${_ui_markdown_xbf}"
    "${_ui_markdown_xaml_typeinfo}" "${_ui_markdown_xaml_typeinfo_impl}")
set(_ui_markdown_pri "${_ui_markdown_output_root}/Microsoft.Terminal.UI.Markdown.pri")
set(_ui_markdown_pri_root "${_ui_markdown_generation_root}/pri-root")
set(_ui_markdown_embed_list "${_ui_markdown_pri_root}/embed.resfiles")
set(_ui_markdown_priconfig "${_ui_markdown_generation_root}/priconfig.xml")
file(GENERATE OUTPUT "${_ui_markdown_embed_list}" CONTENT "Microsoft.Terminal.UI.Markdown\\CodeBlock.xbf\n")
file(GENERATE OUTPUT "${_ui_markdown_priconfig}" CONTENT
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources targetOsVersion=\"10.0.0\" majorVersion=\"1\"><index root=\"${_ui_markdown_pri_root}\" startIndexAt=\"embed.resfiles\"><default><qualifier name=\"Language\" value=\"en-US\" /><qualifier name=\"Contrast\" value=\"standard\" /><qualifier name=\"Scale\" value=\"200\" /><qualifier name=\"HomeRegion\" value=\"001\" /><qualifier name=\"TargetSize\" value=\"256\" /><qualifier name=\"LayoutDirection\" value=\"LTR\" /><qualifier name=\"DXFeatureLevel\" value=\"DX9\" /><qualifier name=\"Configuration\" value=\"\" /><qualifier name=\"AlternateForm\" value=\"\" /><qualifier name=\"Platform\" value=\"UAP\" /></default><indexer-config type=\"RESFILES\" qualifierDelimiter=\".\" /><indexer-config type=\"EMBEDFILES\" /></index></resources>\n")
add_custom_command(
    OUTPUT "${_ui_markdown_pri}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_ui_markdown_output_root}" "${_ui_markdown_pri_root}/Microsoft.Terminal.UI.Markdown"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_ui_markdown_xbf}" "${_ui_markdown_pri_root}/Microsoft.Terminal.UI.Markdown/CodeBlock.xbf"
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new /pr "${_ui_markdown_pri_root}" /cf "${_ui_markdown_priconfig}" /of "${_ui_markdown_pri}" /in Microsoft.Terminal.UI.Markdown /o
    DEPENDS "${_ui_markdown_xbf}" "${_ui_markdown_embed_list}" "${_ui_markdown_priconfig}"
    VERBATIM COMMENT "Generating Microsoft.Terminal.UI.Markdown.pri"
)
set(_ui_markdown_sources
    "${_ui_markdown_source_root}/init.cpp" "${_ui_markdown_source_root}/pch.cpp"
    "${_ui_markdown_source_root}/Builder.cpp" "${_ui_markdown_source_root}/CodeBlock.cpp"
    "${_ui_markdown_source_root}/MarkdownToXaml.cpp" "${_ui_markdown_module_cpp}"
    "${_ui_markdown_xaml_provider_cpp}" "${_ui_markdown_xaml_typeinfo}" "${_ui_markdown_xaml_typeinfo_impl}"
)
set_source_files_properties("${_ui_markdown_module_cpp}" "${_ui_markdown_xaml_provider_cpp}"
    "${_ui_markdown_xaml_typeinfo}" "${_ui_markdown_xaml_typeinfo_impl}" "${_ui_markdown_pri}" PROPERTIES GENERATED TRUE)
add_library(${_ui_markdown_target} SHARED ${_ui_markdown_sources}
    "${_ui_markdown_source_root}/Microsoft.Terminal.UI.Markdown.def" "${_ui_markdown_pri}" "${_ui_markdown_reference_header}")
add_dependencies(${_ui_markdown_target} UIMarkdownXaml ${_ui_helpers_target})
target_compile_features(${_ui_markdown_target} PRIVATE cxx_std_20)
target_precompile_headers(${_ui_markdown_target} PRIVATE "${_ui_markdown_source_root}/pch.h")
target_include_directories(${_ui_markdown_target} PRIVATE
    "${_ui_markdown_reference_root}" "${_ui_markdown_projection_root}" "${_ui_markdown_source_root}"
    "${_ui_helpers_projection_root}" "${_native_cppwinrt_projection_root}"
    "${_wil_package_dir}/include" "${_openconsole_root}/src/cascadia/WinRTUtils/inc"
    "${_openconsole_root}/src/cascadia/inc" "${_openconsole_root}/src/types/inc" "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include" ${_portable_msvc_include_dirs})
target_compile_definitions(${_ui_markdown_target} PRIVATE
    _USRDLL _WINDLL _WINRT_DLL _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__)
target_compile_options(${_ui_markdown_target} PRIVATE ${_native_product_compile_options}
    /W3 /Zc:preprocessor /bigobj /wd4702 /wd5104 /wd28204 /wd4201 /wd4312 /wd5105
    "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
target_link_directories(${_ui_markdown_target} PRIVATE "${_workspace_extension_vcpkg_lib_root}"
    "${_workspace_extension_vcpkg_manual_lib_root}" ${_portable_msvc_lib_dirs})
target_link_libraries(${_ui_markdown_target} PRIVATE ConTypes WinRTUtils
    "${_workspace_extension_vcpkg_lib_root}/cmark.lib" onecoreuap.lib WindowsApp.lib shlwapi.lib)
target_link_options(${_ui_markdown_target} PRIVATE /SUBSYSTEM:CONSOLE /MANIFEST:NO /guard:cf /DEBUG:FULL
    /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT "/INCLUDE:${_portable_dllmain_linker_symbol}")
set_target_properties(${_ui_markdown_target} PROPERTIES OUTPUT_NAME "Microsoft.Terminal.UI.Markdown"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" RUNTIME_OUTPUT_DIRECTORY "${_ui_markdown_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_ui_markdown_output_root}" ARCHIVE_OUTPUT_DIRECTORY "${_ui_markdown_output_root}"
    PDB_OUTPUT_DIRECTORY "${_ui_markdown_output_root}")

set(_terminal_settings_editor_target TerminalSettingsEditor)
set(_terminal_settings_editor_source_root "${_openconsole_root}/src/cascadia/TerminalSettingsEditor")
set(_terminal_settings_editor_output_root "${_native_product_output_root}/Microsoft.Terminal.Settings.Editor")
set(_terminal_settings_editor_generation_root "${CMAKE_BINARY_DIR}/terminal-settings-editor/$<CONFIG>")
set(_terminal_settings_editor_unmerged_root "${_terminal_settings_editor_generation_root}/unmerged")
set(_terminal_settings_editor_projection_root "${_terminal_settings_editor_generation_root}/projection")
set(_terminal_settings_editor_component_root "${_terminal_settings_editor_projection_root}/sources")
set(_terminal_settings_editor_reference_root "${_terminal_settings_editor_generation_root}/references")
set(_terminal_settings_editor_winmd "${_terminal_settings_editor_output_root}/Microsoft.Terminal.Settings.Editor.winmd")
set(_terminal_settings_editor_module_cpp "${_terminal_settings_editor_projection_root}/module.g.cpp")
set(_terminal_settings_editor_projection_header "${_terminal_settings_editor_projection_root}/winrt/Microsoft.Terminal.Settings.Editor.h")
set(_terminal_settings_editor_reference_header "${_terminal_settings_editor_reference_root}/winrt/Microsoft.Terminal.Settings.Model.h")
set(_terminal_settings_editor_xaml_metadata_idl "${_terminal_settings_editor_generation_root}/XamlMetaDataProvider.idl")
file(GENERATE OUTPUT "${_terminal_settings_editor_xaml_metadata_idl}" CONTENT
"// Generated by CMake for the Windows XAML metadata provider.\nnamespace Microsoft.Terminal.Settings.Editor\n{\n    runtimeclass XamlMetaDataProvider : [default] Windows.UI.Xaml.Markup.IXamlMetadataProvider\n    {\n        XamlMetaDataProvider();\n    }\n}\n")
set(_terminal_settings_editor_idl_names
    Actions AddProfile EnumEntry GlobalAppearance ColorSchemes NullableColorPicker IconPicker EditColorScheme
    KeyChordListener Launch NewTabMenu Interaction Compatibility Rendering SearchIndex MainPage ProfileViewModel
    ActionsViewModel TerminalColorConverters ColorSchemeViewModel ColorSchemesPageViewModel RenderingViewModel
    InteractionViewModel GlobalAppearanceViewModel LaunchViewModel NewTabMenuViewModel Extensions Profiles_Base
    Profiles_Base_Orphaned Profiles_Advanced Profiles_Appearance Profiles_Terminal Appearances SettingContainer
)
set(_terminal_settings_editor_idls)
foreach(_terminal_settings_editor_idl_name IN LISTS _terminal_settings_editor_idl_names)
    list(APPEND _terminal_settings_editor_idls "${_terminal_settings_editor_source_root}/${_terminal_settings_editor_idl_name}.idl")
endforeach()
list(APPEND _terminal_settings_editor_idls "${_terminal_settings_editor_xaml_metadata_idl}")
set(_terminal_settings_editor_product_reference_winmds
    "${_ui_helpers_winmd}" "${_terminal_connection_winmd}" "${_terminal_settings_model_winmd}"
    "${_terminal_control_winmd}" "${_terminal_core_winmd}" "${_ui_helpers_mux_winmd}" "${_ui_helpers_webview2_winmd}"
)
set(_terminal_settings_editor_reference_winmds ${_terminal_settings_editor_product_reference_winmds} ${_terminal_control_xaml_sdk_contract_winmds})
set(_terminal_settings_editor_midl_metadata_root "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Windows.Foundation.FoundationContract/4.0.0.0")
file(TO_NATIVE_PATH "${_terminal_settings_editor_midl_metadata_root}" _terminal_settings_editor_midl_metadata_root_native)
file(TO_NATIVE_PATH "${_terminal_settings_editor_source_root}" _terminal_settings_editor_source_root_native)
set(_terminal_settings_editor_midl_reference_args)
set(_terminal_settings_editor_mdmerge_metadata_args)
foreach(_terminal_settings_editor_reference IN LISTS _terminal_settings_editor_reference_winmds)
    file(TO_NATIVE_PATH "${_terminal_settings_editor_reference}" _terminal_settings_editor_reference_native)
    list(APPEND _terminal_settings_editor_midl_reference_args /reference "${_terminal_settings_editor_reference_native}")
    get_filename_component(_terminal_settings_editor_reference_dir "${_terminal_settings_editor_reference}" DIRECTORY)
    list(APPEND _terminal_settings_editor_mdmerge_metadata_args -metadata_dir "${_terminal_settings_editor_reference_dir}")
endforeach()
set(_terminal_settings_editor_unmerged_winmds)
foreach(_terminal_settings_editor_idl IN LISTS _terminal_settings_editor_idls)
    get_filename_component(_terminal_settings_editor_idl_name "${_terminal_settings_editor_idl}" NAME_WE)
    set(_terminal_settings_editor_unmerged_winmd "${_terminal_settings_editor_unmerged_root}/${_terminal_settings_editor_idl_name}.winmd")
    file(TO_NATIVE_PATH "${_terminal_settings_editor_idl}" _terminal_settings_editor_idl_native)
    file(TO_NATIVE_PATH "${_terminal_settings_editor_unmerged_winmd}" _terminal_settings_editor_unmerged_winmd_native)
    add_custom_command(
        OUTPUT "${_terminal_settings_editor_unmerged_winmd}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_settings_editor_unmerged_root}"
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
            "${_portable_midl_path}"
            /I "${_terminal_connection_sdk_winrt_native}" /I "${_terminal_connection_sdk_shared_native}"
            /I "${_terminal_connection_sdk_um_native}" /I "${_terminal_settings_editor_source_root_native}"
            ${_terminal_settings_editor_midl_reference_args}
            /metadata_dir "${_terminal_settings_editor_midl_metadata_root_native}"
            /winrt /W1 /nologo /char signed /env ${_portable_midl_environment}
            /winmd "${_terminal_settings_editor_unmerged_winmd_native}"
            /h NUL /dlldata NUL /iid NUL /proxy NUL /notlb /client none /server none
            /enum_class /ns_prefix /target NT60 /nomidl "${_terminal_settings_editor_idl_native}"
        DEPENDS ${_terminal_settings_editor_idls} ${_terminal_settings_editor_reference_winmds} "${_portable_midl_path}"
        VERBATIM COMMENT "Generating Settings Editor ${_terminal_settings_editor_idl_name}.winmd"
    )
    list(APPEND _terminal_settings_editor_unmerged_winmds "${_terminal_settings_editor_unmerged_winmd}")
endforeach()
add_custom_command(
    OUTPUT "${_terminal_settings_editor_winmd}" "${_terminal_settings_editor_module_cpp}" "${_terminal_settings_editor_projection_header}"
    BYPRODUCTS "${_terminal_settings_editor_component_root}/XamlMetaDataProvider.cpp"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_settings_editor_output_root}" "${_terminal_settings_editor_projection_root}"
    COMMAND "${_portable_mdmerge_path}" -i "${_terminal_settings_editor_unmerged_root}" -o "${_terminal_settings_editor_output_root}"
        ${_terminal_settings_editor_mdmerge_metadata_args} -partial
    COMMAND "${_cppwinrt_path}"
        -input "${_terminal_settings_editor_winmd}" -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -reference "${_ui_helpers_winmd}" -reference "${_terminal_connection_winmd}" -reference "${_terminal_settings_model_winmd}"
        -reference "${_terminal_control_winmd}" -reference "${_terminal_core_winmd}"
        -reference "${_ui_helpers_mux_winmd}" -reference "${_ui_helpers_webview2_winmd}"
        -output "${_terminal_settings_editor_projection_root}" -component "${_terminal_settings_editor_component_root}"
        -overwrite -name Microsoft.Terminal.Settings.Editor -pch pch.h -prefix -optimize
    DEPENDS ${_terminal_settings_editor_unmerged_winmds} ${_terminal_settings_editor_reference_winmds} "${_portable_mdmerge_path}" "${_cppwinrt_path}"
    VERBATIM COMMENT "Merging Settings Editor WinMD and generating its component projection"
)
add_custom_command(
    OUTPUT "${_terminal_settings_editor_reference_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_settings_editor_reference_root}"
    COMMAND "${_cppwinrt_path}"
        -input "${_ui_helpers_winmd}" -input "${_terminal_connection_winmd}" -input "${_terminal_settings_model_winmd}"
        -input "${_terminal_control_winmd}" -input "${_terminal_core_winmd}"
        -input "${_ui_helpers_mux_winmd}" -input "${_ui_helpers_webview2_winmd}"
        -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}" -output "${_terminal_settings_editor_reference_root}"
    DEPENDS ${_terminal_settings_editor_product_reference_winmds} "${_cppwinrt_path}"
    VERBATIM COMMENT "Generating Settings Editor dependency reference projection"
)
set(_terminal_settings_editor_page_names
    Actions EditAction AddProfile CommonResources GlobalAppearance ColorSchemes NullableColorPicker IconPicker
    EditColorScheme Interaction Compatibility KeyChordListener Launch NewTabMenu MainPage Extensions Profiles_Base
    Profiles_Base_Orphaned Profiles_Advanced Profiles_Appearance Profiles_Terminal Appearances Rendering SettingContainerStyle
)
set(_terminal_settings_editor_xaml_pages)
set(_terminal_settings_editor_xaml_page_args)
set(_terminal_settings_editor_xaml_outputs)
set(_terminal_settings_editor_xbf_outputs)
foreach(_terminal_settings_editor_page_name IN LISTS _terminal_settings_editor_page_names)
    set(_terminal_settings_editor_page "${_terminal_settings_editor_source_root}/${_terminal_settings_editor_page_name}.xaml")
    list(APPEND _terminal_settings_editor_xaml_pages "${_terminal_settings_editor_page}")
    list(APPEND _terminal_settings_editor_xaml_page_args --page "${_terminal_settings_editor_page}")
    list(APPEND _terminal_settings_editor_xaml_outputs "${_terminal_settings_editor_projection_root}/${_terminal_settings_editor_page_name}.xaml.g.hpp")
    list(APPEND _terminal_settings_editor_xbf_outputs "${_terminal_settings_editor_projection_root}/${_terminal_settings_editor_page_name}.xbf")
endforeach()
file(GLOB _terminal_settings_editor_headers CONFIGURE_DEPENDS "${_terminal_settings_editor_source_root}/*.h")
set(_terminal_settings_editor_xaml_header_args)
foreach(_terminal_settings_editor_header IN LISTS _terminal_settings_editor_headers)
    get_filename_component(_terminal_settings_editor_header_file "${_terminal_settings_editor_header}" NAME)
    get_filename_component(_terminal_settings_editor_header_name "${_terminal_settings_editor_header}" NAME_WE)
    set(_terminal_settings_editor_header_parent "")
    if(_terminal_settings_editor_header_name IN_LIST _terminal_settings_editor_page_names)
        set(_terminal_settings_editor_header_parent "${_terminal_settings_editor_header_name}.xaml")
    elseif(_terminal_settings_editor_header_name IN_LIST _terminal_settings_editor_idl_names)
        set(_terminal_settings_editor_header_parent "${_terminal_settings_editor_header_name}.idl")
    elseif(_terminal_settings_editor_header_name STREQUAL "ArgsTemplateSelectors")
        set(_terminal_settings_editor_header_parent "ActionsViewModel.idl")
    endif()
    list(APPEND _terminal_settings_editor_xaml_header_args --cl-include "${_terminal_settings_editor_header_file}#${_terminal_settings_editor_header_parent}#${_terminal_settings_editor_header}")
endforeach()
set(_terminal_settings_editor_xaml_product_reference_args)
foreach(_terminal_settings_editor_reference IN LISTS _terminal_settings_editor_product_reference_winmds)
    get_filename_component(_terminal_settings_editor_product_reference_name "${_terminal_settings_editor_reference}" NAME)
    if(NOT _terminal_settings_editor_product_reference_name STREQUAL "Microsoft.Web.WebView2.Core.winmd")
        list(APPEND _terminal_settings_editor_xaml_product_reference_args --winmd-reference "${_terminal_settings_editor_reference}")
    endif()
endforeach()
set(_terminal_settings_editor_xaml_sdk_reference_args)
foreach(_terminal_settings_editor_reference IN LISTS _terminal_control_xaml_sdk_contract_winmds)
    list(APPEND _terminal_settings_editor_xaml_sdk_reference_args --system-reference "${_terminal_settings_editor_reference}")
endforeach()
set(_terminal_settings_editor_xaml_common_args
    --framework-runtime-dir "${_xaml_framework_runtime_dir}" --task-assembly "${_xaml_compiler_task}" --language CppWinRT
    --output "${_terminal_settings_editor_projection_root}" --output-type dynamiclibrary
    --root-namespace Microsoft.Terminal.Settings.Editor --windows-sdk-path "${_portable_sdk_root}"
    --vc-install-dir "${_portable_vc_root}" --target-platform-min-version 10.0.18362.0
    --project-path "${_terminal_settings_editor_source_root}/CMakeLists.txt"
    --project-name Microsoft.Terminal.Settings.Editor --pri-index-name Microsoft.Terminal.Settings.Editor
    ${_terminal_settings_editor_xaml_page_args} --include "${_terminal_settings_editor_source_root}"
)
set(_terminal_settings_editor_xaml_typeinfo "${_terminal_settings_editor_projection_root}/XamlTypeInfo.g.cpp")
set(_terminal_settings_editor_xaml_typeinfo_impl "${_terminal_settings_editor_projection_root}/XamlTypeInfo.Impl.g.cpp")
set(_terminal_settings_editor_xaml_provider_cpp "${_terminal_settings_editor_projection_root}/XamlMetaDataProvider.cpp")
file(GENERATE OUTPUT "${_terminal_settings_editor_xaml_provider_cpp}" CONTENT
"// This file is generated by the build to support Xaml apps\n#include \"pch.h\"\n#include \"XamlMetaDataProvider.h\"\n#include \"XamlMetaDataProvider.g.cpp\"\n")
add_custom_command(
    OUTPUT ${_terminal_settings_editor_xaml_outputs} ${_terminal_settings_editor_xbf_outputs}
        "${_terminal_settings_editor_xaml_typeinfo}" "${_terminal_settings_editor_xaml_typeinfo_impl}"
    BYPRODUCTS "${_terminal_settings_editor_projection_root}/XamlBindingInfo.xaml.g.hpp"
        "${_terminal_settings_editor_projection_root}/XamlMetaDataProvider.h"
        "${_terminal_settings_editor_projection_root}/XamlSaveStateFile.xml"
    COMMAND "${_xaml_compiler_host}" ${_terminal_settings_editor_xaml_common_args}
        ${_terminal_settings_editor_xaml_product_reference_args} ${_terminal_settings_editor_xaml_sdk_reference_args}
        --reference "${_terminal_control_xaml_facade_winmd}" --compile-mode RealBuildPass1
    COMMAND "${_xaml_compiler_host}" ${_terminal_settings_editor_xaml_common_args}
        ${_terminal_settings_editor_xaml_product_reference_args} ${_terminal_settings_editor_xaml_sdk_reference_args}
        --reference "${_terminal_control_xaml_facade_winmd}" --compile-mode RealBuildPass2
        --platform-xml-dir "${_portable_sdk_root}/Platforms/UAP/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        --local-assembly "${_terminal_settings_editor_winmd}" ${_terminal_settings_editor_xaml_header_args}
    DEPENDS "${_xaml_compiler_host}" "${_terminal_settings_editor_winmd}" ${_terminal_settings_editor_xaml_pages}
        ${_terminal_settings_editor_headers} ${_terminal_settings_editor_reference_winmds} "${_terminal_control_xaml_facade_winmd}"
    VERBATIM COMMENT "Generating Settings Editor XAML code and XBF without MSBuild"
)
add_custom_target(TerminalSettingsEditorXaml DEPENDS ${_terminal_settings_editor_xaml_outputs} ${_terminal_settings_editor_xbf_outputs}
    "${_terminal_settings_editor_xaml_typeinfo}" "${_terminal_settings_editor_xaml_typeinfo_impl}")
set(_terminal_settings_editor_generated_index_header "${_terminal_settings_editor_projection_root}/GeneratedSettingsIndex.g.h")
set(_terminal_settings_editor_generated_index_cpp "${_terminal_settings_editor_projection_root}/GeneratedSettingsIndex.g.cpp")
add_custom_command(
    OUTPUT "${_terminal_settings_editor_generated_index_header}" "${_terminal_settings_editor_generated_index_cpp}"
    COMMAND pwsh.exe -NoProfile -ExecutionPolicy Unrestricted "${_openconsole_root}/tools/GenerateSettingsIndex.ps1"
        -SourceDir "${_terminal_settings_editor_source_root}" -OutputDir "${_terminal_settings_editor_projection_root}"
    DEPENDS ${_terminal_settings_editor_xaml_pages} "${_openconsole_root}/tools/GenerateSettingsIndex.ps1"
    VERBATIM COMMENT "Generating Settings Editor search index"
)
set(_terminal_settings_editor_pri "${_terminal_settings_editor_output_root}/Microsoft.Terminal.Settings.Editor.pri")
set(_terminal_settings_editor_pri_root "${_terminal_settings_editor_generation_root}/pri-root")
file(GLOB _terminal_settings_editor_resw CONFIGURE_DEPENDS "${_terminal_settings_editor_source_root}/Resources/*/Resources.resw")
set(_terminal_settings_editor_resources_resfiles "")
foreach(_terminal_settings_editor_resource IN LISTS _terminal_settings_editor_resw)
    file(TO_NATIVE_PATH "${_terminal_settings_editor_resource}" _terminal_settings_editor_resource_native)
    string(APPEND _terminal_settings_editor_resources_resfiles "${_terminal_settings_editor_resource_native}\n")
endforeach()
set(_terminal_settings_editor_resources_list "${_terminal_settings_editor_generation_root}/resources.resfiles")
set(_terminal_settings_editor_embed_list "${_terminal_settings_editor_pri_root}/embed.resfiles")
set(_terminal_settings_editor_priconfig "${_terminal_settings_editor_generation_root}/priconfig.xml")
set(_terminal_settings_editor_embed_content "")
foreach(_terminal_settings_editor_page_name IN LISTS _terminal_settings_editor_page_names)
    string(APPEND _terminal_settings_editor_embed_content "Microsoft.Terminal.Settings.Editor\\${_terminal_settings_editor_page_name}.xbf\n")
endforeach()
file(TO_NATIVE_PATH "${_terminal_settings_editor_resources_list}" _terminal_settings_editor_resources_list_native)
file(GENERATE OUTPUT "${_terminal_settings_editor_resources_list}" CONTENT "${_terminal_settings_editor_resources_resfiles}")
file(GENERATE OUTPUT "${_terminal_settings_editor_embed_list}" CONTENT "${_terminal_settings_editor_embed_content}")
file(GENERATE OUTPUT "${_terminal_settings_editor_priconfig}" CONTENT
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources targetOsVersion=\"10.0.0\" majorVersion=\"1\"><index root=\"\\\" startIndexAt=\"${_terminal_settings_editor_resources_list_native}\"><default><qualifier name=\"Language\" value=\"en-US\" /><qualifier name=\"Contrast\" value=\"standard\" /><qualifier name=\"Scale\" value=\"200\" /><qualifier name=\"HomeRegion\" value=\"001\" /><qualifier name=\"TargetSize\" value=\"256\" /><qualifier name=\"LayoutDirection\" value=\"LTR\" /><qualifier name=\"DXFeatureLevel\" value=\"DX9\" /><qualifier name=\"Configuration\" value=\"\" /><qualifier name=\"AlternateForm\" value=\"\" /><qualifier name=\"Platform\" value=\"UAP\" /></default><indexer-config type=\"RESW\" convertDotsToSlashes=\"true\" initialPath=\"Microsoft.Terminal.Settings.Editor\" /><indexer-config type=\"RESFILES\" qualifierDelimiter=\".\" /></index><index root=\"${_terminal_settings_editor_pri_root}\" startIndexAt=\"embed.resfiles\"><default><qualifier name=\"Language\" value=\"en-US\" /><qualifier name=\"Contrast\" value=\"standard\" /><qualifier name=\"Scale\" value=\"200\" /><qualifier name=\"HomeRegion\" value=\"001\" /><qualifier name=\"TargetSize\" value=\"256\" /><qualifier name=\"LayoutDirection\" value=\"LTR\" /><qualifier name=\"DXFeatureLevel\" value=\"DX9\" /><qualifier name=\"Configuration\" value=\"\" /><qualifier name=\"AlternateForm\" value=\"\" /><qualifier name=\"Platform\" value=\"UAP\" /></default><indexer-config type=\"RESFILES\" qualifierDelimiter=\".\" /><indexer-config type=\"EMBEDFILES\" /></index></resources>\n")
set(_terminal_settings_editor_staged_xbfs)
set(_terminal_settings_editor_stage_commands)
list(LENGTH _terminal_settings_editor_page_names _terminal_settings_editor_page_count)
math(EXPR _terminal_settings_editor_page_last "${_terminal_settings_editor_page_count} - 1")
foreach(_terminal_settings_editor_page_index RANGE ${_terminal_settings_editor_page_last})
    list(GET _terminal_settings_editor_page_names ${_terminal_settings_editor_page_index} _terminal_settings_editor_page_name)
    list(GET _terminal_settings_editor_xbf_outputs ${_terminal_settings_editor_page_index} _terminal_settings_editor_xbf)
    set(_terminal_settings_editor_staged_xbf "${_terminal_settings_editor_pri_root}/Microsoft.Terminal.Settings.Editor/${_terminal_settings_editor_page_name}.xbf")
    list(APPEND _terminal_settings_editor_staged_xbfs "${_terminal_settings_editor_staged_xbf}")
    list(APPEND _terminal_settings_editor_stage_commands COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_terminal_settings_editor_xbf}" "${_terminal_settings_editor_staged_xbf}")
endforeach()
add_custom_command(
    OUTPUT "${_terminal_settings_editor_pri}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_settings_editor_output_root}" "${_terminal_settings_editor_pri_root}/Microsoft.Terminal.Settings.Editor"
    ${_terminal_settings_editor_stage_commands}
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new /pr "${_terminal_settings_editor_pri_root}" /cf "${_terminal_settings_editor_priconfig}" /of "${_terminal_settings_editor_pri}" /in Microsoft.Terminal.Settings.Editor /o
    DEPENDS ${_terminal_settings_editor_resw} ${_terminal_settings_editor_xbf_outputs}
        "${_terminal_settings_editor_resources_list}" "${_terminal_settings_editor_embed_list}" "${_terminal_settings_editor_priconfig}"
    VERBATIM COMMENT "Generating Microsoft.Terminal.Settings.Editor.pri"
)
set(_terminal_settings_editor_source_names
    init Actions ArgsTemplateSelectors EditAction AddProfile GlobalAppearance ColorSchemes NullableColorPicker IconPicker
    EditColorScheme Interaction Compatibility KeyChordListener Launch NewTabMenu pch SearchIndex MainPage ProfileViewModel
    ActionsViewModel TerminalColorConverters ColorSchemeViewModel ColorSchemesPageViewModel RenderingViewModel
    InteractionViewModel GlobalAppearanceViewModel LaunchViewModel NewTabMenuViewModel Extensions Profiles_Base
    Profiles_Base_Orphaned Profiles_Advanced Profiles_Appearance Profiles_Terminal Appearances Rendering SettingContainer
    Utils PreviewConnection
)
set(_terminal_settings_editor_sources)
foreach(_terminal_settings_editor_source_name IN LISTS _terminal_settings_editor_source_names)
    list(APPEND _terminal_settings_editor_sources "${_terminal_settings_editor_source_root}/${_terminal_settings_editor_source_name}.cpp")
endforeach()
list(APPEND _terminal_settings_editor_sources "${_terminal_settings_editor_source_root}/../fzf/fzf.cpp"
    "${_terminal_settings_editor_module_cpp}" "${_terminal_settings_editor_xaml_provider_cpp}"
    "${_terminal_settings_editor_xaml_typeinfo}" "${_terminal_settings_editor_xaml_typeinfo_impl}"
    "${_terminal_settings_editor_generated_index_cpp}")
set_source_files_properties("${_terminal_settings_editor_module_cpp}" "${_terminal_settings_editor_xaml_provider_cpp}"
    "${_terminal_settings_editor_xaml_typeinfo}" "${_terminal_settings_editor_xaml_typeinfo_impl}"
    "${_terminal_settings_editor_generated_index_cpp}" "${_terminal_settings_editor_pri}" PROPERTIES GENERATED TRUE)
add_library(${_terminal_settings_editor_target} SHARED ${_terminal_settings_editor_sources}
    "${_terminal_settings_editor_source_root}/Microsoft.Terminal.Settings.Editor.def"
    "${_terminal_settings_editor_pri}" "${_terminal_settings_editor_reference_header}")
add_dependencies(${_terminal_settings_editor_target} TerminalSettingsEditorXaml ${_terminal_settings_adapter_target})
target_compile_features(${_terminal_settings_editor_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_settings_editor_target} PRIVATE "${_terminal_settings_editor_source_root}/pch.h")
target_include_directories(${_terminal_settings_editor_target} PRIVATE
    "${_terminal_settings_editor_reference_root}" "${_terminal_settings_editor_projection_root}" "${_terminal_settings_editor_source_root}"
    "${_terminal_settings_model_projection_root}" "${_terminal_control_projection_root}" "${_terminal_core_projection_root}"
    "${_terminal_connection_projection_root}" "${_ui_helpers_projection_root}" "${_native_cppwinrt_projection_root}"
    "${_wil_package_dir}/include" "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/types/inc" "${_openconsole_root}/src/inc" "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K" "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include" "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_compile_definitions(${_terminal_settings_editor_target} PRIVATE
    _USRDLL _WINDLL _WINRT_DLL _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__
)
target_compile_options(${_terminal_settings_editor_target} PRIVATE ${_native_product_compile_options}
    /W3 /Zc:preprocessor /bigobj /wd4702 /wd5104 /wd28204 /wd4201 /wd4312 /wd5105
    /wd26434 /wd26445 /wd26456 /wd26478 /wd26494 "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
target_link_directories(${_terminal_settings_editor_target} PRIVATE "${_workspace_extension_vcpkg_lib_root}" "${_workspace_extension_vcpkg_manual_lib_root}" ${_portable_msvc_lib_dirs})
target_link_libraries(${_terminal_settings_editor_target} PRIVATE ${_terminal_settings_adapter_target} ${_terminal_settings_model_target}
    ConTypes WinRTUtils ConInt ConProps "${_workspace_extension_vcpkg_lib_root}/fmt.lib"
    "${_workspace_extension_vcpkg_lib_root}/jsoncpp.lib" shell32.lib onecoreuap.lib WindowsApp.lib ntdll.lib shlwapi.lib pathcch.lib propsys.lib dwmapi.lib)
target_link_options(${_terminal_settings_editor_target} PRIVATE /SUBSYSTEM:WINDOWS /MANIFEST:NO /guard:cf /DEBUG:FULL
    /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT "/INCLUDE:${_portable_dllmain_linker_symbol}")
set_target_properties(${_terminal_settings_editor_target} PROPERTIES OUTPUT_NAME "Microsoft.Terminal.Settings.Editor"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" RUNTIME_OUTPUT_DIRECTORY "${_terminal_settings_editor_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_terminal_settings_editor_output_root}" ARCHIVE_OUTPUT_DIRECTORY "${_terminal_settings_editor_output_root}"
    PDB_OUTPUT_DIRECTORY "${_terminal_settings_editor_output_root}")

set(_terminal_app_lib_target TerminalAppLib)
set(_terminal_app_target TerminalApp)
set(_terminal_app_source_root "${_openconsole_root}/src/cascadia/TerminalApp")
set(_terminal_app_lib_output_root "${_native_product_output_root}/TerminalAppLib")
set(_terminal_app_output_root "${_native_product_output_root}/TerminalApp")
set(_terminal_app_generation_root "${CMAKE_BINARY_DIR}/terminal-app/$<CONFIG>")
set(_terminal_app_unmerged_root "${_terminal_app_generation_root}/unmerged")
set(_terminal_app_projection_root "${_terminal_app_generation_root}/projection")
set(_terminal_app_component_root "${_terminal_app_projection_root}/sources")
set(_terminal_app_reference_root "${_terminal_app_generation_root}/references")
set(_terminal_app_winmd "${_terminal_app_lib_output_root}/TerminalApp.winmd")
set(_terminal_app_module_cpp "${_terminal_app_projection_root}/module.g.cpp")
set(_terminal_app_projection_header "${_terminal_app_projection_root}/winrt/TerminalApp.h")
set(_terminal_app_reference_header "${_terminal_app_reference_root}/winrt/Microsoft.Terminal.Control.h")
set(_terminal_app_xaml_metadata_idl "${_terminal_app_generation_root}/XamlMetaDataProvider.idl")
file(GENERATE OUTPUT "${_terminal_app_xaml_metadata_idl}" CONTENT
"// Generated by CMake for the Windows XAML metadata provider.\nnamespace TerminalApp\n{\n    runtimeclass XamlMetaDataProvider : [default] Windows.UI.Xaml.Markup.IXamlMetadataProvider\n    {\n        XamlMetaDataProvider();\n    }\n}\n")
set(_terminal_app_idl_names
    AboutDialog App Remoting PaletteItemTemplateSelector IPaletteItem ShortcutActionDispatch AppKeyBindings AppLogic
    TerminalWindow MinMaxCloseControl Tab TaskbarState TerminalPage TerminalTabStatus TitlebarControl TabRowControl
    TabHeaderControl HighlightedTextControl ColorPickupFlyout CommandPalette SuggestionsControl FilteredCommand
    IPaneContent TerminalPaneContent MarkdownPaneContent WorkspaceIconPickerDialog)
set(_terminal_app_idls)
foreach(_terminal_app_idl_name IN LISTS _terminal_app_idl_names)
    list(APPEND _terminal_app_idls "${_terminal_app_source_root}/${_terminal_app_idl_name}.idl")
endforeach()
list(APPEND _terminal_app_idls "${_terminal_app_xaml_metadata_idl}")
set(_terminal_app_product_reference_winmds
    "${_terminal_connection_winmd}" "${_terminal_control_winmd}" "${_terminal_settings_editor_winmd}"
    "${_terminal_settings_model_winmd}" "${_terminal_core_winmd}" "${_ui_markdown_winmd}" "${_ui_helpers_winmd}"
    "${_ui_helpers_mux_winmd}" "${_ui_helpers_webview2_winmd}"
    "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Windows.UI.Xaml.Hosting.HostingContract/5.0.0.0/Windows.UI.Xaml.Hosting.HostingContract.winmd"
    "${_openconsole_root}/packages/Microsoft.WindowsPackageManager.ComInterop.1.8.1911/lib/Microsoft.Management.Deployment.winmd")
set(_terminal_app_reference_winmds ${_terminal_app_product_reference_winmds} ${_terminal_control_xaml_sdk_contract_winmds})
set(_terminal_app_midl_metadata_root "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Windows.Foundation.FoundationContract/4.0.0.0")
file(TO_NATIVE_PATH "${_terminal_app_midl_metadata_root}" _terminal_app_midl_metadata_root_native)
file(TO_NATIVE_PATH "${_terminal_app_source_root}" _terminal_app_source_root_native)
set(_terminal_app_midl_reference_args)
set(_terminal_app_mdmerge_metadata_args)
foreach(_terminal_app_reference IN LISTS _terminal_app_reference_winmds)
    file(TO_NATIVE_PATH "${_terminal_app_reference}" _terminal_app_reference_native)
    list(APPEND _terminal_app_midl_reference_args /reference "${_terminal_app_reference_native}")
    get_filename_component(_terminal_app_reference_dir "${_terminal_app_reference}" DIRECTORY)
    list(APPEND _terminal_app_mdmerge_metadata_args -metadata_dir "${_terminal_app_reference_dir}")
endforeach()
set(_terminal_app_unmerged_winmds)
foreach(_terminal_app_idl IN LISTS _terminal_app_idls)
    get_filename_component(_terminal_app_idl_name "${_terminal_app_idl}" NAME_WE)
    set(_terminal_app_unmerged_winmd "${_terminal_app_unmerged_root}/${_terminal_app_idl_name}.winmd")
    file(TO_NATIVE_PATH "${_terminal_app_idl}" _terminal_app_idl_native)
    file(TO_NATIVE_PATH "${_terminal_app_unmerged_winmd}" _terminal_app_unmerged_winmd_native)
    add_custom_command(
        OUTPUT "${_terminal_app_unmerged_winmd}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_app_unmerged_root}"
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
            "${_portable_midl_path}" /I "${_terminal_connection_sdk_winrt_native}" /I "${_terminal_connection_sdk_shared_native}"
            /I "${_terminal_connection_sdk_um_native}" /I "${_terminal_app_source_root_native}"
            ${_terminal_app_midl_reference_args} /metadata_dir "${_terminal_app_midl_metadata_root_native}"
            /winrt /W1 /nologo /char signed /env ${_portable_midl_environment} /winmd "${_terminal_app_unmerged_winmd_native}"
            /h NUL /dlldata NUL /iid NUL /proxy NUL /notlb /client none /server none /enum_class /ns_prefix /target NT60 /nomidl "${_terminal_app_idl_native}"
        DEPENDS ${_terminal_app_idls} ${_terminal_app_reference_winmds} "${_portable_midl_path}"
        VERBATIM COMMENT "Generating TerminalApp ${_terminal_app_idl_name}.winmd")
    list(APPEND _terminal_app_unmerged_winmds "${_terminal_app_unmerged_winmd}")
endforeach()
add_custom_command(
    OUTPUT "${_terminal_app_winmd}" "${_terminal_app_module_cpp}" "${_terminal_app_projection_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_app_lib_output_root}" "${_terminal_app_projection_root}"
    COMMAND "${_portable_mdmerge_path}" -i "${_terminal_app_unmerged_root}" -o "${_terminal_app_lib_output_root}" ${_terminal_app_mdmerge_metadata_args} -partial
    COMMAND "${_cppwinrt_path}" -input "${_terminal_app_winmd}" -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -reference "${_terminal_connection_winmd}" -reference "${_terminal_control_winmd}" -reference "${_terminal_settings_editor_winmd}"
        -reference "${_terminal_settings_model_winmd}" -reference "${_terminal_core_winmd}" -reference "${_ui_markdown_winmd}"
        -reference "${_ui_helpers_winmd}" -reference "${_ui_helpers_mux_winmd}" -reference "${_ui_helpers_webview2_winmd}"
        -output "${_terminal_app_projection_root}" -component "${_terminal_app_component_root}"
        -overwrite -name TerminalApp -pch pch.h -prefix -optimize
    DEPENDS ${_terminal_app_unmerged_winmds} ${_terminal_app_reference_winmds} "${_portable_mdmerge_path}" "${_cppwinrt_path}"
    VERBATIM COMMENT "Merging TerminalApp WinMD and generating its component projection")
add_custom_command(
    OUTPUT "${_terminal_app_reference_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_app_reference_root}"
    COMMAND "${_cppwinrt_path}" -input ${_terminal_app_product_reference_winmds}
        -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}" -output "${_terminal_app_reference_root}"
    DEPENDS ${_terminal_app_product_reference_winmds} "${_cppwinrt_path}"
    VERBATIM COMMENT "Generating TerminalApp dependency reference projection")

set(_terminal_app_page_names AboutDialog MinMaxCloseControl TerminalPage TitlebarControl TabRowControl TabHeaderControl
    HighlightedTextControlStyle ColorPickupFlyout CommandPalette SuggestionsControl SnippetsPaneContent MarkdownPaneContent
    WorkspaceSettingsResources WorkspaceIconPickerDialog)
set(_terminal_app_xaml_pages)
set(_terminal_app_xaml_page_args)
set(_terminal_app_xaml_outputs "${_terminal_app_projection_root}/App.xaml.g.hpp")
set(_terminal_app_xbf_outputs "${_terminal_app_projection_root}/App.xbf")
foreach(_terminal_app_page_name IN LISTS _terminal_app_page_names)
    if(_terminal_app_page_name STREQUAL "WorkspaceSettingsResources")
        set(_terminal_app_page "${_workspace_root}/src/glue/workspace/WorkspaceSettingsResources.xaml")
    else()
        set(_terminal_app_page "${_terminal_app_source_root}/${_terminal_app_page_name}.xaml")
    endif()
    list(APPEND _terminal_app_xaml_pages "${_terminal_app_page}")
    list(APPEND _terminal_app_xaml_page_args --page "${_terminal_app_page}")
    list(APPEND _terminal_app_xaml_outputs "${_terminal_app_projection_root}/${_terminal_app_page_name}.xaml.g.hpp")
    list(APPEND _terminal_app_xbf_outputs "${_terminal_app_projection_root}/${_terminal_app_page_name}.xbf")
endforeach()
set(_terminal_app_xaml_header_args)
file(GLOB _terminal_app_headers CONFIGURE_DEPENDS "${_terminal_app_source_root}/*.h")
foreach(_terminal_app_header IN LISTS _terminal_app_headers)
    get_filename_component(_terminal_app_header_file "${_terminal_app_header}" NAME)
    get_filename_component(_terminal_app_header_name "${_terminal_app_header}" NAME_WE)
    if(_terminal_app_header_file STREQUAL "App.base.h")
        continue()
    endif()
    set(_terminal_app_header_parent "")
    if(_terminal_app_header_name IN_LIST _terminal_app_page_names OR _terminal_app_header_name STREQUAL "App")
        set(_terminal_app_header_parent "${_terminal_app_header_name}.xaml")
    elseif(_terminal_app_header_name IN_LIST _terminal_app_idl_names)
        set(_terminal_app_header_parent "${_terminal_app_header_name}.idl")
    elseif(_terminal_app_header_name STREQUAL "ContentManager")
        set(_terminal_app_header_parent "TerminalPage.idl")
    elseif(_terminal_app_header_name MATCHES "SettingsLoadEventArgs")
        set(_terminal_app_header_parent "TerminalWindow.idl")
    elseif(_terminal_app_header_name MATCHES "ScratchpadContent|SettingsPaneContent|WorkspaceManagerPaneContent")
        set(_terminal_app_header_parent "TerminalPaneContent.idl")
    endif()
    list(APPEND _terminal_app_xaml_header_args --cl-include "${_terminal_app_header_file}#${_terminal_app_header_parent}#${_terminal_app_header}")
endforeach()
set(_terminal_app_xaml_product_reference_args)
foreach(_terminal_app_reference IN LISTS _terminal_app_product_reference_winmds)
    get_filename_component(_terminal_app_reference_name "${_terminal_app_reference}" NAME)
    if(NOT _terminal_app_reference_name STREQUAL "Microsoft.Web.WebView2.Core.winmd")
        list(APPEND _terminal_app_xaml_product_reference_args --winmd-reference "${_terminal_app_reference}")
    endif()
endforeach()
set(_terminal_app_xaml_sdk_reference_args)
foreach(_terminal_app_reference IN LISTS _terminal_control_xaml_sdk_contract_winmds)
    list(APPEND _terminal_app_xaml_sdk_reference_args --system-reference "${_terminal_app_reference}")
endforeach()
set(_terminal_app_xaml_common_args
    --framework-runtime-dir "${_xaml_framework_runtime_dir}" --task-assembly "${_xaml_compiler_task}" --language CppWinRT
    --output "${_terminal_app_projection_root}" --output-type staticlibrary --root-namespace TerminalApp
    --windows-sdk-path "${_portable_sdk_root}" --vc-install-dir "${_portable_vc_root}" --target-platform-min-version 10.0.18362.0
    --project-path "${_terminal_app_source_root}/CMakeLists.txt" --project-name TerminalAppLib --pri-index-name TerminalApp
    --code-generation-control-flags DoNotGenerateOtherProviders
    --application-definition "${_terminal_app_source_root}/App.xaml" ${_terminal_app_xaml_page_args} --include "${_terminal_app_source_root}")
set(_terminal_app_xaml_typeinfo "${_terminal_app_projection_root}/XamlTypeInfo.g.cpp")
set(_terminal_app_xaml_typeinfo_impl "${_terminal_app_projection_root}/XamlTypeInfo.Impl.g.cpp")
set(_terminal_app_xaml_provider_cpp "${_terminal_app_projection_root}/XamlMetaDataProvider.cpp")
file(GENERATE OUTPUT "${_terminal_app_xaml_provider_cpp}" CONTENT
"// Generated by CMake for the TerminalApp XAML metadata provider.\n#include \"pch.h\"\n#include \"XamlMetaDataProvider.h\"\n#include \"XamlMetaDataProvider.g.cpp\"\n")
add_custom_command(
    OUTPUT ${_terminal_app_xaml_outputs} ${_terminal_app_xbf_outputs} "${_terminal_app_xaml_typeinfo}" "${_terminal_app_xaml_typeinfo_impl}"
    BYPRODUCTS "${_terminal_app_projection_root}/XamlBindingInfo.xaml.g.hpp" "${_terminal_app_projection_root}/XamlSaveStateFile.xml"
    COMMAND "${_xaml_compiler_host}" ${_terminal_app_xaml_common_args} ${_terminal_app_xaml_product_reference_args}
        ${_terminal_app_xaml_sdk_reference_args} --reference "${_terminal_control_xaml_facade_winmd}" --compile-mode RealBuildPass1
    COMMAND "${_xaml_compiler_host}" ${_terminal_app_xaml_common_args} ${_terminal_app_xaml_product_reference_args}
        ${_terminal_app_xaml_sdk_reference_args} --reference "${_terminal_control_xaml_facade_winmd}" --compile-mode RealBuildPass2
        --platform-xml-dir "${_portable_sdk_root}/Platforms/UAP/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        --local-assembly "${_terminal_app_winmd}" ${_terminal_app_xaml_header_args}
    DEPENDS "${_xaml_compiler_host}" "${_terminal_app_winmd}" "${_terminal_app_source_root}/App.xaml"
        ${_terminal_app_xaml_pages} ${_terminal_app_headers} ${_terminal_app_reference_winmds} "${_terminal_control_xaml_facade_winmd}"
    VERBATIM COMMENT "Generating TerminalApp XAML code and XBF without MSBuild")
add_custom_target(TerminalAppXaml DEPENDS ${_terminal_app_xaml_outputs} ${_terminal_app_xbf_outputs}
    "${_terminal_app_xaml_typeinfo}" "${_terminal_app_xaml_typeinfo_impl}")

file(GLOB _terminal_app_resw CONFIGURE_DEPENDS "${_terminal_app_source_root}/Resources/*/*.resw")
set(_terminal_app_pri "${_terminal_app_lib_output_root}/TerminalApp.pri")
set(_terminal_app_pri_root "${_terminal_app_generation_root}/pri-root")
set(_terminal_app_resources_list "${_terminal_app_generation_root}/resources.resfiles")
set(_terminal_app_embed_list "${_terminal_app_pri_root}/embed.resfiles")
set(_terminal_app_priconfig "${_terminal_app_generation_root}/priconfig.xml")
set(_terminal_app_resources_content "")
foreach(_terminal_app_resource IN LISTS _terminal_app_resw)
    file(TO_NATIVE_PATH "${_terminal_app_resource}" _terminal_app_resource_native)
    string(APPEND _terminal_app_resources_content "${_terminal_app_resource_native}\n")
endforeach()
set(_terminal_app_embed_content "App.xbf\n")
foreach(_terminal_app_page_name IN LISTS _terminal_app_page_names)
    string(APPEND _terminal_app_embed_content "TerminalApp\\${_terminal_app_page_name}.xbf\n")
endforeach()
file(TO_NATIVE_PATH "${_terminal_app_resources_list}" _terminal_app_resources_list_native)
file(GENERATE OUTPUT "${_terminal_app_resources_list}" CONTENT "${_terminal_app_resources_content}")
file(GENERATE OUTPUT "${_terminal_app_embed_list}" CONTENT "${_terminal_app_embed_content}")
file(GENERATE OUTPUT "${_terminal_app_priconfig}" CONTENT
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources targetOsVersion=\"10.0.0\" majorVersion=\"1\"><index root=\"\\\" startIndexAt=\"${_terminal_app_resources_list_native}\"><default><qualifier name=\"Language\" value=\"en-US\" /><qualifier name=\"Contrast\" value=\"standard\" /><qualifier name=\"Scale\" value=\"200\" /><qualifier name=\"HomeRegion\" value=\"001\" /><qualifier name=\"TargetSize\" value=\"256\" /><qualifier name=\"LayoutDirection\" value=\"LTR\" /><qualifier name=\"DXFeatureLevel\" value=\"DX9\" /><qualifier name=\"Configuration\" value=\"\" /><qualifier name=\"AlternateForm\" value=\"\" /><qualifier name=\"Platform\" value=\"UAP\" /></default><indexer-config type=\"RESW\" convertDotsToSlashes=\"true\" initialPath=\"TerminalApp\" /><indexer-config type=\"RESFILES\" qualifierDelimiter=\".\" /></index><index root=\"${_terminal_app_pri_root}\" startIndexAt=\"embed.resfiles\"><default><qualifier name=\"Language\" value=\"en-US\" /><qualifier name=\"Contrast\" value=\"standard\" /><qualifier name=\"Scale\" value=\"200\" /><qualifier name=\"HomeRegion\" value=\"001\" /><qualifier name=\"TargetSize\" value=\"256\" /><qualifier name=\"LayoutDirection\" value=\"LTR\" /><qualifier name=\"DXFeatureLevel\" value=\"DX9\" /><qualifier name=\"Configuration\" value=\"\" /><qualifier name=\"AlternateForm\" value=\"\" /><qualifier name=\"Platform\" value=\"UAP\" /></default><indexer-config type=\"RESFILES\" qualifierDelimiter=\".\" /><indexer-config type=\"EMBEDFILES\" /></index></resources>\n")
set(_terminal_app_stage_commands COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_terminal_app_projection_root}/App.xbf" "${_terminal_app_pri_root}/App.xbf")
foreach(_terminal_app_page_name IN LISTS _terminal_app_page_names)
    list(APPEND _terminal_app_stage_commands COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_terminal_app_projection_root}/${_terminal_app_page_name}.xbf" "${_terminal_app_pri_root}/TerminalApp/${_terminal_app_page_name}.xbf")
endforeach()
add_custom_command(OUTPUT "${_terminal_app_pri}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_terminal_app_lib_output_root}" "${_terminal_app_pri_root}/TerminalApp"
    ${_terminal_app_stage_commands}
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new /pr "${_terminal_app_pri_root}" /cf "${_terminal_app_priconfig}" /of "${_terminal_app_pri}" /in TerminalApp /o
    DEPENDS ${_terminal_app_resw} ${_terminal_app_xbf_outputs} "${_terminal_app_resources_list}" "${_terminal_app_embed_list}" "${_terminal_app_priconfig}"
    VERBATIM COMMENT "Generating TerminalApp.pri")

set(_terminal_app_sources
    init.cpp AppCommandlineArgs.cpp Commandline.cpp CommandPaletteItems.cpp Jumplist.cpp LanguageProfileNotifier.cpp
    MinMaxCloseControl.cpp Remoting.cpp PaletteItemTemplateSelector.cpp Tab.cpp TaskbarState.cpp TerminalPage.cpp TabManagement.cpp
    ActionPreviewHandlers.cpp TerminalTabStatus.cpp TitlebarControl.cpp TabRowControl.cpp TabHeaderControl.cpp HighlightedTextControl.cpp
    ColorPickupFlyout.cpp CommandPalette.cpp FilteredCommand.cpp Pane.cpp Pane.LayoutSizeNode.cpp DebugTapConnection.cpp AppKeyBindings.cpp
    ShortcutActionDispatch.cpp AboutDialog.cpp App.cpp AppActionHandlers.cpp AppLogic.cpp ContentManager.cpp TerminalWindow.cpp
    TerminalPaneContent.cpp ScratchpadContent.cpp SnippetsPaneContent.cpp SettingsPaneContent.cpp WorkspaceManagerPaneContent.cpp
    Toast.cpp DesktopNotification.cpp TerminalSettingsCache.cpp SuggestionsControl.cpp MarkdownPaneContent.cpp WorkspaceIconPickerDialog.cpp)
list(TRANSFORM _terminal_app_sources PREPEND "${_terminal_app_source_root}/")
list(APPEND _terminal_app_sources "${_terminal_app_source_root}/../fzf/fzf.cpp"
    "${_terminal_app_source_root}/../TerminalSettingsModel/Workspace.cpp"
    "${_workspace_root}/src/glue/workspace/WorkspaceTerminalPageLegacyGlue.cpp"
    "${_workspace_root}/src/core/chat/TerminalInputHarness.cpp" "${_workspace_root}/src/core/chat/TerminalEventStore.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatController.cpp" "${_workspace_root}/src/core/chat/WorkspaceChatStateHelpers.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatTextHelpers.cpp" "${_workspace_root}/src/glue/chat/WorkspaceDiagnosticLog.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatStore.cpp" "${_workspace_root}/src/core/chat/WorkspaceStoragePaths.cpp"
    "${_terminal_app_module_cpp}" "${_terminal_app_xaml_provider_cpp}" "${_terminal_app_xaml_typeinfo}" "${_terminal_app_xaml_typeinfo_impl}")
set(_terminal_app_no_pch_sources
    "${_workspace_root}/src/core/chat/TerminalInputHarness.cpp" "${_workspace_root}/src/core/chat/TerminalEventStore.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatController.cpp" "${_workspace_root}/src/core/chat/WorkspaceChatStateHelpers.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatTextHelpers.cpp" "${_workspace_root}/src/core/chat/WorkspaceChatStore.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceStoragePaths.cpp")
set_source_files_properties(${_terminal_app_no_pch_sources} PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)
set_source_files_properties("${_terminal_app_module_cpp}" "${_terminal_app_xaml_provider_cpp}" "${_terminal_app_xaml_typeinfo}" "${_terminal_app_xaml_typeinfo_impl}" "${_terminal_app_pri}" PROPERTIES GENERATED TRUE)
add_library(${_terminal_app_lib_target} STATIC ${_terminal_app_sources} "${_terminal_app_reference_header}" "${_terminal_app_pri}")
add_dependencies(${_terminal_app_lib_target} TerminalAppXaml ${_terminal_settings_adapter_target})
target_compile_features(${_terminal_app_lib_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_app_lib_target} PRIVATE "${_terminal_app_source_root}/pch.h")
target_include_directories(${_terminal_app_lib_target} PRIVATE
    "${_terminal_app_reference_root}" "${_terminal_app_projection_root}" "${_terminal_app_source_root}" "${_terminal_app_source_root}/.."
    "${_terminal_settings_model_projection_root}" "${_terminal_control_projection_root}" "${_terminal_core_projection_root}"
    "${_terminal_connection_projection_root}" "${_terminal_settings_editor_projection_root}" "${_ui_markdown_projection_root}"
    "${_ui_helpers_projection_root}" "${_native_cppwinrt_projection_root}" "${_wil_package_dir}/include"
    "${_workspace_root}/src/core/chat" "${_workspace_root}/src/glue/chat" "${_workspace_root}/src/glue/workspace"
    "${_themehelpers_package_dir}/inc" "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/types/inc" "${_openconsole_root}/src/inc" "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K" "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include" "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs})
target_compile_definitions(${_terminal_app_lib_target} PRIVATE _LIB _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG DISABLE_XAML_GENERATED_MAIN
    _WINDOWS EXTERNAL_BUILD WT_WORKSPACE_HOST_BUILD _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__)
target_compile_options(${_terminal_app_lib_target} PRIVATE ${_native_product_archive_compile_options} /W3 /Zc:preprocessor /bigobj
    /wd4702 /wd5104 /wd28204 /wd4201 /wd4312 /wd5105 /wd26434 /wd26445 /wd26456 /wd26478 /wd26494
    "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
set_target_properties(${_terminal_app_lib_target} PROPERTIES OUTPUT_NAME TerminalAppLib INTERPROCEDURAL_OPTIMIZATION FALSE
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" ARCHIVE_OUTPUT_DIRECTORY "${_terminal_app_lib_output_root}")
add_library(${_terminal_app_target} SHARED "${_terminal_app_source_root}/dll/pch.cpp" "${_terminal_app_source_root}/dll/TerminalApp.def")
target_link_directories(${_terminal_app_target} PRIVATE "${_workspace_extension_vcpkg_lib_root}" "${_workspace_extension_vcpkg_manual_lib_root}" ${_portable_msvc_lib_dirs})
target_link_libraries(${_terminal_app_target} PRIVATE ${_terminal_app_lib_target} ${_terminal_settings_adapter_target}
    ${_terminal_settings_model_target} ${_terminal_control_target} ${_terminal_settings_editor_target} ${_ui_markdown_target}
    ConTypes WinRTUtils ConProps ConInt TerminalCore TerminalConnection MidiAudio
    "${_themehelpers_package_dir}/runtimes/win10-x64/lib/uap10.0/TerminalThemeHelpers.lib"
    "${_workspace_extension_vcpkg_lib_root}/fmt.lib" "${_workspace_extension_vcpkg_lib_root}/jsoncpp.lib"
    "${_workspace_extension_vcpkg_lib_root}/CLI11.lib"
    user32.lib onecoreuap.lib WindowsApp.lib shell32.lib winmm.lib ntdll.lib shlwapi.lib pathcch.lib propsys.lib dwmapi.lib)
target_link_options(${_terminal_app_target} PRIVATE /SUBSYSTEM:CONSOLE /MANIFEST:NO /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF
    /RELEASE /DYNAMICBASE /NXCOMPAT "/INCLUDE:${_portable_dllmain_linker_symbol}")
add_custom_command(TARGET ${_terminal_app_target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_terminal_app_winmd}" "${_terminal_app_output_root}/TerminalApp.winmd"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_terminal_app_pri}" "${_terminal_app_output_root}/TerminalApp.pri")
set_target_properties(${_terminal_app_target} PROPERTIES OUTPUT_NAME TerminalApp MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_terminal_app_output_root}" LIBRARY_OUTPUT_DIRECTORY "${_terminal_app_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_terminal_app_output_root}" PDB_OUTPUT_DIRECTORY "${_terminal_app_output_root}")

set(_windows_terminal_target WindowsTerminal)
set(_windows_terminal_source_root "${_openconsole_root}/src/cascadia/WindowsTerminal")
set(_windows_terminal_output_root "${_native_product_output_root}/WindowsTerminal")
set(_windows_terminal_unpacked_pri_root "${CMAKE_BINARY_DIR}/windows-terminal-pri/$<CONFIG>")
set(_windows_terminal_xaml_unpack_root "${CMAKE_BINARY_DIR}/windows-terminal-xaml/$<CONFIG>")
set(_windows_terminal_xaml_pri "${_windows_terminal_xaml_unpack_root}/resources.pri")
set(_windows_terminal_xaml_dll "${_windows_terminal_xaml_unpack_root}/Microsoft.UI.Xaml.dll")
set(_windows_terminal_unpacked_pri_list "${_windows_terminal_unpacked_pri_root}/pri.resfiles")
set(_windows_terminal_unpacked_dump_list "${_windows_terminal_unpacked_pri_root}/dump.resfiles")
set(_windows_terminal_unpacked_pri_config "${_windows_terminal_unpacked_pri_root}/priconfig.xml")
set(_windows_terminal_unpacked_resources "${_windows_terminal_output_root}/resources.pri")
set(_windows_terminal_unpacked_pris
    "${_windows_terminal_xaml_pri}"
    "${_terminal_control_pri}"
    "${_terminal_connection_pri}"
    "${_terminal_settings_model_pri}"
    "${_ui_helpers_pri}"
    "${_terminal_app_pri}"
    "${_terminal_settings_editor_pri}"
    "${_ui_markdown_pri}")
add_custom_command(
    OUTPUT "${_windows_terminal_xaml_pri}" "${_windows_terminal_xaml_dll}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_windows_terminal_xaml_unpack_root}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_windows_terminal_xaml_unpack_root}" "${_windows_terminal_output_root}"
    COMMAND "${_portable_makeappx_path}" unpack /p "${_portable_xaml_package}" /d "${_windows_terminal_xaml_unpack_root}" /o
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_windows_terminal_xaml_dll}" "${_windows_terminal_output_root}/Microsoft.UI.Xaml.dll"
    DEPENDS "${_portable_xaml_package}"
    VERBATIM
    COMMENT "Extracting the full Microsoft.UI.Xaml unpackaged runtime")
add_custom_target(windows-terminal-xaml-runtime DEPENDS "${_windows_terminal_xaml_pri}" "${_windows_terminal_xaml_dll}")
set(_windows_terminal_unpacked_native_pris)
foreach(_windows_terminal_unpacked_pri IN LISTS _windows_terminal_unpacked_pris)
    file(TO_NATIVE_PATH "${_windows_terminal_unpacked_pri}" _windows_terminal_unpacked_native_pri)
    list(APPEND _windows_terminal_unpacked_native_pris "${_windows_terminal_unpacked_native_pri}")
endforeach()
string(JOIN "\n" _windows_terminal_unpacked_pri_content ${_windows_terminal_unpacked_native_pris})
file(GENERATE OUTPUT "${_windows_terminal_unpacked_pri_list}" CONTENT "${_windows_terminal_unpacked_pri_content}\n")
file(GENERATE OUTPUT "${_windows_terminal_unpacked_dump_list}" CONTENT "")
file(GENERATE OUTPUT "${_windows_terminal_unpacked_pri_config}" CONTENT
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources targetOsVersion=\"10.0.0\" majorVersion=\"1\">\n  <index root=\"\\\" startIndexAt=\"dump.resfiles\"><default><qualifier name=\"Language\" value=\"en-US\"/><qualifier name=\"Contrast\" value=\"standard\"/><qualifier name=\"Scale\" value=\"200\"/><qualifier name=\"HomeRegion\" value=\"001\"/><qualifier name=\"TargetSize\" value=\"256\"/><qualifier name=\"LayoutDirection\" value=\"LTR\"/><qualifier name=\"DXFeatureLevel\" value=\"DX9\"/><qualifier name=\"Configuration\" value=\"\"/><qualifier name=\"AlternateForm\" value=\"\"/><qualifier name=\"Platform\" value=\"UAP\"/></default><indexer-config type=\"PRIINFO\"/><indexer-config type=\"RESFILES\" qualifierDelimiter=\".\"/></index>\n  <index root=\"\\\" startIndexAt=\"pri.resfiles\"><default><qualifier name=\"Language\" value=\"en-US\"/><qualifier name=\"Contrast\" value=\"standard\"/><qualifier name=\"Scale\" value=\"200\"/><qualifier name=\"HomeRegion\" value=\"001\"/><qualifier name=\"TargetSize\" value=\"256\"/><qualifier name=\"LayoutDirection\" value=\"LTR\"/><qualifier name=\"DXFeatureLevel\" value=\"DX9\"/><qualifier name=\"Configuration\" value=\"\"/><qualifier name=\"AlternateForm\" value=\"\"/><qualifier name=\"Platform\" value=\"UAP\"/></default><indexer-config type=\"PRI\"/><indexer-config type=\"RESFILES\" qualifierDelimiter=\".\"/></index>\n</resources>\n")
add_custom_command(
    OUTPUT "${_windows_terminal_unpacked_resources}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_windows_terminal_unpacked_pri_root}" "${_windows_terminal_output_root}"
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new /pr "${_windows_terminal_unpacked_pri_root}" /cf "${_windows_terminal_unpacked_pri_config}" /o /in Application /of "${_windows_terminal_unpacked_resources}"
    DEPENDS ${_windows_terminal_unpacked_pris} "${_windows_terminal_unpacked_pri_list}" "${_windows_terminal_unpacked_dump_list}" "${_windows_terminal_unpacked_pri_config}"
    VERBATIM
    COMMENT "Merging unpackaged WindowsTerminal resources")
add_custom_target(windows-terminal-unpackaged-resources DEPENDS "${_windows_terminal_unpacked_resources}")
set(_windows_terminal_sources
    "${_windows_terminal_source_root}/pch.cpp" "${_windows_terminal_source_root}/main.cpp"
    "${_windows_terminal_source_root}/AppHost.cpp" "${_windows_terminal_source_root}/IslandWindow.cpp"
    "${_windows_terminal_source_root}/NonClientIslandWindow.cpp" "${_windows_terminal_source_root}/VirtualDesktopUtils.cpp"
    "${_windows_terminal_source_root}/WindowEmperor.cpp" "${_windows_terminal_source_root}/icon.cpp"
    "${_windows_terminal_source_root}/WindowsTerminal.rc"
    "${_workspace_root}/src/glue/chat/WorkspaceDiagnosticLog.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatTextHelpers.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceStoragePaths.cpp")
set_source_files_properties(
    "${_workspace_root}/src/core/chat/WorkspaceChatTextHelpers.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceStoragePaths.cpp"
    PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)
add_executable(${_windows_terminal_target} WIN32 ${_windows_terminal_sources})
add_dependencies(${_windows_terminal_target} ${_terminal_app_target} windows-terminal-xaml-runtime windows-terminal-unpackaged-resources)
target_compile_features(${_windows_terminal_target} PRIVATE cxx_std_20)
target_precompile_headers(${_windows_terminal_target} PRIVATE "${_windows_terminal_source_root}/pch.h")
target_include_directories(${_windows_terminal_target} PRIVATE
    "${_windows_terminal_source_root}" "${_terminal_app_projection_root}" "${_terminal_app_reference_root}"
    "${_terminal_settings_model_projection_root}" "${_terminal_control_projection_root}" "${_terminal_core_projection_root}"
    "${_terminal_connection_projection_root}" "${_terminal_settings_editor_projection_root}" "${_ui_markdown_projection_root}"
    "${_ui_helpers_projection_root}" "${_native_cppwinrt_projection_root}" "${_wil_package_dir}/include"
    "${_themehelpers_package_dir}/inc" "${_workspace_root}/src/core/chat" "${_workspace_root}/src/glue/chat"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/types/inc" "${_openconsole_root}/src/inc" "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K" "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs})
target_compile_definitions(${_windows_terminal_target} PRIVATE _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG
    _WINDOWS EXTERNAL_BUILD WT_WORKSPACE_HOST_BUILD _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING __WRL_NO_DEFAULT_LIB__)
target_compile_options(${_windows_terminal_target} PRIVATE ${_native_product_compile_options} /W3 /Zc:preprocessor /bigobj
    /wd4702 /wd5104 /wd28204 /wd4201 /wd4312 /wd5105 /wd26434 /wd26445 /wd26456 /wd26478 /wd26494
    "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
target_link_directories(${_windows_terminal_target} PRIVATE "${_workspace_extension_vcpkg_lib_root}"
    "${_workspace_extension_vcpkg_manual_lib_root}" ${_portable_msvc_lib_dirs})
target_link_libraries(${_windows_terminal_target} PRIVATE ${_terminal_app_target} ${_terminal_app_lib_target} ${_terminal_settings_adapter_target}
    ${_terminal_settings_model_target} ${_terminal_control_target} ${_terminal_settings_editor_target} ${_ui_markdown_target}
    ConTypes WinRTUtils ConProps ConInt TerminalCore TerminalConnection MidiAudio
    "${_themehelpers_package_dir}/runtimes/win10-x64/lib/uap10.0/TerminalThemeHelpers.lib"
    "${_workspace_extension_vcpkg_lib_root}/fmt.lib" "${_workspace_extension_vcpkg_lib_root}/jsoncpp.lib"
    "${_workspace_extension_vcpkg_lib_root}/CLI11.lib"
    user32.lib gdi32.lib dwmapi.lib shcore.lib uxtheme.lib shell32.lib shlwapi.lib pathcch.lib propsys.lib ntdll.lib
    onecoreuap.lib WindowsApp.lib)

# The MSBuild build imports GenerateSxsManifestsFromWinmds.targets before
# linking WindowsTerminal. These manifests are required for unpackaged and
# portable activation of the product's WinRT components. Recreate that
# behavior here instead of relying on an installed package identity.
set(_windows_terminal_sxs_manifest_root "${CMAKE_BINARY_DIR}/windows-terminal-sxs/$<CONFIG>")
set(_windows_terminal_sxs_manifests)
set(_windows_terminal_sxs_link_inputs)
function(add_windows_terminal_sxs_manifest manifest_name winmd_path implementation_target)
    set(_manifest_path "${_windows_terminal_sxs_manifest_root}/${manifest_name}.manifest")
    if(implementation_target)
        set(_windows_terminal_sxs_dll_argument "-dll:$<TARGET_FILE_NAME:${implementation_target}>")
        set(_windows_terminal_sxs_dependencies "${winmd_path}" ${implementation_target})
    else()
        set(_windows_terminal_sxs_dll_argument "-dll:${manifest_name}.dll")
        set(_windows_terminal_sxs_dependencies "${winmd_path}")
    endif()
    add_custom_command(
        OUTPUT "${_manifest_path}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_windows_terminal_sxs_manifest_root}"
        COMMAND "${_portable_mt_path}"
            "-winmd:${winmd_path}"
            "${_windows_terminal_sxs_dll_argument}"
            "-out:${_manifest_path}"
        DEPENDS ${_windows_terminal_sxs_dependencies} "${_portable_mt_path}"
        VERBATIM
        COMMENT "Generating ${manifest_name} WinRT side-by-side manifest")
    list(APPEND _windows_terminal_sxs_manifests "${_manifest_path}")
    list(APPEND _windows_terminal_sxs_link_inputs "/MANIFESTINPUT:${_manifest_path}")
    set(_windows_terminal_sxs_manifests "${_windows_terminal_sxs_manifests}" PARENT_SCOPE)
    set(_windows_terminal_sxs_link_inputs "${_windows_terminal_sxs_link_inputs}" PARENT_SCOPE)
endfunction()
add_windows_terminal_sxs_manifest("Microsoft.Terminal.Control" "${_terminal_control_dll_winmd}" ${_terminal_control_target})
add_windows_terminal_sxs_manifest("Microsoft.Terminal.TerminalConnection" "${_terminal_connection_winmd}" ${_terminal_connection_target})
add_windows_terminal_sxs_manifest("Microsoft.Terminal.Settings.Model" "${_terminal_settings_model_dll_winmd}" ${_terminal_settings_model_target})
add_windows_terminal_sxs_manifest("Microsoft.Terminal.UI" "${_ui_helpers_winmd}" ${_ui_helpers_target})
add_windows_terminal_sxs_manifest("TerminalApp" "${_terminal_app_winmd}" ${_terminal_app_target})
add_windows_terminal_sxs_manifest("Microsoft.Terminal.Settings.Editor" "${_terminal_settings_editor_winmd}" ${_terminal_settings_editor_target})
add_windows_terminal_sxs_manifest("Microsoft.Terminal.UI.Markdown" "${_ui_markdown_winmd}" ${_ui_markdown_target})
add_windows_terminal_sxs_manifest("Microsoft.UI.Xaml" "${_openconsole_root}/packages/Microsoft.UI.Xaml.2.8.4/lib/uap10.0/Microsoft.UI.Xaml.winmd" "")
add_custom_target(windows-terminal-sxs-manifests DEPENDS ${_windows_terminal_sxs_manifests})
add_dependencies(${_windows_terminal_target} windows-terminal-sxs-manifests)
target_link_options(${_windows_terminal_target} PRIVATE /SUBSYSTEM:WINDOWS
    "/MANIFEST:EMBED" "/MANIFESTINPUT:${_windows_terminal_source_root}/WindowsTerminal.manifest"
    ${_windows_terminal_sxs_link_inputs}
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT)
set_target_properties(${_windows_terminal_target} PROPERTIES OUTPUT_NAME WindowsTerminal
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" RUNTIME_OUTPUT_DIRECTORY "${_windows_terminal_output_root}"
    PDB_OUTPUT_DIRECTORY "${_windows_terminal_output_root}")

set(_control_unit_tests_target ControlUnitTests)
set(_control_unit_tests_root "${_openconsole_root}/src/cascadia/UnitTests_Control")
add_library(${_control_unit_tests_target} SHARED
    "${_control_unit_tests_root}/ControlCoreTests.cpp"
    "${_control_unit_tests_root}/ControlInteractivityTests.cpp"
    "${_control_unit_tests_root}/pch.cpp")
add_dependencies(${_control_unit_tests_target} ${_terminal_control_target})
target_compile_features(${_control_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_control_unit_tests_target} PRIVATE "${_control_unit_tests_root}/pch.h")
target_include_directories(${_control_unit_tests_target} PRIVATE
    "${_control_unit_tests_root}" "${_terminal_control_projection_root}" "${_terminal_core_projection_root}"
    "${_terminal_connection_projection_root}" "${_ui_helpers_projection_root}" "${_native_cppwinrt_projection_root}"
    "${_taef_package_root}/Include" "${_wil_package_dir}/include"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/inc" "${_openconsole_root}/src/inc/test" "${_openconsole_root}/src/types/inc"
    "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include" ${_portable_msvc_include_dirs})
target_compile_definitions(${_control_unit_tests_target} PRIVATE
    _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING __WRL_NO_DEFAULT_LIB__ _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
target_compile_options(${_control_unit_tests_target} PRIVATE ${_native_product_compile_options}
    /Zc:preprocessor /bigobj /wd4702 "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
target_link_directories(${_control_unit_tests_target} PRIVATE ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}" "${_workspace_extension_vcpkg_lib_root}")
target_link_libraries(${_control_unit_tests_target} PRIVATE
    ${_terminal_control_lib_target} ${_midi_audio_target} TerminalCore ConTypes WinRTUtils ConRenderAtlas TerminalInput ConTermAdapt
    ${_renderer_uia_target} ${_tsf_target} ConRenderBase ConBufferOut ConTermParser
    "${_workspace_extension_vcpkg_lib_root}/fmt.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib ntdll.lib onecoreuap.lib WindowsApp.lib
    crypt32.lib bcrypt.lib icu.lib winmm.lib imm32.lib d2d1.lib dwrite.lib dxgi.lib d3d11.lib d3dcompiler.lib windowscodecs.lib)
target_link_options(${_control_unit_tests_target} PRIVATE "/INCLUDE:${_portable_dllmain_linker_symbol}"
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT)
set_target_properties(${_control_unit_tests_target} PROPERTIES OUTPUT_NAME "Control.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}" LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}" PDB_OUTPUT_DIRECTORY "${_native_product_output_root}")

set(_settings_model_unit_tests_target SettingsModelUnitTests)
set(_settings_model_unit_tests_root "${_openconsole_root}/src/cascadia/UnitTests_SettingsModel")
set(_settings_model_unit_tests_source_names
    ProfileTests ColorSchemeTests KeyBindingsTests CommandTests DeserializationTests NewTabMenuTests
    SerializationTests TerminalSettingsTests ThemeTests MediaResourceTests WorkspaceTests pch)
set(_settings_model_unit_tests_sources)
foreach(_settings_model_unit_tests_source_name IN LISTS _settings_model_unit_tests_source_names)
    list(APPEND _settings_model_unit_tests_sources "${_settings_model_unit_tests_root}/${_settings_model_unit_tests_source_name}.cpp")
endforeach()
list(APPEND _settings_model_unit_tests_sources "${_openconsole_root}/src/cascadia/TerminalSettingsAppAdapterLib/TerminalSettings.cpp")
add_library(${_settings_model_unit_tests_target} SHARED
    ${_settings_model_unit_tests_sources}
    "${_settings_model_unit_tests_root}/KeyChordTestSupport.cpp")
add_dependencies(${_settings_model_unit_tests_target} ${_terminal_settings_model_target} ${_terminal_control_target})
target_compile_features(${_settings_model_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_settings_model_unit_tests_target} PRIVATE "${_settings_model_unit_tests_root}/pch.h")
target_include_directories(${_settings_model_unit_tests_target} PRIVATE
    "${_settings_model_unit_tests_root}" "${_openconsole_root}/src/cascadia" "${_terminal_settings_model_source_root}"
    "${_terminal_settings_model_projection_root}" "${_terminal_control_projection_root}" "${_terminal_core_projection_root}"
    "${_terminal_connection_projection_root}" "${_ui_helpers_projection_root}" "${_native_cppwinrt_projection_root}"
    "${_taef_package_root}/Include" "${_wil_package_dir}/include" "${_openconsole_root}/src/cascadia/WinRTUtils/inc"
    "${_openconsole_root}/packages/Microsoft.VisualStudio.Setup.Configuration.Native.2.3.2262/lib/native/include"
    "${_openconsole_root}/src/cascadia/inc" "${_openconsole_root}/src/inc" "${_openconsole_root}/src/inc/test"
    "${_openconsole_root}/src/types/inc" "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K" "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include" ${_portable_msvc_include_dirs})
target_compile_definitions(${_settings_model_unit_tests_target} PRIVATE
    _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV NDEBUG _WINDOWS EXTERNAL_BUILD
    INLINE_TEST_METHOD_MARKUP UNIT_TESTING __WRL_NO_DEFAULT_LIB__ _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
target_compile_options(${_settings_model_unit_tests_target} PRIVATE ${_native_product_compile_options}
    /Zc:preprocessor /bigobj /wd4702 "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
target_link_directories(${_settings_model_unit_tests_target} PRIVATE ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}" "${_workspace_extension_vcpkg_lib_root}")
target_link_libraries(${_settings_model_unit_tests_target} PRIVATE
    ${_terminal_settings_model_lib_target} ConTypes WinRTUtils ConInt ConProps
    "${_workspace_extension_vcpkg_lib_root}/fmt.lib" "${_workspace_extension_vcpkg_lib_root}/jsoncpp.lib"
    "${_openconsole_root}/packages/Microsoft.VisualStudio.Setup.Configuration.Native.2.3.2262/lib/native/v141/${PORTABLE_PLATFORM}/Microsoft.VisualStudio.Setup.Configuration.Native.lib"
    Wex.Logger.lib Wex.Common.lib TE.Common.lib
    onecoreuap.lib WindowsApp.lib ntdll.lib shlwapi.lib pathcch.lib propsys.lib)
target_link_options(${_settings_model_unit_tests_target} PRIVATE "/INCLUDE:${_portable_dllmain_linker_symbol}"
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT)
set_target_properties(${_settings_model_unit_tests_target} PROPERTIES OUTPUT_NAME "SettingsModel.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}" LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}" PDB_OUTPUT_DIRECTORY "${_native_product_output_root}")

set(_terminal_app_unit_tests_target TerminalAppUnitTests)
set(_terminal_app_unit_tests_root "${_openconsole_root}/src/cascadia/ut_app")
add_library(${_terminal_app_unit_tests_target} SHARED
    "${_terminal_app_unit_tests_root}/JsonUtilsTests.cpp" "${_terminal_app_unit_tests_root}/FzfTests.cpp"
    "${_terminal_app_unit_tests_root}/precomp.cpp")
add_dependencies(${_terminal_app_unit_tests_target} ${_terminal_app_target})
target_compile_features(${_terminal_app_unit_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_app_unit_tests_target} PRIVATE "${_terminal_app_unit_tests_root}/precomp.h")
target_include_directories(${_terminal_app_unit_tests_target} PRIVATE
    "${_terminal_app_unit_tests_root}" "${_terminal_app_source_root}/.." "${_terminal_app_source_root}"
    "${_terminal_app_projection_root}" "${_terminal_app_reference_root}" "${_terminal_settings_model_projection_root}"
    "${_terminal_control_projection_root}" "${_terminal_core_projection_root}" "${_terminal_connection_projection_root}"
    "${_ui_helpers_projection_root}" "${_native_cppwinrt_projection_root}" "${_taef_package_root}/Include"
    "${_wil_package_dir}/include" "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/inc" "${_openconsole_root}/src/inc/test" "${_openconsole_root}/src/types/inc"
    "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include" ${_portable_msvc_include_dirs})
target_compile_definitions(${_terminal_app_unit_tests_target} PRIVATE _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV
    NDEBUG _WINDOWS EXTERNAL_BUILD INLINE_TEST_METHOD_MARKUP UNIT_TESTING __WRL_NO_DEFAULT_LIB__)
target_compile_options(${_terminal_app_unit_tests_target} PRIVATE ${_native_product_compile_options} /Zc:preprocessor /bigobj /wd4702
    "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
target_link_directories(${_terminal_app_unit_tests_target} PRIVATE ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}" "${_workspace_extension_vcpkg_lib_root}")
target_link_libraries(${_terminal_app_unit_tests_target} PRIVATE ${_terminal_app_lib_target} ${_terminal_settings_model_lib_target}
    ${_terminal_settings_adapter_target} ConTypes WinRTUtils ConInt ConProps
    "${_workspace_extension_vcpkg_lib_root}/fmt.lib" "${_workspace_extension_vcpkg_lib_root}/jsoncpp.lib"
    "${_workspace_extension_vcpkg_lib_root}/CLI11.lib" Wex.Logger.lib Wex.Common.lib TE.Common.lib WindowsApp.lib onecoreuap.lib ntdll.lib)
target_link_options(${_terminal_app_unit_tests_target} PRIVATE "/INCLUDE:${_portable_dllmain_linker_symbol}" /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT)
set_target_properties(${_terminal_app_unit_tests_target} PROPERTIES OUTPUT_NAME "Terminal.App.Unit.Tests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}" ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}"
    PDB_OUTPUT_DIRECTORY "${_native_product_output_root}")

set(_terminal_app_local_tests_target TerminalAppLocalTests)
set(_terminal_app_local_tests_root "${_openconsole_root}/src/cascadia/LocalTests_TerminalApp")
add_library(${_terminal_app_local_tests_target} SHARED
    "${_terminal_app_local_tests_root}/CommandlineTest.cpp" "${_terminal_app_local_tests_root}/SettingsTests.cpp"
    "${_terminal_app_local_tests_root}/TabTests.cpp" "${_terminal_app_local_tests_root}/FilteredCommandTests.cpp"
    "${_terminal_app_local_tests_root}/TerminalInputHarnessTests.cpp" "${_terminal_app_local_tests_root}/pch.cpp")
add_dependencies(${_terminal_app_local_tests_target} ${_terminal_app_target})
target_compile_features(${_terminal_app_local_tests_target} PRIVATE cxx_std_20)
target_precompile_headers(${_terminal_app_local_tests_target} PRIVATE "${_terminal_app_local_tests_root}/pch.h")
target_include_directories(${_terminal_app_local_tests_target} PRIVATE
    "${_terminal_app_local_tests_root}" "${_terminal_app_source_root}/.." "${_terminal_app_source_root}"
    "${_terminal_app_projection_root}" "${_terminal_app_reference_root}" "${_terminal_settings_model_projection_root}"
    "${_terminal_control_projection_root}" "${_terminal_core_projection_root}" "${_terminal_connection_projection_root}"
    "${_terminal_settings_editor_projection_root}" "${_ui_helpers_projection_root}" "${_native_cppwinrt_projection_root}"
    "${_workspace_root}/src/core/chat" "${_taef_package_root}/Include" "${_wil_package_dir}/include"
    "${_themehelpers_package_dir}/inc" "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/inc" "${_openconsole_root}/src/inc/test" "${_openconsole_root}/src/types/inc"
    "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include" ${_portable_msvc_include_dirs})
target_compile_definitions(${_terminal_app_local_tests_target} PRIVATE _UNICODE UNICODE WINRT_LEAN_AND_MEAN WT_BRANDING_DEV
    NDEBUG _WINDOWS EXTERNAL_BUILD INLINE_TEST_METHOD_MARKUP UNIT_TESTING WT_WORKSPACE_HOST_BUILD __WRL_NO_DEFAULT_LIB__)
target_compile_options(${_terminal_app_local_tests_target} PRIVATE ${_native_product_compile_options} /Zc:preprocessor /bigobj /wd4702
    "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
target_link_directories(${_terminal_app_local_tests_target} PRIVATE ${_portable_msvc_lib_dirs}
    "${_taef_package_root}/Library/${PORTABLE_PLATFORM}" "${_workspace_extension_vcpkg_lib_root}")
target_link_libraries(${_terminal_app_local_tests_target} PRIVATE ${_terminal_app_lib_target} ${_terminal_settings_adapter_target}
    ${_terminal_settings_model_lib_target} ConTypes WinRTUtils ConInt ConProps TerminalCore TerminalConnection
    "${_themehelpers_package_dir}/runtimes/win10-x64/lib/uap10.0/TerminalThemeHelpers.lib"
    "${_workspace_extension_vcpkg_lib_root}/fmt.lib" "${_workspace_extension_vcpkg_lib_root}/jsoncpp.lib"
    "${_workspace_extension_vcpkg_lib_root}/CLI11.lib" Wex.Logger.lib Wex.Common.lib TE.Common.lib onecoreuap.lib WindowsApp.lib ntdll.lib)
target_link_options(${_terminal_app_local_tests_target} PRIVATE "/INCLUDE:${_portable_dllmain_linker_symbol}" /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT)
set_target_properties(${_terminal_app_local_tests_target} PROPERTIES OUTPUT_NAME "TerminalApp.LocalTests"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" RUNTIME_OUTPUT_DIRECTORY "${_native_product_output_root}/LocalTests_TerminalApp"
    LIBRARY_OUTPUT_DIRECTORY "${_native_product_output_root}/LocalTests_TerminalApp"
    ARCHIVE_OUTPUT_DIRECTORY "${_native_product_output_root}/LocalTests_TerminalApp" PDB_OUTPUT_DIRECTORY "${_native_product_output_root}/LocalTests_TerminalApp")

set(_test_host_app_target TestHostApp)
set(_test_host_app_root "${_terminal_app_local_tests_root}/TestHostApp")
set(_test_host_app_output_root "${_native_product_output_root}/Tests/Data")
set(_test_host_app_winrtcore "${_taef_package_root}/../lib/Microsoft.VisualStudio.TestPlatform.TestExecutor.WinRTCore.winmd")
set(_test_host_app_projection_root "${CMAKE_BINARY_DIR}/test-host-app/projection")
set(_test_host_app_projection_header "${_test_host_app_projection_root}/winrt/Microsoft.VisualStudio.TestPlatform.TestExecutor.WinRTCore.h")
set(_test_host_app_manifest "${CMAKE_BINARY_DIR}/test-host-app/$<CONFIG>/AppxManifest.xml")
file(READ "${_test_host_app_root}/Package.appxmanifest" _test_host_app_manifest_content)
string(REPLACE "$targetnametoken$.exe" "TestHostApp.exe" _test_host_app_manifest_content "${_test_host_app_manifest_content}")
file(GENERATE OUTPUT "${_test_host_app_manifest}" CONTENT "${_test_host_app_manifest_content}")
add_custom_command(OUTPUT "${_test_host_app_projection_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_test_host_app_projection_root}"
    COMMAND "${_cppwinrt_path}" -input "${_test_host_app_winrtcore}"
        -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}" -output "${_test_host_app_projection_root}"
    DEPENDS "${_test_host_app_winrtcore}" "${_cppwinrt_path}"
    VERBATIM COMMENT "Generating TestHostApp TAEF C++/WinRT projection")
add_executable(${_test_host_app_target} WIN32
    "${_test_host_app_root}/pch.cpp" "${_test_host_app_root}/UnitTestApp.cpp" "${_test_host_app_projection_header}")
add_dependencies(${_test_host_app_target} ${_terminal_app_local_tests_target})
target_compile_features(${_test_host_app_target} PRIVATE cxx_std_20)
target_include_directories(${_test_host_app_target} PRIVATE "${_test_host_app_root}"
    "${_test_host_app_projection_root}" "${_native_cppwinrt_projection_root}" "${_wil_package_dir}/include"
    "${_taef_package_root}/Include" ${_portable_msvc_include_dirs})
target_compile_definitions(${_test_host_app_target} PRIVATE _UNICODE UNICODE INLINE_TEST_METHOD_MARKUP NDEBUG _WINDOWS EXTERNAL_BUILD)
target_compile_options(${_test_host_app_target} PRIVATE ${_native_product_compile_options})
target_link_directories(${_test_host_app_target} PRIVATE ${_portable_msvc_lib_dirs})
target_link_libraries(${_test_host_app_target} PRIVATE onecoreuap.lib WindowsApp.lib)
target_link_options(${_test_host_app_target} PRIVATE /SUBSYSTEM:WINDOWS
    /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT)
add_custom_command(TARGET ${_test_host_app_target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_test_host_app_manifest}" "${_test_host_app_output_root}/AppxManifest.xml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_test_host_app_manifest}" "${_test_host_app_output_root}/TestHostAppXManifest.xml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${_terminal_app_local_tests_target}>" "${_test_host_app_output_root}/TerminalApp.LocalTests.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_taef_package_root}/Binaries/${PORTABLE_PLATFORM}/TE.AppxUnitTestClient.dll" "${_test_host_app_output_root}/TE.AppxUnitTestClient.dll")
set_target_properties(${_test_host_app_target} PROPERTIES OUTPUT_NAME TestHostApp
    RUNTIME_OUTPUT_DIRECTORY "${_test_host_app_output_root}" PDB_OUTPUT_DIRECTORY "${_test_host_app_output_root}")

set(_sample_app_lib_target ScratchSampleAppLib)
set(_sample_app_target ScratchSampleApp)
set(_sample_app_root "${_openconsole_root}/scratch/ScratchIslandApp/SampleApp")
set(_sample_app_generation_root "${CMAKE_BINARY_DIR}/scratch-sample-app/$<CONFIG>")
set(_sample_app_unmerged_root "${_sample_app_generation_root}/unmerged")
set(_sample_app_projection_root "${_sample_app_generation_root}/projection")
set(_sample_app_component_root "${_sample_app_generation_root}/component")
set(_sample_app_reference_root "${_sample_app_generation_root}/references")
set(_sample_app_lib_output_root "${_native_product_output_root}/ScratchSampleAppLib")
set(_sample_app_output_root "${_native_product_output_root}/ScratchSampleApp")
set(_sample_app_winmd "${_sample_app_lib_output_root}/SampleApp.winmd")
set(_sample_app_provider_idl "${_sample_app_generation_root}/XamlMetaDataProvider.idl")
file(GENERATE OUTPUT "${_sample_app_provider_idl}" CONTENT
"namespace SampleApp\n{\n    runtimeclass XamlMetaDataProvider : [default] Windows.UI.Xaml.Markup.IXamlMetadataProvider\n    {\n        XamlMetaDataProvider();\n    }\n}\n")
set(_sample_app_idls "${_sample_app_root}/App.idl" "${_sample_app_root}/SampleAppLogic.idl"
    "${_sample_app_root}/MySettings.idl" "${_sample_app_root}/MyPage.idl" "${_sample_app_provider_idl}")
set(_sample_app_reference_winmds "${_terminal_core_winmd}" "${_terminal_connection_winmd}" "${_terminal_control_winmd}"
    "${_ui_helpers_mux_winmd}" "${_ui_helpers_webview2_winmd}"
    "${_portable_sdk_root}/References/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}/Windows.UI.Xaml.Hosting.HostingContract/5.0.0.0/Windows.UI.Xaml.Hosting.HostingContract.winmd"
    ${_terminal_control_xaml_sdk_contract_winmds})
set(_sample_app_midl_reference_args)
set(_sample_app_mdmerge_args)
foreach(_sample_app_reference IN LISTS _sample_app_reference_winmds)
    file(TO_NATIVE_PATH "${_sample_app_reference}" _sample_app_reference_native)
    list(APPEND _sample_app_midl_reference_args /reference "${_sample_app_reference_native}")
    get_filename_component(_sample_app_reference_dir "${_sample_app_reference}" DIRECTORY)
    list(APPEND _sample_app_mdmerge_args -metadata_dir "${_sample_app_reference_dir}")
endforeach()
file(TO_NATIVE_PATH "${_sample_app_root}" _sample_app_root_native)
file(TO_NATIVE_PATH "${_terminal_app_midl_metadata_root}" _sample_app_midl_metadata_native)
set(_sample_app_unmerged_winmds)
foreach(_sample_app_idl IN LISTS _sample_app_idls)
    get_filename_component(_sample_app_idl_name "${_sample_app_idl}" NAME_WE)
    set(_sample_app_unmerged_winmd "${_sample_app_unmerged_root}/${_sample_app_idl_name}.winmd")
    file(TO_NATIVE_PATH "${_sample_app_idl}" _sample_app_idl_native)
    file(TO_NATIVE_PATH "${_sample_app_unmerged_winmd}" _sample_app_unmerged_winmd_native)
    add_custom_command(OUTPUT "${_sample_app_unmerged_winmd}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_sample_app_unmerged_root}"
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_portable_compiler_target_dir};$ENV{PATH}"
            "${_portable_midl_path}" /I "${_terminal_connection_sdk_winrt_native}" /I "${_terminal_connection_sdk_shared_native}"
            /I "${_terminal_connection_sdk_um_native}" /I "${_sample_app_root_native}" ${_sample_app_midl_reference_args}
            /metadata_dir "${_sample_app_midl_metadata_native}" /winrt /W1 /nologo /char signed /env ${_portable_midl_environment}
            /winmd "${_sample_app_unmerged_winmd_native}" /h NUL /dlldata NUL /iid NUL /proxy NUL /notlb /client none
            /server none /enum_class /ns_prefix /target NT60 /nomidl "${_sample_app_idl_native}"
        DEPENDS ${_sample_app_idls} ${_sample_app_reference_winmds} "${_portable_midl_path}" VERBATIM)
    list(APPEND _sample_app_unmerged_winmds "${_sample_app_unmerged_winmd}")
endforeach()
set(_sample_app_module_cpp "${_sample_app_projection_root}/module.g.cpp")
set(_sample_app_projection_header "${_sample_app_projection_root}/winrt/SampleApp.h")
add_custom_command(OUTPUT "${_sample_app_winmd}" "${_sample_app_module_cpp}" "${_sample_app_projection_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_sample_app_lib_output_root}" "${_sample_app_projection_root}"
    COMMAND "${_portable_mdmerge_path}" -i "${_sample_app_unmerged_root}" -o "${_sample_app_lib_output_root}" ${_sample_app_mdmerge_args} -partial
    COMMAND "${_cppwinrt_path}" -input "${_sample_app_winmd}" -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -reference "${_terminal_core_winmd}" -reference "${_terminal_connection_winmd}" -reference "${_terminal_control_winmd}"
        -reference "${_ui_helpers_mux_winmd}" -output "${_sample_app_projection_root}" -component "${_sample_app_component_root}"
        -overwrite -name SampleApp -pch pch.h -prefix -optimize
    DEPENDS ${_sample_app_unmerged_winmds} ${_sample_app_reference_winmds} "${_portable_mdmerge_path}" "${_cppwinrt_path}" VERBATIM)
set(_sample_app_reference_header "${_sample_app_reference_root}/winrt/Microsoft.Terminal.Control.h")
add_custom_command(OUTPUT "${_sample_app_reference_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_sample_app_reference_root}"
    COMMAND "${_cppwinrt_path}" -input ${_sample_app_reference_winmds} -reference "${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        -output "${_sample_app_reference_root}"
    DEPENDS ${_sample_app_reference_winmds} "${_cppwinrt_path}" VERBATIM)
set(_sample_app_xaml_common_args
    --framework-runtime-dir "${_xaml_framework_runtime_dir}" --task-assembly "${_xaml_compiler_task}" --language CppWinRT
    --output "${_sample_app_projection_root}" --output-type staticlibrary --root-namespace SampleApp
    --windows-sdk-path "${_portable_sdk_root}" --vc-install-dir "${_portable_vc_root}" --target-platform-min-version 10.0.18362.0
    --project-path "${_sample_app_root}/CMakeLists.txt" --project-name SampleAppLib --pri-index-name SampleApp
    --application-definition "${_sample_app_root}/App.xaml" --page "${_sample_app_root}/MyPage.xaml" --include "${_sample_app_root}")
set(_sample_app_xaml_reference_args)
foreach(_sample_app_reference IN LISTS _sample_app_reference_winmds)
    list(APPEND _sample_app_xaml_reference_args --winmd-reference "${_sample_app_reference}")
endforeach()
set(_sample_app_xaml_outputs "${_sample_app_projection_root}/App.xaml.g.hpp" "${_sample_app_projection_root}/MyPage.xaml.g.hpp"
    "${_sample_app_projection_root}/App.xbf" "${_sample_app_projection_root}/MyPage.xbf"
    "${_sample_app_projection_root}/XamlTypeInfo.g.cpp" "${_sample_app_projection_root}/XamlTypeInfo.Impl.g.cpp")
set(_sample_app_xaml_provider_cpp "${_sample_app_projection_root}/XamlMetaDataProvider.cpp")
file(GENERATE OUTPUT "${_sample_app_xaml_provider_cpp}" CONTENT
"// Generated by CMake for the Scratch SampleApp XAML metadata provider.\n#include \"pch.h\"\n#include \"XamlMetaDataProvider.h\"\n#include \"XamlMetaDataProvider.g.cpp\"\n")
add_custom_command(OUTPUT ${_sample_app_xaml_outputs}
    COMMAND "${_xaml_compiler_host}" ${_sample_app_xaml_common_args} ${_sample_app_xaml_reference_args}
        ${_terminal_app_xaml_sdk_reference_args} --reference "${_terminal_control_xaml_facade_winmd}" --compile-mode RealBuildPass1
    COMMAND "${_xaml_compiler_host}" ${_sample_app_xaml_common_args} ${_sample_app_xaml_reference_args}
        ${_terminal_app_xaml_sdk_reference_args} --reference "${_terminal_control_xaml_facade_winmd}" --compile-mode RealBuildPass2
        --platform-xml-dir "${_portable_sdk_root}/Platforms/UAP/${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
        --local-assembly "${_sample_app_winmd}" --cl-include "App.h#App.xaml#${_sample_app_root}/App.h"
        --cl-include "MyPage.h#MyPage.xaml#${_sample_app_root}/MyPage.h"
        --cl-include "MySettings.h#MySettings.idl#${_sample_app_root}/MySettings.h"
        --cl-include "SampleAppLogic.h#SampleAppLogic.idl#${_sample_app_root}/SampleAppLogic.h"
    DEPENDS "${_xaml_compiler_host}" "${_sample_app_winmd}" "${_sample_app_root}/App.xaml" "${_sample_app_root}/MyPage.xaml"
        ${_sample_app_reference_winmds} "${_terminal_control_xaml_facade_winmd}" VERBATIM)
add_custom_target(ScratchSampleAppXaml DEPENDS ${_sample_app_xaml_outputs})
set(_sample_app_pri "${_sample_app_lib_output_root}/SampleApp.pri")
set(_sample_app_pri_root "${_sample_app_generation_root}/pri-root")
set(_sample_app_priconfig "${_sample_app_generation_root}/priconfig.xml")
add_custom_command(OUTPUT "${_sample_app_pri}"
    BYPRODUCTS "${_sample_app_priconfig}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_sample_app_pri_root}/SampleApp" "${_sample_app_pri_root}/Resources/en-US" "${_sample_app_lib_output_root}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_sample_app_projection_root}/App.xbf" "${_sample_app_pri_root}/App.xbf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_sample_app_projection_root}/MyPage.xbf" "${_sample_app_pri_root}/SampleApp/MyPage.xbf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_sample_app_root}/Resources/en-US/Resources.resw" "${_sample_app_pri_root}/Resources/en-US/Resources.resw"
    COMMAND "${PORTABLE_MAKEPRI_PATH}" createconfig /cf "${_sample_app_priconfig}" /dq en-US /pv 10.0.0 /o
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new /pr "${_sample_app_pri_root}" /cf "${_sample_app_priconfig}" /of "${_sample_app_pri}" /in SampleApp /o
    DEPENDS ${_sample_app_xaml_outputs} "${_sample_app_root}/Resources/en-US/Resources.resw" VERBATIM)
set(_sample_app_sources "${_sample_app_root}/init.cpp" "${_sample_app_root}/pch.cpp" "${_sample_app_root}/App.cpp"
    "${_sample_app_root}/MySettings.cpp" "${_sample_app_root}/MyPage.cpp" "${_sample_app_root}/SampleAppLogic.cpp"
    "${_sample_app_module_cpp}" "${_sample_app_xaml_provider_cpp}" "${_sample_app_projection_root}/XamlTypeInfo.g.cpp" "${_sample_app_projection_root}/XamlTypeInfo.Impl.g.cpp")
set_source_files_properties("${_sample_app_module_cpp}" "${_sample_app_projection_root}/XamlTypeInfo.g.cpp"
    "${_sample_app_xaml_provider_cpp}" "${_sample_app_projection_root}/XamlTypeInfo.Impl.g.cpp" "${_sample_app_pri}" PROPERTIES GENERATED TRUE)
add_library(${_sample_app_lib_target} STATIC ${_sample_app_sources} "${_sample_app_reference_header}" "${_sample_app_pri}")
add_dependencies(${_sample_app_lib_target} ScratchSampleAppXaml)
target_compile_features(${_sample_app_lib_target} PRIVATE cxx_std_20)
target_precompile_headers(${_sample_app_lib_target} PRIVATE "${_sample_app_root}/pch.h")
target_include_directories(${_sample_app_lib_target} PRIVATE "${_sample_app_projection_root}" "${_sample_app_reference_root}"
    "${_sample_app_root}" "${_terminal_control_projection_root}" "${_terminal_core_projection_root}" "${_terminal_connection_projection_root}"
    "${_ui_helpers_projection_root}" "${_native_cppwinrt_projection_root}" "${_wil_package_dir}/include"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc" "${_openconsole_root}/src/cascadia/inc" "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/types/inc" "${_openconsole_root}/dep" "${_openconsole_root}/dep/Console" "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium" "${_openconsole_root}/oss/interval_tree" "${_openconsole_root}/oss/pcg/include"
    "${_workspace_extension_vcpkg_lib_root}/../include" ${_portable_msvc_include_dirs})
target_compile_definitions(${_sample_app_lib_target} PRIVATE _LIB _UNICODE UNICODE WINRT_LEAN_AND_MEAN NDEBUG _WINDOWS EXTERNAL_BUILD DISABLE_XAML_GENERATED_MAIN __WRL_NO_DEFAULT_LIB__)
target_compile_options(${_sample_app_lib_target} PRIVATE ${_native_product_archive_compile_options} /W3 /Zc:preprocessor /bigobj
    /wd4702 "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
set_target_properties(${_sample_app_lib_target} PROPERTIES OUTPUT_NAME SampleAppLib MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    ARCHIVE_OUTPUT_DIRECTORY "${_sample_app_lib_output_root}")
add_library(${_sample_app_target} SHARED "${_sample_app_root}/dll/pch.cpp" "${_sample_app_root}/dll/SampleApp.def")
target_link_directories(${_sample_app_target} PRIVATE ${_portable_msvc_lib_dirs} "${_workspace_extension_vcpkg_lib_root}")
target_link_libraries(${_sample_app_target} PRIVATE ${_sample_app_lib_target} ${_terminal_control_target} TerminalConnection TerminalCore
    ConTypes WinRTUtils "${_workspace_extension_vcpkg_lib_root}/fmt.lib" WindowsApp.lib onecoreuap.lib shell32.lib user32.lib ntdll.lib)
target_link_options(${_sample_app_target} PRIVATE /SUBSYSTEM:CONSOLE /MANIFEST:NO "/INCLUDE:${_portable_dllmain_linker_symbol}")
add_custom_command(TARGET ${_sample_app_target} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_sample_app_winmd}" "${_sample_app_output_root}/SampleApp.winmd"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_sample_app_pri}" "${_sample_app_output_root}/SampleApp.pri")
set_target_properties(${_sample_app_target} PROPERTIES OUTPUT_NAME SampleApp MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_sample_app_output_root}" LIBRARY_OUTPUT_DIRECTORY "${_sample_app_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_sample_app_output_root}" PDB_OUTPUT_DIRECTORY "${_sample_app_output_root}")

set(_scratch_window_exe_target ScratchWindowExe)
set(_scratch_window_exe_root "${_openconsole_root}/scratch/ScratchIslandApp/WindowExe")
set(_scratch_window_exe_output_root "${_native_product_output_root}/ScratchIslandApp")
add_executable(${_scratch_window_exe_target} WIN32
    "${_scratch_window_exe_root}/pch.cpp"
    "${_scratch_window_exe_root}/icon.cpp"
    "${_scratch_window_exe_root}/SampleMain.cpp"
    "${_scratch_window_exe_root}/SampleAppHost.cpp"
    "${_scratch_window_exe_root}/SampleIslandWindow.cpp"
    "${_scratch_window_exe_root}/WindowExe.rc"
    "${_scratch_window_exe_root}/WindowExe.manifest")
add_dependencies(${_scratch_window_exe_target} ${_sample_app_target})
target_compile_features(${_scratch_window_exe_target} PRIVATE cxx_std_20)
target_include_directories(${_scratch_window_exe_target} PRIVATE
    "${_scratch_window_exe_root}"
    "${_sample_app_projection_root}"
    "${_sample_app_reference_root}"
    "${_native_cppwinrt_projection_root}"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/scratch/ScratchIslandApp"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc"
    "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/src/types/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_workspace_extension_vcpkg_lib_root}/../include"
    ${_portable_msvc_include_dirs})
target_compile_definitions(${_scratch_window_exe_target} PRIVATE
    _UNICODE UNICODE WINRT_LEAN_AND_MEAN NDEBUG _WINDOWS EXTERNAL_BUILD WT_BRANDING_DEV)
target_compile_options(${_scratch_window_exe_target} PRIVATE ${_native_product_compile_options} /bigobj /wd4702
    "/FU${_workspace_extension_platform_winmd}" "/FI${_workspace_extension_til_feature_header}")
target_link_directories(${_scratch_window_exe_target} PRIVATE ${_portable_msvc_lib_dirs} "${_workspace_extension_vcpkg_lib_root}")
target_link_libraries(${_scratch_window_exe_target} PRIVATE
    ${_sample_app_lib_target} ${_terminal_control_target} TerminalConnection TerminalCore ConTypes WinRTUtils
    "${_workspace_extension_vcpkg_lib_root}/fmt.lib" WindowsApp.lib onecoreuap.lib shell32.lib user32.lib ntdll.lib shcore.lib)
target_link_options(${_scratch_window_exe_target} PRIVATE
    /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /guard:cf /DEBUG:FULL /OPT:REF /OPT:ICF /RELEASE /DYNAMICBASE /NXCOMPAT)
set_target_properties(${_scratch_window_exe_target} PROPERTIES OUTPUT_NAME WindowExe
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_scratch_window_exe_output_root}"
    PDB_OUTPUT_DIRECTORY "${_scratch_window_exe_output_root}")

add_library(${_shell_extension_target} SHARED EXCLUDE_FROM_ALL
    "${_shell_extension_source_root}/pch.cpp"
    "${_shell_extension_source_root}/OpenTerminalHere.cpp"
    "${_shell_extension_source_root}/dllmain.cpp"
    "${_native_cppwinrt_projection_base}"
)
target_compile_features(${_shell_extension_target} PRIVATE cxx_std_20)
target_precompile_headers(${_shell_extension_target} PRIVATE "${_shell_extension_source_root}/pch.h")
target_include_directories(${_shell_extension_target} PRIVATE
    "${_native_cppwinrt_projection_root}"
    "${_wil_package_dir}/include"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc"
    "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/console"
    "${_openconsole_root}/dep/win32k"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)
target_link_directories(${_shell_extension_target} PRIVATE
    "${_workspace_extension_vcpkg_lib_root}"
    "${_workspace_extension_vcpkg_manual_lib_root}"
    ${_portable_msvc_lib_dirs}
)
target_compile_definitions(${_shell_extension_target} PRIVATE
    _USRDLL
    _WINDLL
    _UNICODE
    UNICODE
    _WINRT_DLL
    WINRT_LEAN_AND_MEAN
    WT_BRANDING_DEV
    _WINDOWS
    EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_options(${_shell_extension_target} PRIVATE
    ${_native_product_compile_options}
    /bigobj
    /wd5104
    /wd28204
    "/FI${_workspace_extension_til_feature_header}"
)
target_link_libraries(${_shell_extension_target} PRIVATE
    onecoreuap_apiset.lib
    d3dcompiler.lib
    dwmapi.lib
    uxtheme.lib
    shlwapi.lib
    ntdll.lib
    user32.lib
    windowsapp.lib
    ${_types_target}
    "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/WinRTUtils/WinRTUtils.lib"
    ${_workspace_extension_vcpkg_libs}
)
target_link_options(${_shell_extension_target} PRIVATE
    "/DEF:${_shell_extension_source_root}/WindowsTerminalShellExt.def"
    /MANIFEST:NO
    /NODEFAULTLIB:LIBUCRT.LIB
    /DEFAULTLIB:ucrt.lib
    /DEBUG:FULL
    /SUBSYSTEM:WINDOWS
    /OPT:REF
    /OPT:ICF
    /RELEASE
    /DYNAMICBASE
    /NXCOMPAT
    /guard:cf
    /DEBUGTYPE:CV,FIXUP
)
set_target_properties(${_shell_extension_target} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
    RUNTIME_OUTPUT_DIRECTORY "${_shell_extension_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_shell_extension_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_shell_extension_output_root}"
    PDB_OUTPUT_DIRECTORY "${_shell_extension_output_root}"
)

add_custom_command(
    OUTPUT "${_ext_generated_version_header}"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_FILE=${_ext_version_file}
        -DOUTPUT_FILE=${_ext_generated_version_header}
        -P "${_ext_generate_version_script}"
    DEPENDS
        "${_ext_version_file}"
        "${_ext_generate_version_script}"
    VERBATIM
    COMMENT "Generating ext About version header from ${_ext_version_file}"
)

add_custom_target(ext-build-version
    DEPENDS "${_ext_generated_version_header}"
)

add_library(${_workspace_extension_target} SHARED
    "${_workspace_root}/src/glue/workspace/WorkspaceTerminalPageExtension.cpp"
    "${_workspace_root}/src/glue/workspace/dll/WorkspaceModelWrapper.cpp"
    "${_workspace_root}/src/core/chat/TerminalInputHarness.cpp"
    "${_workspace_root}/src/core/chat/TerminalEventStore.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatController.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatStateHelpers.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatTextHelpers.cpp"
    "${_workspace_root}/src/glue/chat/WorkspaceDiagnosticLog.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceChatStore.cpp"
    "${_workspace_root}/src/core/chat/WorkspaceStoragePaths.cpp"
)

set_source_files_properties(
    "${_workspace_root}/src/glue/workspace/dll/WorkspaceModelWrapper.cpp"
    PROPERTIES
        OBJECT_DEPENDS
            "${_workspace_root}/src/glue/workspace/WorkspaceApi.h;${_workspace_root}/src/glue/workspace/WorkspaceDataTypes.h;${_workspace_root}/src/glue/workspace/WorkspaceRuntimeTypes.h;${_workspace_root}/src/glue/workspace/WorkspaceEditorTypes.h;${_workspace_root}/src/glue/workspace/WorkspaceModel.cpp;${_workspace_root}/src/core/workspace/WorkspaceCore.cpp;${_workspace_root}/src/core/workspace/WorkspaceCoreManagerStorage.cpp;${_workspace_root}/src/core/workspace/WorkspaceCoreNavigation.cpp;${_workspace_root}/src/core/workspace/WorkspaceCoreEditor.cpp;${_workspace_root}/src/core/workspace/WorkspaceCoreEditorState.cpp;${_workspace_root}/src/core/workspace/WorkspaceCoreLaunch.cpp;${_workspace_root}/src/core/workspace/WorkspaceCorePersistence.cpp;${_workspace_root}/src/core/workspace/WorkspaceCoreRuntime.cpp;${_workspace_root}/src/core/workspace/WorkspaceCoreRuntimeOps.cpp;${_workspace_root}/src/core/workspace/WorkspaceCoreStateManager.cpp;${_workspace_root}/src/core/workspace/deprecated/WorkspaceCaptureDeprecated.cpp;${_workspace_root}/src/glue/workspace/WorkspacePaletteHelpers.cpp;${_workspace_root}/src/glue/workspace/WorkspaceFacadeConversionHelpers.cpp;${_workspace_root}/src/glue/workspace/WorkspaceFacadeEditorStateMethods.cpp;${_workspace_root}/src/glue/workspace/WorkspaceFacadeStartupMethods.cpp;${_workspace_root}/src/glue/workspace/WorkspaceFacadePersistenceMethods.cpp;${_workspace_root}/src/glue/workspace/WorkspaceFacadeMethods.cpp;${_workspace_root}/src/glue/workspace/WorkspaceUtilityHelpers.cpp;${_workspace_root}/src/glue/workspace/WorkspaceStartupColorLogic.cpp;${_workspace_root}/src/glue/workspace/WorkspaceTerminalPageLegacyGlue.cpp;${_workspace_root}/src/glue/workspace/TerminalPageWorkspacePrelude.cpp;${_workspace_root}/src/glue/workspace/TerminalPageWorkspaceGlue.cpp;${_workspace_root}/src/glue/workspace/WorkspaceTerminalGlue.cpp;${_workspace_root}/src/glue/workspace/WorkspaceUiStateGlue.cpp;${_workspace_root}/src/glue/workspace/WorkspaceSaveRuntimeGlue.cpp;${_workspace_root}/src/glue/workspace/WorkspaceEditorGlue.cpp;${_workspace_root}/src/glue/workspace/WorkspaceManagerIconPickerGlue.cpp;${_workspace_root}/src/glue/workspace/WorkspaceManagerEditorPanels.cpp;${_workspace_root}/src/glue/workspace/WorkspaceManagerContentGlue.cpp;${_workspace_root}/src/glue/workspace/WorkspaceSaverGlue.cpp;${_workspace_root}/src/glue/workspace/WorkspaceLaunchRuntimeGlue.cpp;${_workspace_root}/src/glue/workspace/WorkspaceCreateGlue.cpp;${_workspace_root}/src/core/chat/WorkspaceChatTextHelpers.cpp;${_workspace_root}/src/glue/chat/WorkspaceChatDispatchGlue.cpp;${_workspace_root}/src/glue/chat/WorkspaceChatTerminalGlue.cpp;${_workspace_root}/src/glue/chat/WorkspaceChatTopLevelHelpers.cpp"
)

target_compile_features(${_workspace_extension_target} PRIVATE cxx_std_20)
target_precompile_headers(${_workspace_extension_target} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_workspace_root}/src/glue/workspace/dll/pch.h>"
)

target_include_directories(${_workspace_extension_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_themehelpers_package_dir}/inc"
    "${_workspace_root}/src/glue/workspace/dll"
    "${_workspace_extension_generated_root}"
    "${_workspace_extension_obj_root}"
    "${_openconsole_root}/src/cascadia"
    "${_openconsole_root}/src/cascadia/TerminalApp"
    "${_openconsole_root}/src/cascadia/TerminalApp/Generated Files"
    "${_openconsole_root}/src/cascadia/WindowsTerminal/Generated Files"
    "${_openconsole_root}/src/cascadia/WindowsTerminal/Generated Files/winrt"
    "${_openconsole_root}/src/cascadia/TerminalControl"
    "${_openconsole_root}/src/cascadia/TerminalControl/Generated Files"
    "${_openconsole_root}/src/cascadia/TerminalControl/Generated Files/winrt"
    "${_openconsole_root}/src/cascadia/TerminalControl/dll/Generated Files"
    "${_openconsole_root}/src/cascadia/TerminalControl/dll/Generated Files/winrt"
    "${_openconsole_root}/src/cascadia/TerminalSettingsModel"
    "${_openconsole_root}/src/cascadia/TerminalSettingsModel/Generated Files"
    "${_openconsole_root}/src/cascadia/TerminalSettingsModel/Generated Files/winrt"
    "${_openconsole_root}/src/cascadia/TerminalSettingsModel/dll/Generated Files"
    "${_openconsole_root}/src/cascadia/TerminalSettingsModel/dll/Generated Files/winrt"
    "${_openconsole_root}/src/cascadia/TerminalSettingsAppAdapterLib"
    "${_openconsole_root}/src/cascadia/TerminalSettingsAppAdapterLib/Generated Files"
    "${_openconsole_root}/src/cascadia/TerminalSettingsAppAdapterLib/Generated Files/winrt"
    "${_openconsole_root}/src/cascadia/UIHelpers/Generated Files"
    "${_openconsole_root}/src/cascadia/UIHelpers/Generated Files/winrt"
    "${_openconsole_root}/src/cascadia/UIMarkdown/Generated Files"
    "${_openconsole_root}/src/cascadia/UIMarkdown/Generated Files/winrt"
    "${_openconsole_root}/src/inc"
    "${_openconsole_root}/dep"
    "${_openconsole_root}/dep/Console"
    "${_openconsole_root}/dep/Win32K"
    "${_openconsole_root}/dep/gsl/include"
    "${_openconsole_root}/src/cascadia/WinRTUtils/inc"
    "${_openconsole_root}/src/cascadia/inc"
    "${_openconsole_root}/oss/chromium"
    "${_openconsole_root}/oss/interval_tree"
    "${_openconsole_root}/oss/pcg/include"
    "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/vcpkg/${PORTABLE_PLATFORM}-windows-static/include"
    ${_portable_msvc_include_dirs}
)

target_compile_definitions(${_workspace_extension_target} PRIVATE
    _USRDLL
    _WINDLL
    UNICODE
    WT_WORKSPACE_EXTENSION_EXPORTS
    _WINRT_DLL
    WINRT_LEAN_AND_MEAN
    WT_BRANDING_DEV
    NDEBUG
    _WINDOWS
    EXTERNAL_BUILD
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    __WRL_NO_DEFAULT_LIB__
)

target_compile_options(${_workspace_extension_target} PRIVATE
    /nologo
    /W3
    /WX
    /diagnostics:column
    /sdl
    /MP
    "$<$<NOT:$<CONFIG:Debug>>:/O2>"
    /Oi
    /Oy-
    /Zc:preprocessor
    /Gm-
    /EHsc
    /GS
    /guard:cf
    /Gy
    /fp:precise
    /Zc:wchar_t
    /Zc:forScope
    /Zc:inline
    /GR-
    /permissive-
    /external:W3
    /wd5104
    /wd28204
    /wd4201
    /wd4312
    /wd5105
    /wd26434
    /wd26445
    /wd26456
    /wd26478
    /wd26494
    /analyze-
    /FC
    /errorReport:queue
    /we4189
    /we4100
    /we4242
    /we4389
    /we4244
    /source-charset:utf-8
    /execution-charset:utf-8
    /Zc:__cplusplus
    /Zc:__STDC__
    /Zc:enumTypes
    /Zc:templateScope
    /Zc:throwingNew
    /diagnostics:caret
    /bigobj
    "/FI${_workspace_extension_til_feature_header}"
    "/FU${_workspace_extension_platform_winmd}"
)

target_link_directories(${_workspace_extension_target} PRIVATE
    "${_workspace_extension_vcpkg_lib_root}"
    "${_workspace_extension_vcpkg_manual_lib_root}"
    ${_portable_msvc_lib_dirs}
)

target_link_libraries(${_workspace_extension_target} PRIVATE
    "${_workspace_extension_themehelpers_lib}"
    ${_types_target}
    "${_workspace_extension_release_root}/ConProps.lib"
    "${_workspace_extension_release_root}/WinRTUtils/WinRTUtils.lib"
    "${_workspace_extension_release_root}/TerminalSettingsAppAdapterLib/TerminalSettingsAppAdapterLib.lib"
    "${_workspace_extension_release_root}/TerminalConnection/TerminalConnection.lib"
    "${_workspace_extension_release_root}/Microsoft.Terminal.Control/Microsoft.Terminal.Control.lib"
    "${_workspace_extension_release_root}/Microsoft.Terminal.Settings.Model/Microsoft.Terminal.Settings.Model.lib"
    "${_workspace_extension_release_root}/Microsoft.Terminal.Settings.Editor/Microsoft.Terminal.Settings.Editor.lib"
    "${_workspace_extension_release_root}/Microsoft.Terminal.UI/Microsoft.Terminal.UI.lib"
    "${_workspace_extension_release_root}/Microsoft.Terminal.UI.Markdown/Microsoft.Terminal.UI.Markdown.lib"
    ${_workspace_extension_vcpkg_libs}
    onecoreuap_apiset
    d3dcompiler
    dwmapi
    uxtheme
    shlwapi
    ntdll
    WindowsApp
    User32
    shell32
    WinMM
)

target_link_options(${_workspace_extension_target} PRIVATE
    /ERRORREPORT:QUEUE
    /INCREMENTAL:NO
    /NOLOGO
    /NODEFAULTLIB:libucrt.lib
    /DEFAULTLIB:ucrt.lib
    /MANIFEST:NO
    /SUBSYSTEM:WINDOWS
    /OPT:REF
    /OPT:ICF
    /TLBID:1
    /WINMD:NO
    /RELEASE
    /DYNAMICBASE
    /NXCOMPAT
    /guard:cf
    "/NATVIS:${_openconsole_root}/tools/ConsoleTypes.natvis"
    "/NATVIS:${_wil_package_dir}/natvis/wil.natvis"
    "/NATVIS:${_wil_package_dir}/natvis/wil.natstepfilter"
)

set_target_properties(${_workspace_extension_target} PROPERTIES
    OUTPUT_NAME "WorkspaceExtension"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    EXCLUDE_FROM_ALL YES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${_workspace_extension_output_root}"
    LIBRARY_OUTPUT_DIRECTORY "${_workspace_extension_output_root}"
    ARCHIVE_OUTPUT_DIRECTORY "${_workspace_extension_output_root}"
)

set(_portable_tool_source "${_workspace_root}/src/glue/portable/PortablePackageTool/Program.cs")
set(_portable_tool_global_usings "${_workspace_root}/src/glue/portable/PortablePackageTool/GlobalUsings.cs")
set(_portable_manifest_tool_source "${_workspace_root}/src/glue/portable/PortablePackageTool/ProductManifestGenerator.cs")
set(_portable_pri_tool_source "${_workspace_root}/src/glue/portable/PortablePackageTool/ProductPriGenerator.cs")
set(_portable_package_map_tool_source "${_workspace_root}/src/glue/portable/PortablePackageTool/ProductPackageMapGenerator.cs")
set(_portable_tool_runtimeconfig_source "${_workspace_root}/src/glue/portable/PortablePackageTool/PortablePackageTool.runtimeconfig.json")
set(_portable_tool_output_dir "${CMAKE_BINARY_DIR}/portable-package-tool")
set(_portable_tool_dll "${_portable_tool_output_dir}/PortablePackageTool.dll")
set(_portable_tool_pdb "${_portable_tool_output_dir}/PortablePackageTool.pdb")
set(_portable_tool_runtimeconfig "${_portable_tool_output_dir}/PortablePackageTool.runtimeconfig.json")
set(_portable_tool_rsp "${CMAKE_BINARY_DIR}/portable-package-tool.rsp")
set(_portable_cmake_dir "${_workspace_root}/src/glue/portable/cmake")
set(_portable_timed_command_script "${_portable_cmake_dir}/RunTimedCommand.cmake")
set(_portable_timing_dir "${CMAKE_BINARY_DIR}/portable-timing")
set(_product_package_build_root "${CMAKE_BINARY_DIR}/product-package/$<CONFIG>")
set(_product_manifest "${_product_package_build_root}/AppxManifest.xml")
set(_product_pri_config "${_product_package_build_root}/priconfig.xml")
set(_product_resources_pri "${_product_package_build_root}/resources.pri")
set(_product_package_map "${_product_package_build_root}/package.map.txt")
set(_product_bin_root "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>")
set(_product_package_project_root "${_openconsole_root}/src/cascadia/CascadiaPackage")
set(_product_source_manifest "${_product_package_project_root}/Package-Dev.appxmanifest")
set(_product_xaml_pri "${_openconsole_root}/packages/Microsoft.UI.Xaml.2.8.4/lib/uap10.0/Microsoft.UI.Xaml.pri")
set(_product_xaml_appx "${_openconsole_root}/packages/Microsoft.UI.Xaml.2.8.4/tools/AppX/${PORTABLE_PLATFORM}/Release/Microsoft.UI.Xaml.2.8.appx")
set(_product_xaml_runtime_root "${_windows_terminal_output_root}/_xaml")
set(_product_xaml_runtime_dll "${_product_xaml_runtime_root}/Microsoft.UI.Xaml.dll")
set(_product_xaml_runtime_themes "${_product_xaml_runtime_root}/Microsoft.UI.Xaml/Themes/Generic.xaml")
set(_product_xaml_runtime_output_dll "${_windows_terminal_output_root}/Microsoft.UI.Xaml.dll")
set(_product_xaml_runtime_output_themes "${_windows_terminal_output_root}/Microsoft.UI.Xaml/Themes/Generic.xaml")
if(NOT EXISTS "${_product_xaml_appx}")
    message(FATAL_ERROR "Could not find Microsoft.UI.Xaml AppX runtime at ${_product_xaml_appx}.")
endif()
add_custom_command(
    OUTPUT "${_product_xaml_runtime_dll}"
    BYPRODUCTS "${_product_xaml_runtime_themes}" "${_product_xaml_runtime_output_dll}" "${_product_xaml_runtime_output_themes}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_product_xaml_runtime_root}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_product_xaml_runtime_root}" "${_windows_terminal_output_root}"
    COMMAND "${_portable_makeappx_path}" unpack /p "${_product_xaml_appx}" /d "${_product_xaml_runtime_root}" /o
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_product_xaml_runtime_dll}" "${_product_xaml_runtime_output_dll}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_product_xaml_runtime_root}/Microsoft.UI.Xaml" "${_windows_terminal_output_root}/Microsoft.UI.Xaml"
    DEPENDS "${_product_xaml_appx}"
    VERBATIM
    COMMENT "Extracting Microsoft.UI.Xaml runtime without MSBuild"
)
add_custom_target(product-xaml-runtime DEPENDS "${_product_xaml_runtime_dll}")
add_dependencies(${_windows_terminal_target} product-xaml-runtime)

set(_portable_tool_rsp_content "/nostdlib+\n/target:exe\n/langversion:12.0\n/nullable:enable\n/optimize+\n/deterministic+\n/debug:portable\n/out:\"${_portable_tool_dll}\"\n")
foreach(_dotnet_reference IN LISTS _dotnet_net8_references)
    string(APPEND _portable_tool_rsp_content "/reference:\"${_dotnet_reference}\"\n")
endforeach()
string(APPEND _portable_tool_rsp_content "\"${_portable_tool_source}\"\n")
string(APPEND _portable_tool_rsp_content "\"${_portable_tool_global_usings}\"\n")
string(APPEND _portable_tool_rsp_content "\"${_portable_manifest_tool_source}\"\n")
string(APPEND _portable_tool_rsp_content "\"${_portable_pri_tool_source}\"\n")
string(APPEND _portable_tool_rsp_content "\"${_portable_package_map_tool_source}\"\n")
file(GENERATE OUTPUT "${_portable_tool_rsp}" CONTENT "${_portable_tool_rsp_content}")

add_custom_command(
    OUTPUT
        "${_portable_tool_dll}"
        "${_portable_tool_pdb}"
        "${_portable_tool_runtimeconfig}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_portable_tool_output_dir}"
    COMMAND "${CMAKE_COMMAND}"
        -DSTEP_NAME=package-tool
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${_dotnet_path}" "${_dotnet_csc_path}" /noconfig "@${_portable_tool_rsp}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_portable_tool_runtimeconfig_source}" "${_portable_tool_runtimeconfig}"
    DEPENDS
        "${_portable_tool_source}"
        "${_portable_tool_global_usings}"
        "${_portable_manifest_tool_source}"
        "${_portable_pri_tool_source}"
        "${_portable_package_map_tool_source}"
        "${_portable_tool_runtimeconfig_source}"
        "${_dotnet_csc_path}"
        ${_dotnet_net8_references}
    VERBATIM
)
add_custom_target(portable-package-tool DEPENDS
    "${_portable_tool_dll}"
    "${_portable_tool_pdb}"
    "${_portable_tool_runtimeconfig}"
)

# WPF control: the original SDK project only supplied XAML markup compilation
# and NuGet packing. Keep both operations explicit so this target is usable
# without dotnet/MSBuild.
set(_wpf_control_root "${_openconsole_root}/src/cascadia/WpfTerminalControl")
set(_wpf_control_generated_source "${CMAKE_BINARY_DIR}/wpf-terminal-control/TerminalControl.g.cs")
set(_wpf_control_output_root "${_openconsole_root}/bin/AnyCPU/$<CONFIG>/WpfTerminalControl")
set(_wpf_control_net472_dll "${_wpf_control_output_root}/net472/Microsoft.Terminal.Wpf.dll")
set(_wpf_control_net8_dll "${_wpf_control_output_root}/net8.0-windows/Microsoft.Terminal.Wpf.dll")
set(PORTABLE_WPF_PACKAGE_VERSION "0.1" CACHE STRING "Microsoft.Terminal.Wpf NuGet package version")
set(_wpf_control_nuspec "${CMAKE_BINARY_DIR}/wpf-terminal-control/$<CONFIG>/Microsoft.Terminal.Wpf.nuspec")
set(_wpf_control_nupkg_dir "${CMAKE_BINARY_DIR}/nupkg")
set(_wpf_control_nupkg "${_wpf_control_nupkg_dir}/Microsoft.Terminal.Wpf.${PORTABLE_WPF_PACKAGE_VERSION}.nupkg")
set(_wpf_control_symbols_nupkg "${_wpf_control_nupkg_dir}/Microsoft.Terminal.Wpf.${PORTABLE_WPF_PACKAGE_VERSION}.symbols.nupkg")
set(_wpf_control_nuget "${_openconsole_root}/dep/nuget/nuget.exe")
set(_wpf_control_x86_native "${_openconsole_root}/bin/x86/$<CONFIG>/Microsoft.Terminal.Control")
set(_wpf_control_x64_native "${_openconsole_root}/bin/x64/$<CONFIG>/Microsoft.Terminal.Control")
set(_wpf_control_arm64_native "${_openconsole_root}/bin/arm64/$<CONFIG>/Microsoft.Terminal.Control")
set(_wpf_control_sources
    "${_wpf_control_root}/ITerminalConnection.cs"
    "${_wpf_control_root}/NativeMethods.cs"
    "${_wpf_control_root}/TerminalContainer.cs"
    "${_wpf_control_root}/TerminalControl.xaml.cs"
    "${_wpf_control_root}/TerminalOutputEventArgs.cs"
    "${_wpf_control_root}/TerminalTheme.cs"
)
file(GENERATE OUTPUT "${_wpf_control_generated_source}" CONTENT "namespace Microsoft.Terminal.Wpf\n{\n    public partial class TerminalControl\n    {\n        internal System.Windows.Controls.Grid terminalGrid;\n        internal TerminalContainer termContainer;\n        internal System.Windows.Controls.Primitives.ScrollBar scrollbar;\n        internal TerminalControl terminalUserControl;\n\n        public void InitializeComponent()\n        {\n            terminalUserControl = this;\n            Name = \"terminalUserControl\";\n            Focusable = true;\n            terminalGrid = new System.Windows.Controls.Grid { Name = \"terminalGrid\" };\n            terminalGrid.ColumnDefinitions.Add(new System.Windows.Controls.ColumnDefinition());\n            terminalGrid.ColumnDefinitions.Add(new System.Windows.Controls.ColumnDefinition { Width = System.Windows.GridLength.Auto });\n            termContainer = new TerminalContainer { Name = \"termContainer\" };\n            scrollbar = new System.Windows.Controls.Primitives.ScrollBar { Name = \"scrollbar\", SmallChange = 1 };\n            System.Windows.Controls.Grid.SetColumn(scrollbar, 1);\n            scrollbar.Scroll += Scrollbar_Scroll;\n            terminalGrid.Children.Add(termContainer);\n            terminalGrid.Children.Add(scrollbar);\n            Content = terminalGrid;\n        }\n    }\n}\n")
set(_wpf_control_net472_rsp "${CMAKE_BINARY_DIR}/wpf-terminal-control/$<CONFIG>/net472.rsp")
set(_wpf_control_net8_rsp "${CMAKE_BINARY_DIR}/wpf-terminal-control/$<CONFIG>/net8.rsp")
set(_wpf_control_net472_rsp_content "/nostdlib+\n/target:library\n/langversion:latest\n/nullable:enable\n/debug:portable\n/out:\"${_wpf_control_net472_dll}\"\n")
foreach(_reference IN LISTS _net472_references)
    string(APPEND _wpf_control_net472_rsp_content "/reference:\"${_reference}\"\n")
endforeach()
set(_wpf_control_net8_rsp_content "/nostdlib+\n/target:library\n/langversion:latest\n/nullable:enable\n/debug:portable\n/out:\"${_wpf_control_net8_dll}\"\n")
foreach(_reference IN LISTS _dotnet_net8_references)
    string(APPEND _wpf_control_net8_rsp_content "/reference:\"${_reference}\"\n")
endforeach()
foreach(_reference IN LISTS _dotnet_windowsdesktop_references)
    string(APPEND _wpf_control_net8_rsp_content "/reference:\"${_reference}\"\n")
endforeach()
foreach(_source IN LISTS _wpf_control_sources)
    string(APPEND _wpf_control_net472_rsp_content "\"${_source}\"\n")
    string(APPEND _wpf_control_net8_rsp_content "\"${_source}\"\n")
endforeach()
string(APPEND _wpf_control_net472_rsp_content "\"${_wpf_control_generated_source}\"\n")
string(APPEND _wpf_control_net8_rsp_content "\"${_wpf_control_generated_source}\"\n")
file(GENERATE OUTPUT "${_wpf_control_net472_rsp}" CONTENT "${_wpf_control_net472_rsp_content}")
file(GENERATE OUTPUT "${_wpf_control_net8_rsp}" CONTENT "${_wpf_control_net8_rsp_content}")
file(GENERATE OUTPUT "${_wpf_control_nuspec}" CONTENT "<?xml version=\"1.0\"?>\n<package>\n  <metadata>\n    <id>Microsoft.Terminal.Wpf</id>\n    <version>${PORTABLE_WPF_PACKAGE_VERSION}</version>\n    <authors>Microsoft</authors>\n    <description>Windows Terminal WPF control.</description>\n  </metadata>\n  <files>\n    <file src=\"${_wpf_control_net472_dll}\" target=\"lib\\net472\" />\n    <file src=\"${_wpf_control_net8_dll}\" target=\"lib\\net8.0-windows\" />\n    <file src=\"${_wpf_control_x86_native}/Microsoft.Terminal.Control.dll\" target=\"runtimes\\win-x86\\native\" />\n    <file src=\"${_wpf_control_x64_native}/Microsoft.Terminal.Control.dll\" target=\"runtimes\\win-x64\\native\" />\n    <file src=\"${_wpf_control_arm64_native}/Microsoft.Terminal.Control.dll\" target=\"runtimes\\win-arm64\\native\" />\n    <file src=\"${_wpf_control_x86_native}/Microsoft.Terminal.Control.pdb\" target=\"runtimes\\win-x86\\native\" />\n    <file src=\"${_wpf_control_x64_native}/Microsoft.Terminal.Control.pdb\" target=\"runtimes\\win-x64\\native\" />\n    <file src=\"${_wpf_control_arm64_native}/Microsoft.Terminal.Control.pdb\" target=\"runtimes\\win-arm64\\native\" />\n  </files>\n</package>\n")
add_custom_command(OUTPUT "${_wpf_control_net472_dll}" "${_wpf_control_net8_dll}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_wpf_control_output_root}/net472" "${_wpf_control_output_root}/net8.0-windows"
    COMMAND "${_dotnet_path}" "${_dotnet_csc_path}" /noconfig "@${_wpf_control_net472_rsp}"
    COMMAND "${_dotnet_path}" "${_dotnet_csc_path}" /noconfig "@${_wpf_control_net8_rsp}"
    DEPENDS ${_wpf_control_sources} "${_wpf_control_generated_source}" "${_dotnet_csc_path}" ${_net472_references} ${_dotnet_net8_references} ${_dotnet_windowsdesktop_references}
    VERBATIM)
add_custom_target(WpfTerminalControl DEPENDS "${_wpf_control_net472_dll}" "${_wpf_control_net8_dll}")
add_custom_command(
    OUTPUT "${_wpf_control_nupkg}" "${_wpf_control_symbols_nupkg}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_wpf_control_nupkg_dir}"
    COMMAND "${_wpf_control_nuget}" pack "${_wpf_control_nuspec}" -OutputDirectory "${_wpf_control_nupkg_dir}" -Symbols -NoPackageAnalysis
    DEPENDS
        WpfTerminalControl
        "${_wpf_control_nuget}"
        "${_wpf_control_nuspec}"
        "${_wpf_control_x86_native}/Microsoft.Terminal.Control.dll"
        "${_wpf_control_x64_native}/Microsoft.Terminal.Control.dll"
        "${_wpf_control_arm64_native}/Microsoft.Terminal.Control.dll"
        "${_wpf_control_x86_native}/Microsoft.Terminal.Control.pdb"
        "${_wpf_control_x64_native}/Microsoft.Terminal.Control.pdb"
        "${_wpf_control_arm64_native}/Microsoft.Terminal.Control.pdb"
    VERBATIM)
add_custom_target(WpfTerminalControlPack DEPENDS "${_wpf_control_nupkg}" "${_wpf_control_symbols_nupkg}")

# ASan annotates STL types in every translation unit. Instrument exactly the
# static-library closure of OpenConsoleFuzzer; applying this globally also
# changes unrelated DLL targets, which do not link the sanitizer coverage
# runtime.
foreach(_host_fuzz_instrumented_target IN ITEMS
    ${_host_fuzz_wrapper_target}
    ${_types_target} ${_renderer_atlas_target} ${_terminal_input_target} ${_terminal_adapter_target}
    ${_interactivity_win32_target} ${_host_library_target} ${_midi_audio_target} ${_internal_target}
    ${_tsf_target} ${_renderer_gdi_target} ${_interactivity_base_target} ${_renderer_base_target}
    ${_buffer_output_target} ${_server_target} ${_terminal_parser_target} ${_props_library_target}
)
    target_compile_options(${_host_fuzz_instrumented_target} PRIVATE
        "$<$<CONFIG:Fuzzing>:/fsanitize=address>"
        "$<$<CONFIG:Fuzzing>:/fsanitize-coverage=inline-bool-flag>"
        "$<$<CONFIG:Fuzzing>:/fsanitize-coverage=edge>"
        "$<$<CONFIG:Fuzzing>:/fsanitize-coverage=trace-cmp>"
        "$<$<CONFIG:Fuzzing>:/fsanitize-coverage=trace-div>"
    )
    # The vendored static fmt archive is intentionally not ASan-instrumented.
    # Disable the MSVC STL container-annotation ABI markers in the instrumented
    # closure so it remains link-compatible while ordinary ASan checks remain.
    target_compile_definitions(${_host_fuzz_instrumented_target} PRIVATE
        "$<$<CONFIG:Fuzzing>:_DISABLE_STRING_ANNOTATION>"
        "$<$<CONFIG:Fuzzing>:_DISABLE_VECTOR_ANNOTATION>"
    )
endforeach()

if(NOT PORTABLE_PGO_BUILD_MODE STREQUAL "None")
    if(NOT PORTABLE_PLATFORM STREQUAL "x64" AND NOT PORTABLE_PLATFORM STREQUAL "arm64")
        message(FATAL_ERROR "PGO is supported only for x64 and arm64 portable builds.")
    endif()
    set(_portable_pgort_lib "${_portable_msvc_root}/lib/${PORTABLE_PLATFORM}/pgort.lib")
    set(_portable_pgo_tools_dir "${_portable_msvc_root}/bin/Hostx64/${PORTABLE_PLATFORM}")
    foreach(_portable_pgo_file IN ITEMS "${_portable_pgort_lib}" "${_portable_pgo_tools_dir}/pgort140.dll" "${_portable_pgo_tools_dir}/pgosweep.exe")
        if(NOT EXISTS "${_portable_pgo_file}")
            message(FATAL_ERROR "Required PGO runtime file is missing: ${_portable_pgo_file}")
        endif()
    endforeach()
    foreach(_portable_pgo_target IN ITEMS
        ${_terminal_connection_target}
        ${_terminal_control_target}
        ${_terminal_settings_model_target}
        ${_terminal_settings_editor_target}
        ${_terminal_app_target}
        ${_windows_terminal_target}
        ${_host_executable_target}
    )
        if(PORTABLE_PGO_BUILD_MODE STREQUAL "Instrument")
            target_link_options(${_portable_pgo_target} PRIVATE /GENPROFILE /LTCG)
        else()
            if(NOT PORTABLE_PGO_DATABASE_ROOT)
                message(FATAL_ERROR "PGO Optimize requires PORTABLE_PGO_DATABASE_ROOT to name the restored PGD NuGet package directory.")
            endif()
            get_target_property(_portable_pgo_output_name ${_portable_pgo_target} OUTPUT_NAME)
            if(NOT _portable_pgo_output_name)
                set(_portable_pgo_output_name "${_portable_pgo_target}")
            endif()
            set(_portable_pgd_file "${PORTABLE_PGO_DATABASE_ROOT}/tools/${PORTABLE_PLATFORM}/${_portable_pgo_output_name}.pgd")
            if(NOT EXISTS "${_portable_pgd_file}")
                message(FATAL_ERROR "PGO database for ${_portable_pgo_target} is missing: ${_portable_pgd_file}")
            endif()
            target_link_options(${_portable_pgo_target} PRIVATE "/USEPROFILE:PGD=${_portable_pgd_file}" /LTCG)
        endif()
        set_property(TARGET ${_portable_pgo_target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        target_link_libraries(${_portable_pgo_target} PRIVATE "${_portable_pgort_lib}")
        if(PORTABLE_PGO_BUILD_MODE STREQUAL "Instrument")
            add_custom_command(TARGET ${_portable_pgo_target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_portable_pgo_tools_dir}/pgort140.dll" "$<TARGET_FILE_DIR:${_portable_pgo_target}>/pgort140.dll"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_portable_pgo_tools_dir}/pgosweep.exe" "$<TARGET_FILE_DIR:${_portable_pgo_target}>/pgosweep.exe"
                VERBATIM)
        endif()
    endforeach()
endif()

set(PORTABLE_VCPKG_ROOT "${_vcpkg_root}")
set(PORTABLE_OPENCONSOLE_ROOT "${_openconsole_root}")
include("${_portable_cmake_dir}/EnsureVcpkg.cmake")

set_property(TARGET ${_workspace_extension_target} PROPERTY RULE_LAUNCH_COMPILE
    "\"${CMAKE_COMMAND}\" -DSTEP_NAME=workspace-extension-compile -P \"${_portable_timed_command_script}\" --"
)
set_property(TARGET ${_workspace_extension_target} PROPERTY RULE_LAUNCH_LINK
    "\"${CMAKE_COMMAND}\" -DSTEP_NAME=workspace-extension-link -P \"${_portable_timed_command_script}\" --"
)

add_custom_target(full
    COMMAND ${CMAKE_COMMAND}
        -DSTEP_NAME=native-product-foundation
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --config "$<CONFIG>" --target ${_midi_audio_target} ${_renderer_uia_target} ${_internal_target} ${_tsf_target} ${_renderer_gdi_target} ${_interactivity_base_target} ${_interactivity_win32_target} ${_renderer_base_target} ${_terminal_input_target} ${_terminal_adapter_target} ${_terminal_parser_target} ${_props_library_target} ${_buffer_output_target} ${_server_target} ${_host_library_target} ${_conpty_library_target} ${_renderer_atlas_target} ${_types_target} ${_winrt_utils_target} ${_terminal_core_target} ${_openconsole_proxy_target} ${_terminal_connection_target} ${_ui_helpers_target} ${_terminal_control_target} ${_terminal_settings_model_target} ${_terminal_settings_editor_target} ${_ui_markdown_target} ${_terminal_app_target} ${_windows_terminal_target} ${_host_executable_target}
    COMMAND ${CMAKE_COMMAND}
        -DSTEP_NAME=terminal-settings-adapter
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --config "$<CONFIG>" --target ${_terminal_settings_adapter_target}
    COMMAND ${CMAKE_COMMAND}
        -DSTEP_NAME=native-product-shims
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --config "$<CONFIG>" --target terminal-wt terminal-elevate-shim "${_shell_extension_target}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_product_package_build_root}" "${_portable_package_root}"
    COMMAND "${_dotnet_path}" "${_portable_tool_dll}" generate-manifest
        --source-manifest "${_product_source_manifest}"
        --output-manifest "${_product_manifest}"
        --architecture "${PORTABLE_PLATFORM}"
        --product-bin-root "${_product_bin_root}"
    COMMAND "${_dotnet_path}" "${_portable_tool_dll}" generate-pri-config
        --output-root "${_product_package_build_root}"
        --repo-root "${_workspace_root}"
        --package-root "${_product_package_project_root}"
        --product-bin-root "${_product_bin_root}"
        --xaml-pri "${_product_xaml_pri}"
    COMMAND "${PORTABLE_MAKEPRI_PATH}" new
        /pr "${_workspace_root}"
        /cf "${_product_pri_config}"
        /mn "${_product_manifest}"
        /of "${_product_resources_pri}"
        /o
    COMMAND "${_dotnet_path}" "${_portable_tool_dll}" generate-package-map
        --output-map "${_product_package_map}"
        --repo-root "${_workspace_root}"
        --package-root "${_product_package_project_root}"
        --product-bin-root "${_product_bin_root}"
        --platform "${PORTABLE_PLATFORM}"
        --manifest "${_product_manifest}"
        --resources-pri "${_product_resources_pri}"
    COMMAND "${_portable_makeappx_path}" pack
        /f "${_product_package_map}"
        /p "${_portable_terminal_package}"
        /o
    COMMAND ${CMAKE_COMMAND}
        -DSTEP_NAME=workspace-extension-full
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --config "$<CONFIG>" --target "${_workspace_extension_target}"
    COMMAND ${CMAKE_COMMAND}
        -DSTEP_NAME=full-repack
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${_dotnet_path}" "${_portable_tool_dll}"
        --package "${_portable_terminal_package_native}"
        --xaml-package "${_portable_xaml_package_native}"
        --platform "${PORTABLE_PLATFORM}"
        --configuration "$<CONFIG>"
        --destination "${_portable_output_root_native}"
        --source-root "${_workspace_root_native}"
        --makepri-path "${_makepri_path_native}"
        --roslyn-path "${_dotnet_csc_path_native}"
        --framework-reference-path "${_net472_reference_path_native}"
        --workspace-extension-output-dir "${_openconsole_bin_root_native_base}/$<CONFIG>/WorkspaceExtension"
        --single-file-output
        --portable-mode
    DEPENDS
        ext-build-version
        portable-package-tool
    USES_TERMINAL
    VERBATIM
    COMMENT "Packaging final bin artifacts into ${_portable_output_root} from a full rebuild"
)
add_dependencies(full product-xaml-runtime)

add_custom_target(ext ALL
    COMMAND ${CMAKE_COMMAND}
        -DSTEP_NAME=ext-repack
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${_dotnet_path}" "${_portable_tool_dll}"
        --package "${_portable_terminal_package_native}"
        --xaml-package "${_portable_xaml_package_native}"
        --platform "${PORTABLE_PLATFORM}"
        --configuration "$<CONFIG>"
        --destination "${_portable_output_root_native}"
        --source-root "${_workspace_root_native}"
        --makepri-path "${_makepri_path_native}"
        --roslyn-path "${_dotnet_csc_path_native}"
        --framework-reference-path "${_net472_reference_path_native}"
        --workspace-extension-output-dir "${_openconsole_bin_root_native_base}/$<CONFIG>/WorkspaceExtension"
        --single-file-output
        --portable-mode
    DEPENDS
        portable-package-tool
        ${_workspace_extension_target}
    USES_TERMINAL
    VERBATIM
    COMMENT "Packaging final bin artifacts into ${_portable_output_root} from ext-only changes"
)

# Targets migrated before PORTABLE_BRANDING existed carry the old Dev token in
# their local definitions. Keep only the single directory-level brand token.
get_property(_portable_build_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_portable_build_target IN LISTS _portable_build_targets)
    get_target_property(_portable_target_definitions "${_portable_build_target}" COMPILE_DEFINITIONS)
    if(_portable_target_definitions)
        list(REMOVE_ITEM _portable_target_definitions WT_BRANDING_DEV)
        set_property(TARGET "${_portable_build_target}" PROPERTY COMPILE_DEFINITIONS "${_portable_target_definitions}")
    endif()
    get_target_property(_portable_target_type "${_portable_build_target}" TYPE)
    if(_portable_target_type STREQUAL "EXECUTABLE" OR _portable_target_type STREQUAL "STATIC_LIBRARY" OR _portable_target_type STREQUAL "SHARED_LIBRARY" OR _portable_target_type STREQUAL "MODULE_LIBRARY")
        # The checked-in static dependency closure has only release CRT
        # variants. Use that ABI for Debug too; DBG controls source-level
        # debug paths without introducing _ITERATOR_DEBUG_LEVEL or CRT
        # mismatches at the final link.
        set_property(TARGET "${_portable_build_target}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
        # AuditMode reports external-header findings, but unlike ordinary builds
        # it must not discard the complete analysis run at the first warning.
        target_compile_options("${_portable_build_target}" PRIVATE "$<$<CONFIG:AuditMode>:/WX->")
    endif()
endforeach()
