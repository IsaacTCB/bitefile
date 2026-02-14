project(examples)

set(example_dirs
    basic
    utf8
    multiple_files
)

set(working_dir ${CMAKE_CURRENT_BINARY_DIR})
set(pack_dir "${working_dir}/assets")
set(pack_output "${working_dir}/data.bite")

set(example_assets)
foreach(example_dir ${example_dirs})
    set(example_name ${example_dir})
    file(GLOB sources ${example_dir}/*.c)

    # Store all filepaths from the assets folder.
    file(COPY ${example_dir}/assets/ DESTINATION ${pack_dir})

    add_executable(${example_dir} ${sources})
    target_link_libraries(${example_name} PRIVATE bitefile)
endforeach()

# =========================
# Create example data.bite
# =========================

# Warning: this code's a mess, but it works so whatever
find_package(Python QUIET COMPONENTS Interpreter)
if(Python_FOUND)
    # Copy assets to project folder for easier access
    # file(
    #     COPY ${example_assets}
    #     DESTINATION ${pack_dir}
    # )

    # Command for generating the bitefile
    add_custom_command(
        OUTPUT ${pack_output}

        DEPENDS "${BITEFILE_TOOLS_DIR}/bite_packer.py"
        ${pack_dir}

        COMMAND "$<TARGET_FILE:Python::Interpreter>"
        "${BITEFILE_TOOLS_DIR}/bite_packer.py"
        --relative-to ${working_dir}
        --recursive
        --output ${pack_output}
        ${pack_dir}

        COMMENT "Packing all example assets into data.bite"

        VERBATIM
    )

    # Create a target for the bite.
    add_custom_target(example_pack_assets ALL
        DEPENDS ${pack_output}
    )
else()
    message(
        "No Python interpreter was found, the example Bite won't get generated!"
    )
endif()
