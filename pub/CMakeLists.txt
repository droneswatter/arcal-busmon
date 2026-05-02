# Traffic generator — publishes UCI messages to exercise the busmon UI.
# Built as a subdirectory of the arcal-busmon project so the arcal imported
# targets defined in the parent CMakeLists are available here.

add_executable(uci_traffic_gen uci_traffic_gen.cpp)
target_link_libraries(uci_traffic_gen PRIVATE arcal)
set_target_properties(uci_traffic_gen PROPERTIES
    BUILD_RPATH "${ARCAL_BUILD_DIR}"
)
