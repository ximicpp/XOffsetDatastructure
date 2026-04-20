if(NOT DEFINED EXPORTER)
    message(FATAL_ERROR "EXPORTER is required")
endif()

if(NOT DEFINED SOURCE_SIG_DIR)
    message(FATAL_ERROR "SOURCE_SIG_DIR is required")
endif()

if(NOT DEFINED GENERATED_SIG_DIR)
    message(FATAL_ERROR "GENERATED_SIG_DIR is required")
endif()

file(REMOVE_RECURSE "${GENERATED_SIG_DIR}")
file(MAKE_DIRECTORY "${GENERATED_SIG_DIR}")

execute_process(
    COMMAND "${EXPORTER}" "${GENERATED_SIG_DIR}"
    RESULT_VARIABLE export_status)

if(NOT export_status EQUAL 0)
    message(FATAL_ERROR "signature exporter failed with status ${export_status}")
endif()

file(GLOB source_sig_files RELATIVE "${SOURCE_SIG_DIR}" "${SOURCE_SIG_DIR}/*.sig.hpp")
file(GLOB generated_sig_files RELATIVE "${GENERATED_SIG_DIR}" "${GENERATED_SIG_DIR}/*.sig.hpp")
list(SORT source_sig_files)
list(SORT generated_sig_files)

if(NOT source_sig_files STREQUAL generated_sig_files)
    message(FATAL_ERROR
        "regenerated signature inventory differs from committed tools/sigs")
endif()

foreach(sig_file IN LISTS source_sig_files)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${SOURCE_SIG_DIR}/${sig_file}"
            "${GENERATED_SIG_DIR}/${sig_file}"
        RESULT_VARIABLE compare_status)
    if(NOT compare_status EQUAL 0)
        message(FATAL_ERROR
            "regenerated signature baseline drift detected for ${sig_file}")
    endif()
endforeach()
