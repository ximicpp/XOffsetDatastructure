# Included through CMAKE_USER_MAKE_RULES_OVERRIDE by the MSVC-ABI toolchain files:
# the static CRT and STL (/MT, /MTd) instead of CMake's /MD default, so the test
# programs carry no msvcp140/vcruntime140 dependency and wine's own runtime DLLs
# never stand in for Microsoft's. (The project's cmake_minimum_required predates
# CMAKE_MSVC_RUNTIME_LIBRARY.)
foreach(lang C CXX)
    foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        string(REPLACE "/MDd" "/MTd" CMAKE_${lang}_FLAGS_${config}_INIT "${CMAKE_${lang}_FLAGS_${config}_INIT}")
        string(REPLACE "/MD" "/MT" CMAKE_${lang}_FLAGS_${config}_INIT "${CMAKE_${lang}_FLAGS_${config}_INIT}")
    endforeach()
endforeach()
