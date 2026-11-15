# XOFFSET_CXX_STANDARD when the first configure does not give it: 26 when the
# toolchain has C++26 reflection (CMake 3.30+, GCC 16+ or a P2996 Clang), else
# 20, the field-list build every client uses. Both write the same wire bytes.
# An explicit -DXOFFSET_CXX_STANDARD=26 never falls back: its checks fail
# with the reason (cmake/XOffsetBuildChecks.cmake).
function(xoffset_default_cxx_standard result_variable)
    set(reason "")
    set(flags "")
    set(link_flags "")
    set(id "${CMAKE_CXX_COMPILER_ID}")
    if(CMAKE_VERSION VERSION_LESS 3.30)
        set(reason "CMake ${CMAKE_VERSION}; the C++26 build needs CMake 3.30+")
    elseif(id STREQUAL "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
            set(reason "GCC ${CMAKE_CXX_COMPILER_VERSION}; C++26 reflection needs GCC 16+")
        else()
            set(flags -std=c++26 -freflection)
        endif()
    elseif(id STREQUAL "Clang" AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        # Same flags and directories as the full interface (XOffsetBuildChecks.cmake).
        get_filename_component(bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(install_dir "${bin_dir}" DIRECTORY)
        set(flags -std=c++26 -freflection -fexpansion-statements -stdlib=libc++)
        set(link_flags -stdlib=libc++)
        if(IS_DIRECTORY "${install_dir}/include/c++/v1")
            list(APPEND flags "-isystem" "${install_dir}/include/c++/v1")
        endif()
        if(IS_DIRECTORY "${install_dir}/lib")
            list(APPEND link_flags "-L${install_dir}/lib")
        endif()
    else()
        set(reason "${id} ${CMAKE_CXX_COMPILER_VERSION} has no C++26 reflection")
    endif()
    if(reason STREQUAL "")
        set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
        unset(XOFFSET_DEFAULT_STANDARD_PROBE CACHE)
        try_compile(XOFFSET_DEFAULT_STANDARD_PROBE
            "${PROJECT_BINARY_DIR}/compile-checks/default_standard"
            SOURCES "${PROJECT_SOURCE_DIR}/cmake/default_standard_probe.cpp"
            COMPILE_DEFINITIONS ${flags}
            LINK_OPTIONS ${link_flags}
            OUTPUT_VARIABLE probe_output)
        file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/compile-checks")
        file(WRITE "${PROJECT_BINARY_DIR}/compile-checks/default_standard.log" "${probe_output}")
        if(NOT XOFFSET_DEFAULT_STANDARD_PROBE)
            string(CONCAT reason "${id} ${CMAKE_CXX_COMPILER_VERSION} did not build the C++26 "
                "reflection probe, see ${PROJECT_BINARY_DIR}/compile-checks/default_standard.log")
        endif()
        unset(XOFFSET_DEFAULT_STANDARD_PROBE CACHE)
    endif()
    if(reason STREQUAL "")
        message(STATUS "XOFFSET_CXX_STANDARD not given: C++26 reflection found, using 26")
        set(${result_variable} 26 PARENT_SCOPE)
        return()
    endif()
    string(CONCAT note
        "XOFFSET_CXX_STANDARD not given: no C++26 reflection here (${reason}), "
        "so the full library builds as C++20 with one XOFFSET_RECORD line per record "
        "(the client build; same wire bytes as a C++26 build).\n"
        "For the C++26 reflection build, use a new build directory with CMake 3.30+ and "
        "GCC 16+ or the P2996 Clang (toolchains/README.md) and pass -DXOFFSET_CXX_STANDARD=26. "
        "Pass -DXOFFSET_CXX_STANDARD=20 to skip this check.")
    message("${note}")
    set(${result_variable} 20 PARENT_SCOPE)
endfunction()
