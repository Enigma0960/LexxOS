include_guard(GLOBAL)

set(LEXXOS_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
get_filename_component(LEXXOS_ROOT_DIR "${LEXXOS_ROOT_DIR}" ABSOLUTE)


message(STATUS "LexxOS ESP-IDF project")
message(STATUS "  platform: ${LEXXOS_PLATFORM}")
message(STATUS "  board:    ${LEXXOS_BOARD}")
message(STATUS "  target:   ${IDF_TARGET}")
