# Included through CMAKE_USER_MAKE_RULES_OVERRIDE by the MSVC-ABI toolchain files
# (the project's cmake_minimum_required predates CMAKE_MSVC_RUNTIME_LIBRARY and
# CMAKE_MSVC_DEBUG_INFORMATION_FORMAT):
# - the static CRT and STL (/MT, /MTd) instead of /MD, so the test programs carry
#   no msvcp140/vcruntime140 dependency and wine's own runtime DLLs never stand
#   in for Microsoft's;
# - debug information in the objects (/Z7) instead of a PDB written through
#   mspdbsrv (/Zi), which does not run under wine (C1902).
foreach(lang C CXX)
    foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set(flags "${CMAKE_${lang}_FLAGS_${config}_INIT}")
        string(REPLACE "/MDd" "/MTd" flags "${flags}")
        string(REPLACE "/MD" "/MT" flags "${flags}")
        string(REPLACE "/Zi" "/Z7" flags "${flags}")
        set(CMAKE_${lang}_FLAGS_${config}_INIT "${flags}")
    endforeach()
endforeach()
