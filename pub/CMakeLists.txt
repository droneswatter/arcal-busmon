# ---------------------------------------------------------------------------
# Traffic generators — publish UCI messages to exercise the busmon UI.
# Built as a subdirectory of the arcal-busmon project so the arcal imported
# targets defined in the parent CMakeLists are available here.
# ---------------------------------------------------------------------------

add_executable(pub_rich pub_rich.cpp)
target_link_libraries(pub_rich PRIVATE arcal arcal_externalizer_cdr)

add_executable(pub_continuous pub_continuous.cpp)
target_link_libraries(pub_continuous PRIVATE arcal arcal_externalizer_cdr)

foreach(_tgt pub_rich pub_continuous)
    set_target_properties(${_tgt} PROPERTIES
        BUILD_RPATH
            "${ARCAL_BUILD_DIR};${ARCAL_BUILD_DIR}/externalizer/cdr"
    )
endforeach()
