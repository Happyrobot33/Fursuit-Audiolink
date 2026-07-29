# 1. Define paths first (Safe for the first pass)
SET(PROTO_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/../../protobuf")
set(PROTO_FILE "${PROTO_FOLDER}/audiolink_data.proto")
set(GENERATED_H "${CMAKE_CURRENT_BINARY_DIR}/audiolink_data.pb.h")
set(GENERATED_C "${CMAKE_CURRENT_BINARY_DIR}/audiolink_data.pb.c")

# 2. Register the component immediately
idf_component_register(
    SRCS "main.c"
         "sender.c"
         "receiver.c"
         "cobs.c"
         "${GENERATED_C}"
    INCLUDE_DIRS "."
                 "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES nvs_flash
             esp_wifi
             esp_netif
             esp_timer
             esp_driver_uart
             nanopb
)

# 3. Locate Python and build custom targets (Only safe AFTER registration)
find_package(Python3 REQUIRED)

add_custom_command(
    OUTPUT "${GENERATED_H}" "${GENERATED_C}"
    COMMAND ${Python3_EXECUTABLE} 
            "${PROTO_FOLDER}/generator/nanopb_generator.py"
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
