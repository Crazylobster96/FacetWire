add_library(facetwire_status_tile_renderer MODULE src/plugin.c)
target_compile_features(facetwire_status_tile_renderer PRIVATE c_std_11)
target_include_directories(facetwire_status_tile_renderer PRIVATE
    ${PROJECT_SOURCE_DIR}/include)
set_target_properties(facetwire_status_tile_renderer PROPERTIES
    PREFIX "" OUTPUT_NAME "facetwire_status_tile_renderer")
if(MSVC)
    target_compile_options(facetwire_status_tile_renderer PRIVATE
        /W4 /WX /permissive- /utf-8)
else()
    target_compile_options(facetwire_status_tile_renderer PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
install(TARGETS facetwire_status_tile_renderer
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
