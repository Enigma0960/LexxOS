message(FATAL_ERROR
        "ports/espidf is a legacy ESP-IDF project entry point.\n"
        "Open the repository root as the CMake project and use the root presets instead:\n"
        "  cmake --preset debug-esp32\n"
        "  cmake --build --preset lexxos-esp32\n"
        "\n"
        "From any subdirectory, use the wrapper:\n"
        "  tools/lexxos-cmake --preset debug-esp32\n"
        "  tools/lexxos-cmake --build --preset lexxos-esp32")
