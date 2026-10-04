cmake_minimum_required(VERSION 3.13)
project(vhs LANGUAGES CXX)

include(GNUInstallDirs)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VHS_VERSION)
string(STRIP "${VHS_VERSION}" VHS_VERSION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vhs_core src/tape.cpp src/executor.cpp src/terminal.cpp src/renderer.cpp src/video.cpp src/record.cpp)
target_include_directories(vhs_core PUBLIC include)
target_compile_definitions(vhs_core PRIVATE _XOPEN_SOURCE=600)
target_compile_options(vhs_core PRIVATE -Wall -Wextra -Wpedantic)
if(HAIKU)
    target_link_libraries(vhs_core PUBLIC be translation)
endif()

add_executable(vhs src/main.cpp)
target_link_libraries(vhs PRIVATE vhs_core)
target_compile_definitions(vhs PRIVATE VHS_VERSION="${VHS_VERSION}")

enable_testing()
add_executable(tape_test tests/tape_test.cpp)
target_link_libraries(tape_test PRIVATE vhs_core)
add_test(NAME tape_test COMMAND tape_test)
add_executable(terminal_screen_test tests/terminal_screen_test.cpp)
target_link_libraries(terminal_screen_test PRIVATE vhs_core)
add_test(NAME terminal_screen_test COMMAND terminal_screen_test)
add_test(NAME cli_test
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.sh $<TARGET_FILE:vhs>)

install(TARGETS vhs RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES docs/vhs.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES completions/vhs.bash
    DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
    RENAME vhs)
install(FILES completions/_vhs
    DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions)
install(FILES completions/vhs.fish
    DESTINATION ${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d)
install(FILES README.md docs/ROADMAP.md docs/RELEASE.md CHANGELOG.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY examples DESTINATION ${CMAKE_INSTALL_DOCDIR})
