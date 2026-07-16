cmake_minimum_required(VERSION 3.21)

project(TerminalPortableBuild LANGUAGES CXX)

if(CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
    set(CMAKE_CONFIGURATION_TYPES "Release;Debug;RelWithDebInfo" CACHE STRING "Supported build configurations" FORCE)
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

set(PORTABLE_PLATFORM "x64" CACHE STRING "Portable build platform")
set_property(CACHE PORTABLE_PLATFORM PROPERTY STRINGS x64 x86 arm64)

set(PORTABLE_DOTNET_SDK_VERSION "8.0.421" CACHE STRING "Pinned .NET SDK version")
set(PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION "10.0.22621.0" CACHE STRING "Windows SDK target version")
set(PORTABLE_PLATFORM_TOOLSET "" CACHE STRING "Optional platform toolset override")
set(PORTABLE_TARGET_FRAMEWORK_ROOT_PATH "" CACHE PATH "Optional TargetFrameworkRootPath override")
set(PORTABLE_MSBUILD_PATH "" CACHE FILEPATH "Optional MSBuild.exe path override")
set(PORTABLE_VCPKG_ROOT "" CACHE PATH "Optional vcpkg root override")
set(PORTABLE_MAKEPRI_PATH "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64/MakePri.exe" CACHE FILEPATH "Path to MakePri.exe")

set(_workspace_root "${CMAKE_SOURCE_DIR}")
set(_openconsole_root "${CMAKE_SOURCE_DIR}/microsoft")
set(_portable_output_root "${_workspace_root}/bin")
set(_portable_package_root "${_portable_output_root}/msix")

file(TO_NATIVE_PATH "${_workspace_root}" _workspace_root_native)
file(TO_NATIVE_PATH "${_openconsole_root}" _openconsole_root_native)
file(TO_NATIVE_PATH "${_portable_output_root}" _portable_output_root_native)
file(TO_NATIVE_PATH "${_portable_package_root}" _portable_package_root_native)
file(TO_NATIVE_PATH "${_openconsole_root}/bin/${PORTABLE_PLATFORM}" _openconsole_bin_root_native)
string(REGEX REPLACE "[/\\\\]+$" "" _openconsole_root_native_base "${_openconsole_root_native}")
string(REGEX REPLACE "[/\\\\]+$" "" _openconsole_bin_root_native_base "${_openconsole_bin_root_native}")
string(REGEX REPLACE "[/\\\\]+$" "" _portable_package_root_native_base "${_portable_package_root_native}")
set(_openconsole_root_msbuild "${_openconsole_root_native_base}/")
set(_portable_package_root_msbuild "${_portable_package_root_native_base}/")
file(TO_NATIVE_PATH "${_openconsole_root}/src/host/proxy/Host.Proxy.vcxproj" _host_proxy_project_native)
file(TO_NATIVE_PATH "${_openconsole_root}/src/cascadia/TerminalControl/dll/TerminalControl.vcxproj" _terminal_control_project_native)
file(TO_NATIVE_PATH "${_openconsole_root}/src/cascadia/TerminalSettingsModel/dll/Microsoft.Terminal.Settings.Model.vcxproj" _settings_model_project_native)
file(TO_NATIVE_PATH "${_openconsole_root}/src/cascadia/CascadiaPackage/CascadiaPackage.wapproj" _cascadia_package_project_native)

if(PORTABLE_MSBUILD_PATH)
    set(_msbuild_path "${PORTABLE_MSBUILD_PATH}")
else()
    find_program(_msbuild_path
        NAMES MSBuild.exe
        PATHS
            "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin"
            "C:/Program Files/Microsoft Visual Studio/17/Community/MSBuild/Current/Bin"
    )
endif()
if(NOT _msbuild_path)
    message(FATAL_ERROR "Could not find MSBuild.exe. Set PORTABLE_MSBUILD_PATH explicitly.")
endif()

set(_vs_install_root "")
set(_vswhere_path "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe")
if(EXISTS "${_vswhere_path}")
    execute_process(
        COMMAND "${_vswhere_path}" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        OUTPUT_VARIABLE _vs_install_root
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

if(NOT _vs_install_root)
    file(TO_CMAKE_PATH "${_msbuild_path}" _msbuild_path_cmake)
    string(REGEX REPLACE "/MSBuild/Current/Bin(/amd64)?/MSBuild\\.exe$" "" _vs_install_root "${_msbuild_path_cmake}")
endif()

if(EXISTS "${_vs_install_root}/MSBuild/Microsoft/VC/v180/Microsoft.Cpp.Default.props")
    set(_visual_studio_version "18.0")
    if(NOT PORTABLE_PLATFORM_TOOLSET)
        set(PORTABLE_PLATFORM_TOOLSET "v145" CACHE STRING "Optional platform toolset override" FORCE)
    endif()
elseif(EXISTS "${_vs_install_root}/MSBuild/Microsoft/VC/v170/Microsoft.Cpp.Default.props")
    set(_visual_studio_version "17.0")
    if(NOT PORTABLE_PLATFORM_TOOLSET)
        set(PORTABLE_PLATFORM_TOOLSET "v143" CACHE STRING "Optional platform toolset override" FORCE)
    endif()
else()
    message(FATAL_ERROR "Could not resolve VC targets from ${_msbuild_path}.")
endif()

set(_portable_msvc_include_dirs)
set(_portable_msvc_lib_dirs)
if(CMAKE_GENERATOR MATCHES "Ninja")
    if(NOT DEFINED ENV{INCLUDE} OR "$ENV{INCLUDE}" STREQUAL "" OR NOT DEFINED ENV{LIB} OR "$ENV{LIB}" STREQUAL "")
        message(FATAL_ERROR "Configure Ninja builds from a vcvars64-initialized environment so INCLUDE/LIB can be captured into CMake.")
    endif()

    cmake_path(CONVERT "$ENV{INCLUDE}" TO_CMAKE_PATH_LIST _portable_msvc_include_dirs NORMALIZE)
    cmake_path(CONVERT "$ENV{LIB}" TO_CMAKE_PATH_LIST _portable_msvc_lib_dirs NORMALIZE)
endif()

if(PORTABLE_VCPKG_ROOT)
    set(_vcpkg_root "${PORTABLE_VCPKG_ROOT}")
elseif(EXISTS "${_vs_install_root}/VC/vcpkg/vcpkg.exe")
    set(_vcpkg_root "${_vs_install_root}/VC/vcpkg")
else()
    set(_vcpkg_root "${_openconsole_root}/dep/vcpkg")
endif()

if(PORTABLE_TARGET_FRAMEWORK_ROOT_PATH)
    set(_target_framework_root_path "${PORTABLE_TARGET_FRAMEWORK_ROOT_PATH}")
else()
    set(_target_framework_root_path "${_openconsole_root}/packages-ref/Microsoft.NETFramework.ReferenceAssemblies.net472.1.0.3/build/")
endif()
file(TO_NATIVE_PATH "${_target_framework_root_path}" _target_framework_root_path_native)
string(REGEX REPLACE "[/\\\\]+$" "" _target_framework_root_path_native_base "${_target_framework_root_path_native}")
set(_target_framework_root_path_msbuild "${_target_framework_root_path_native_base}/")

find_program(_dotnet_path NAMES dotnet.exe REQUIRED)
if(NOT EXISTS "${PORTABLE_MAKEPRI_PATH}")
    message(FATAL_ERROR "Could not find MakePri.exe at ${PORTABLE_MAKEPRI_PATH}.")
endif()

set(_dotnet_root "C:/Program Files/dotnet")
file(TO_NATIVE_PATH "${_dotnet_root}" _dotnet_root_native)
set(_sdk_path "${_dotnet_root}/sdk/${PORTABLE_DOTNET_SDK_VERSION}/Sdks")
if(NOT EXISTS "${_sdk_path}")
    message(FATAL_ERROR "Could not find .NET SDK at ${_sdk_path}.")
endif()

set(_roslyn_csc_path "${_vs_install_root}/MSBuild/Current/Bin/Roslyn/csc.exe")
if(NOT EXISTS "${_roslyn_csc_path}")
    find_program(_roslyn_csc_path NAMES csc.exe)
endif()
if(NOT _roslyn_csc_path)
    message(FATAL_ERROR "Could not find csc.exe for the portable launcher.")
endif()
file(TO_NATIVE_PATH "${_vcpkg_root}" _vcpkg_root_native)
file(TO_NATIVE_PATH "${_roslyn_csc_path}" _roslyn_csc_path_native)
file(TO_NATIVE_PATH "${PORTABLE_MAKEPRI_PATH}" _makepri_path_native)

file(GLOB _wil_package_dirs LIST_DIRECTORIES true "${_openconsole_root}/packages/Microsoft.Windows.ImplementationLibrary.*")
list(SORT _wil_package_dirs COMPARE NATURAL ORDER DESCENDING)
list(GET _wil_package_dirs 0 _wil_package_dir)
if(NOT _wil_package_dir)
    message(FATAL_ERROR "Could not locate a Microsoft.Windows.ImplementationLibrary package under ${_openconsole_root}/packages.")
endif()

file(GLOB _themehelpers_package_dirs LIST_DIRECTORIES true "${_openconsole_root}/packages/Microsoft.Internal.Windows.Terminal.ThemeHelpers.*")
list(SORT _themehelpers_package_dirs COMPARE NATURAL ORDER DESCENDING)
list(GET _themehelpers_package_dirs 0 _themehelpers_package_dir)
if(NOT _themehelpers_package_dir)
    message(FATAL_ERROR "Could not locate a Microsoft.Internal.Windows.Terminal.ThemeHelpers package under ${_openconsole_root}/packages.")
endif()

file(GLOB _msvc_toolsets LIST_DIRECTORIES true "${_vs_install_root}/VC/Tools/MSVC/*")
list(SORT _msvc_toolsets COMPARE NATURAL ORDER DESCENDING)
list(GET _msvc_toolsets 0 _msvc_toolset_dir)
if(NOT _msvc_toolset_dir)
    message(FATAL_ERROR "Could not locate an MSVC toolset under ${_vs_install_root}/VC/Tools/MSVC.")
endif()

set(_workspace_extension_target WorkspaceExtension)
set(_workspace_extension_output_root "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>/WorkspaceExtension")
set(_workspace_extension_obj_root "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/$<CONFIG>/WorkspaceExtension")
set(_workspace_extension_generated_root "${_openconsole_root}/obj/${PORTABLE_PLATFORM}/$<CONFIG>/WorkspaceExtension/Generated Files")
set(_workspace_extension_platform_winmd "${_msvc_toolset_dir}/lib/x86/store/references/platform.winmd")
set(_workspace_extension_til_feature_header "${_openconsole_root}/bin/Release/inc/TilFeatureStaging.h")
set(_workspace_extension_themehelpers_lib "${_themehelpers_package_dir}/runtimes/win10-x64/lib/uap10.0/TerminalThemeHelpers.lib")
set(_workspace_extension_vcpkg_lib_root "${_openconsole_root}/obj/x64/vcpkg/x64-windows-static/lib")
set(_workspace_extension_vcpkg_manual_lib_root "${_workspace_extension_vcpkg_lib_root}/manual-link")
set(_workspace_extension_release_root "${_openconsole_root}/bin/${PORTABLE_PLATFORM}/$<CONFIG>")

file(GLOB _workspace_extension_vcpkg_libs "${_workspace_extension_vcpkg_lib_root}/*.lib")

add_library(${_workspace_extension_target} SHARED
    "${_workspace_root}/ext/src/workspace/WorkspaceTerminalPageExtension.cpp"
    "${_workspace_root}/ext/src/workspace/dll/WorkspaceModelWrapper.cpp"
    "${_workspace_root}/ext/src/chat/TerminalInputHarness.cpp"
    "${_workspace_root}/ext/src/chat/TerminalEventStore.cpp"
    "${_workspace_root}/ext/src/chat/WorkspaceChatController.cpp"
    "${_workspace_root}/ext/src/chat/WorkspaceChatStateHelpers.cpp"
    "${_workspace_root}/ext/src/chat/WorkspaceChatTextHelpers.cpp"
    "${_workspace_root}/ext/src/chat/WorkspaceDiagnosticLog.cpp"
    "${_workspace_root}/ext/src/chat/WorkspaceChatStore.cpp"
    "${_workspace_root}/ext/src/chat/WorkspaceStoragePaths.cpp"
)

target_compile_features(${_workspace_extension_target} PRIVATE cxx_std_20)
target_precompile_headers(${_workspace_extension_target} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${_workspace_root}/ext/src/workspace/dll/pch.h>"
)

target_include_directories(${_workspace_extension_target} PRIVATE
    "${_wil_package_dir}/include"
    "${_themehelpers_package_dir}/inc"
    "${_workspace_root}/ext/src/workspace/dll"
    "${_workspace_extension_generated_root}"
    "${_workspace_extension_obj_root}"
    "${_openconsole_root}/src/cascadia"
    "${_openconsole_root}/src/cascadia/TerminalApp"
    "${_openconsole_root}/src/cascadia/TerminalApp/Generated Files"
    "${_openconsole_root}/src/cascadia/TerminalControl"
    "${_openconsole_root}/src/cascadia/TerminalSettingsModel"
    "${_openconsole_root}/src/cascadia/TerminalSettingsAppAdapterLib"
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
    "${_openconsole_root}/obj/x64/vcpkg/x64-windows-static/include"
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
    /O2
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
    "${_workspace_extension_release_root}/ConTypes.lib"
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

set(_msbuild_env
    "${CMAKE_COMMAND}" -E chdir "${_workspace_root_native}"
    "${CMAKE_COMMAND}" -E env
    "DOTNET_ROOT=${_dotnet_root_native}"
    "MSBuildSDKsPath=${_sdk_path}"
    "MSBuildEnableWorkloadResolver=false"
    "DOTNET_MSBUILD_SDK_RESOLVER_SDKS_DIR=${_sdk_path}"
    "DOTNET_MSBUILD_SDK_RESOLVER_SDKS_VER=${PORTABLE_DOTNET_SDK_VERSION}"
    "DOTNET_MSBUILD_SDK_RESOLVER_CLI_DIR=${_dotnet_root_native}"
    "VCPKG_ROOT=${_vcpkg_root_native}"
    "VisualStudioVersion=${_visual_studio_version}"
    "VCPKG_PLATFORM_TOOLSET=${PORTABLE_PLATFORM_TOOLSET}"
)

set(_msbuild_common_args
    "/t:Build"
    "/m:1"
    "/p:Platform=${PORTABLE_PLATFORM}"
    "/p:PlatformToolset=${PORTABLE_PLATFORM_TOOLSET}"
    "/p:WindowsTargetPlatformVersion=${PORTABLE_WINDOWS_TARGET_PLATFORM_VERSION}"
    "/p:VcpkgManifestInstall=false"
    "/p:VisualStudioVersion=${_visual_studio_version}"
    "/p:OpenConsoleDir=${_openconsole_root_msbuild}"
    "/p:SolutionDir=${_openconsole_root_msbuild}"
    "/p:TargetFrameworkRootPath=${_target_framework_root_path_msbuild}"
)

set(_portable_tool_project "${_workspace_root}/ext/src/portable/PortablePackageTool/PortablePackageTool.csproj")
set(_portable_tool_output_dir "${CMAKE_BINARY_DIR}/portable-package-tool")
set(_portable_tool_dll "${_portable_tool_output_dir}/PortablePackageTool.dll")
set(_portable_cmake_dir "${_workspace_root}/ext/src/portable/cmake")
set(_portable_timed_command_script "${_portable_cmake_dir}/RunTimedCommand.cmake")
set(_portable_timing_dir "${CMAKE_BINARY_DIR}/portable-timing")

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
        -DSTEP_NAME=package-tool
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${_dotnet_path}" build "${_portable_tool_project}" --configuration Release --output "${_portable_tool_output_dir}" /p:UseAppHost=false /nologo
    COMMAND ${CMAKE_COMMAND}
        -DSTEP_NAME=msix-full
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        ${_msbuild_env} "${_msbuild_path}" "${_host_proxy_project_native}" ${_msbuild_common_args} "/p:Configuration=$<CONFIG>"
    COMMAND ${_msbuild_env} "${_msbuild_path}" "${_terminal_control_project_native}" ${_msbuild_common_args} "/p:Configuration=$<CONFIG>"
    COMMAND ${_msbuild_env} "${_msbuild_path}" "${_settings_model_project_native}" ${_msbuild_common_args} "/p:Configuration=$<CONFIG>"
    COMMAND ${_msbuild_env} "${_msbuild_path}" "${_cascadia_package_project_native}" ${_msbuild_common_args}
        "/p:Configuration=$<CONFIG>"
        "/p:GenerateAppxPackageOnBuild=true"
        "/p:AppxPackageDir=${_portable_package_root_msbuild}"
    COMMAND ${CMAKE_COMMAND}
        -DSTEP_NAME=full-repack
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${_dotnet_path}" "${_portable_tool_dll}"
        --package-root "${_portable_package_root_native}"
        --platform "${PORTABLE_PLATFORM}"
        --configuration "$<CONFIG>"
        --destination "${_portable_output_root_native}"
        --source-root "${_workspace_root_native}"
        --makepri-path "${_makepri_path_native}"
        --csc-path "${_roslyn_csc_path_native}"
        --single-file-output
        --portable-mode
    DEPENDS ${_workspace_extension_target}
    USES_TERMINAL
    VERBATIM
    COMMENT "Packaging final bin artifacts into ${_portable_output_root} from a full rebuild"
)

add_custom_target(ext ALL
    COMMAND ${CMAKE_COMMAND}
        -DSTEP_NAME=package-tool
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${_dotnet_path}" build "${_portable_tool_project}" --configuration Release --output "${_portable_tool_output_dir}" /p:UseAppHost=false /nologo
    COMMAND ${CMAKE_COMMAND}
        -DSTEP_NAME=ext-repack
        -DSTEP_WORKING_DIRECTORY=${_workspace_root}
        -P "${_portable_timed_command_script}"
        --
        "${_dotnet_path}" "${_portable_tool_dll}"
        --package-root "${_portable_package_root_native}"
        --platform "${PORTABLE_PLATFORM}"
        --configuration "$<CONFIG>"
        --destination "${_portable_output_root_native}"
        --source-root "${_workspace_root_native}"
        --makepri-path "${_makepri_path_native}"
        --csc-path "${_roslyn_csc_path_native}"
        --workspace-extension-output-dir "${_openconsole_bin_root_native_base}/$<CONFIG>/WorkspaceExtension"
        --single-file-output
        --portable-mode
    DEPENDS ${_workspace_extension_target}
    USES_TERMINAL
    VERBATIM
    COMMENT "Packaging final bin artifacts into ${_portable_output_root} from ext-only changes"
)
