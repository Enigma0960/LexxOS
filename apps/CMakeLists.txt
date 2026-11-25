set(LEXXOS_APPS_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/build/apps")
set(_lexx_app_aggregate apps)
include("../cmake/LexxAppSDKConfig.cmake")

add_subdirectory(sdk-demo)
add_subdirectory(ui-demo)
# ui-tree-demo is superseded by the tabbed gallery in ui-demo.
add_subdirectory(fileman)
add_subdirectory(pacman)
add_subdirectory(snake)
add_subdirectory(tetris)
