# 1. Define paths first (Safe for the first pass)
SET(PROTO_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/../../Fursuit-Audiolink-Driver/Assets")
set(PROTO_FILE "${PROTO_FOLDER}/audiolink_data.proto")
set(GENERATED_H "${CMAKE_CURRENT_BINARY_DIR}/audiolink_data.pb.h")
set(GENERATED_C "${CMAKE_CURRENT_BINARY_DIR}/audiolink_data.pb.c")

# 2. Register the component immediately
# CONFIGURE_DEPENDS auto-reruns CMake when files are added/removed, but errors out during
# ESP-IDF's early script-mode (cmake -P) pass used to discover REQUIRES, so only use it
# during the real configure pass.
if(CMAKE_SCRIPT_MODE_FILE)
    set(GLOB_DEPENDS_ARG "")
else()
    set(GLOB_DEPENDS_ARG "CONFIGURE_DEPENDS")
endif()

file(GLOB_RECURSE APP_SOURCES ${GLOB_DEPENDS_ARG} "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

file(GLOB_RECURSE APP_HEADERS ${GLOB_DEPENDS_ARG} "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
set(APP_INCLUDE_DIRS ".")
foreach(header ${APP_HEADERS})
    get_filename_component(header_dir ${header} DIRECTORY)
    list(APPEND APP_INCLUDE_DIRS ${header_dir})
endforeach()
list(REMOVE_DUPLICATES APP_INCLUDE_DIRS)

idf_component_register(
    SRCS ${APP_SOURCES}
         "${GENERATED_C}"
    INCLUDE_DIRS ${APP_INCLUDE_DIRS}
                 "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES    spi_flash
                esp_driver_gpio
                driver
                esp_hal_i2s
                esp_wifi
                nvs_flash
                esp_event
                esp_netif
                esp32-hub75-matrixpanel-dma
                nanopb
                zlib
)

# 3. Locate Python and build custom targets (Only safe AFTER registration)
find_package(Python3 REQUIRED)

add_custom_command(
    OUTPUT "${GENERATED_H}" "${GENERATED_C}"
    COMMAND ${Python3_EXECUTABLE} 
            "${CMAKE_CURRENT_SOURCE_DIR}/../../protobuf/generator/nanopb_generator.py"
            --quiet
            --output-dir="${CMAKE_CURRENT_BINARY_DIR}"
            --proto-path="${PROTO_FOLDER}"
            "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
    COMMENT "[nanopb] Generating C bindings from audiolink_data.proto"
)

# 4. Bind the generation to the component library target
add_custom_target(generate_nanopb DEPENDS "${GENERATED_H}" "${GENERATED_C}")
add_dependencies(${COMPONENT_LIB} generate_nanopb)
