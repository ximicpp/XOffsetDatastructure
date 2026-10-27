include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
foreach(directory CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_DATADIR CMAKE_INSTALL_BINDIR)
    if(IS_ABSOLUTE "${${directory}}" OR "${${directory}}" MATCHES "(^|/)\\.\\.(/|$)")
        message(FATAL_ERROR "${directory} must be relative and stay inside the install prefix for a relocatable XOffset package")
    endif()
endforeach()
set(XOFFSET_PACKAGE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/XOffsetDatastructure")
set(XOFFSET_RELEASE_DIR "${CMAKE_INSTALL_DATADIR}/XOffsetDatastructure")
set(XOFFSET_LICENSE_DIR "${CMAKE_INSTALL_DATADIR}/XOffsetDatastructure/licenses")
set(XOFFSET_HAS_FULL OFF)

install(FILES "${PROJECT_SOURCE_DIR}/XOFFSET_VERSION" DESTINATION "${XOFFSET_RELEASE_DIR}" RENAME VERSION)
install(FILES "${PROJECT_SOURCE_DIR}/CHANGELOG.md"
    "${PROJECT_SOURCE_DIR}/docs/ALPHA_COMPATIBILITY.md" DESTINATION "${XOFFSET_RELEASE_DIR}")
target_include_directories(xoffset_storage INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
install(TARGETS xoffset_storage EXPORT XOffsetTargets)
install(FILES "${PROJECT_SOURCE_DIR}/xoffset_virtual_memory.hpp"
    "${PROJECT_SOURCE_DIR}/xoffset_slot_storage.hpp" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${XOFFSET_LICENSE_DIR}")
if(NOT XOFFSET_STORAGE_ONLY)
    set(XOFFSET_HAS_FULL ON)
    target_include_directories(xoffset_common INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_include_directories(xoffset_common SYSTEM INTERFACE $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}/xoffset-deps>)
    install(TARGETS xoffset_common EXPORT XOffsetTargets)
    foreach(header IN LISTS XOFFSET_FULL_OWN_HEADERS)
        install(FILES "${PROJECT_SOURCE_DIR}/${header}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    endforeach()
    foreach(runtime IN ITEMS xoffset_validation_scratch xoffset_snapshot_index_runtime)
        if(TARGET ${runtime})
            target_include_directories(${runtime} INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
            install(TARGETS ${runtime} EXPORT XOffsetTargets
                RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
                LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
        endif()
    endforeach()
    foreach(include_dir IN LISTS BOOST_INCLUDE_DIRS TYPELAYOUT_INCLUDE_DIRS)
        install(DIRECTORY "${include_dir}/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/xoffset-deps")
    endforeach()
    install(FILES "${PROJECT_SOURCE_DIR}/external/boost/LICENSE_1_0.txt" DESTINATION "${XOFFSET_LICENSE_DIR}/boost")
    if(TYPELAYOUT_INCLUDE_DIRS)
        install(FILES "${PROJECT_SOURCE_DIR}/external/typelayout/LICENSE" "${PROJECT_SOURCE_DIR}/external/typelayout/LICENSE_1_0.txt" DESTINATION "${XOFFSET_LICENSE_DIR}/typelayout")
    endif()
    install(FILES "${PROJECT_BINARY_DIR}/xoffset-third-party-manifest.txt" DESTINATION "${CMAKE_INSTALL_DATADIR}/XOffsetDatastructure")
endif()
configure_package_config_file("${PROJECT_SOURCE_DIR}/cmake/XOffsetDatastructureConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/XOffsetDatastructureConfig.cmake"
    INSTALL_DESTINATION "${XOFFSET_PACKAGE_DIR}")
configure_file("${PROJECT_SOURCE_DIR}/cmake/XOffsetDatastructureConfigVersion.cmake.in"
    "${PROJECT_BINARY_DIR}/XOffsetDatastructureConfigVersion.cmake" @ONLY NEWLINE_STYLE LF)
install(EXPORT XOffsetTargets NAMESPACE XOffset:: DESTINATION "${XOFFSET_PACKAGE_DIR}")
install(FILES "${PROJECT_BINARY_DIR}/XOffsetDatastructureConfig.cmake"
    "${PROJECT_BINARY_DIR}/XOffsetDatastructureConfigVersion.cmake" DESTINATION "${XOFFSET_PACKAGE_DIR}")
