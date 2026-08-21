add_library(facetwire_hello_plugin STATIC plugin.c)
target_compile_features(facetwire_hello_plugin PRIVATE c_std_11)
target_include_directories(facetwire_hello_plugin PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_executable(facetwire_hello_host host.c)
target_compile_features(facetwire_hello_host PRIVATE c_std_11)
target_link_libraries(facetwire_hello_host PRIVATE
    FacetWire::facetwire
    facetwire_hello_plugin
)

if(BUILD_TESTING)
    add_test(NAME facetwire.example.hello COMMAND facetwire_hello_host)
endif()
