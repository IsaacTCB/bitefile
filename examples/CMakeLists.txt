project(examples)

set(example_dirs
    basic
)

set(example_assets)

foreach(example_dir ${example_dirs})
    set(example_name ${example_dir})
    file(GLOB sources ${example_dir}/*.c)

    file(GLOB assets ${example_dir}/assets/*)
    list(APPEND example_assets ${assets})

    add_executable(${example_dir} ${sources})
    target_link_libraries(${example_name} PRIVATE bitefile)
endforeach()

# Copy assets and bundle the all into the same bite file
file(COPY ${example_assets} DESTINATION "assets/")

find_package(Python REQUIRED COMPONENTS Interpreter)
add_custom_command(
        OUTPUT "data.bite"
        DEPENDS "${BITE_TOOLS_DIR}/bite_packer.py"

        COMAND "$<TARGET_FILE:Python::Interpreter>"
        "${BITE_TOOLS_DIR}/bite_packer.py"
)
