# Same as the official x86-windows-static triplet
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# ...but with explicit platform toolset, so that future toolsets
# aren't automatically picked up (it defaults to the latest one).
if(DEFINED ENV{VCPKG_PLATFORM_TOOLSET} AND NOT "$ENV{VCPKG_PLATFORM_TOOLSET}" STREQUAL "")
    set(VCPKG_PLATFORM_TOOLSET "$ENV{VCPKG_PLATFORM_TOOLSET}")
elseif((DEFINED ENV{VisualStudioVersion} AND "$ENV{VisualStudioVersion}" VERSION_GREATER_EQUAL "18.0")
    OR (DEFINED ENV{VSCMD_VER} AND "$ENV{VSCMD_VER}" VERSION_GREATER_EQUAL "18.0")
    OR EXISTS "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Microsoft/VC/v180/Microsoft.Cpp.Default.props"
    OR EXISTS "C:/Program Files/Microsoft Visual Studio/18/Professional/MSBuild/Microsoft/VC/v180/Microsoft.Cpp.Default.props"
    OR EXISTS "C:/Program Files/Microsoft Visual Studio/18/Enterprise/MSBuild/Microsoft/VC/v180/Microsoft.Cpp.Default.props"
    OR EXISTS "C:/BuildTools2026/MSBuild/Microsoft/VC/v180/Microsoft.Cpp.Default.props")
    set(VCPKG_PLATFORM_TOOLSET v145)
else()
    set(VCPKG_PLATFORM_TOOLSET v143)
endif()

set(VCPKG_CXX_FLAGS /fsanitize=address)
set(VCPKG_C_FLAGS /fsanitize=address)
