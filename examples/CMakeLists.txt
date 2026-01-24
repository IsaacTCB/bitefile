project(examples)

set(example_dirs
    basic
)

set(example_assets)
foreach(example_dir ${example_dirs})
    set(example_name ${example_dir})
    file(GLOB sources ${example_dir}/*.c)

    # Store all filepaths from the assets folder.
    file(GLOB_RECURSE assets ${example_dir}/assets/*)
    list(APPEND example_assets ${assets})

    add_executable(${example_dir} ${sources})
    target_link_libraries(${example_name} PRIVATE bitefile)
endforeach()

# =========================
# Create example data.bite
# =========================

# Warning: this code's a mess, but it works so whatever
find_package(Python QUIET COMPONENTS Interpreter)
if(Python_FOUND)
    set(working_dir ${CMAKE_CURRENT_BINARY_DIR})
    set(pack_dir "${working_dir}/assets")
    set(pack_output "${working_dir}/data.bite")

    # Copy assets to project folder for easier access
    file(
        COPY ${example_assets}
        DESTINATION ${pack_dir}
    )

    # The python packing script only accepts relative paths,
    # so we need to convert them first.
    set(input_paths)
    file(GLOB_RECURSE files_to_pack "${pack_dir}/*")
    foreach(asset ${files_to_pack})
        file(RELATIVE_PATH path ${working_dir} ${asset})
        list(APPEND input_paths ${path})
    endforeach()

    # Command for generating the bitefile
    add_custom_command(
        OUTPUT ${pack_output}

        DEPENDS "${BITEFILE_TOOLS_DIR}/bite_packer.py"
        ${files_to_pack}

        COMMAND "$<TARGET_FILE:Python::Interpreter>"
        "${BITEFILE_TOOLS_DIR}/bite_packer.py"
        ${input_paths}
        -o ${pack_output}

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
