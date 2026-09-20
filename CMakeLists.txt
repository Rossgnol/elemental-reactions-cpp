cmake_minimum_required(VERSION 3.20)

project(elemental_reactions VERSION 0.1.0 LANGUAGES CXX)

option(ELEMENTAL_BUILD_TESTS "Build the unit tests" ON)
option(ELEMENTAL_BUILD_EXAMPLE "Build the command-line example" ON)

add_library(elemental_reactions
    src/battle.cpp
    src/reaction.cpp
)
add_library(elemental::reactions ALIAS elemental_reactions)

function(elemental_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endfunction()

target_include_directories(elemental_reactions
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(elemental_reactions PUBLIC cxx_std_17)
elemental_enable_warnings(elemental_reactions)

if(ELEMENTAL_BUILD_EXAMPLE)
    add_executable(elemental_demo examples/demo.cpp)
    target_link_libraries(elemental_demo PRIVATE elemental::reactions)
    elemental_enable_warnings(elemental_demo)

    add_executable(elemental_team_demo examples/team_simulation.cpp)
    target_link_libraries(elemental_team_demo PRIVATE elemental::reactions)
    elemental_enable_warnings(elemental_team_demo)
endif()

include(CTest)
if(BUILD_TESTING AND ELEMENTAL_BUILD_TESTS)
    add_executable(elemental_tests tests/reaction_tests.cpp)
    target_link_libraries(elemental_tests PRIVATE elemental::reactions)
    elemental_enable_warnings(elemental_tests)
    add_test(NAME elemental_tests COMMAND elemental_tests)

    add_executable(elemental_battle_tests tests/battle_tests.cpp)
    target_link_libraries(elemental_battle_tests PRIVATE elemental::reactions)
    elemental_enable_warnings(elemental_battle_tests)
    add_test(NAME elemental_battle_tests COMMAND elemental_battle_tests)
endif()

install(TARGETS elemental_reactions EXPORT elemental_reactionsTargets)
install(DIRECTORY include/ DESTINATION include)
install(
    EXPORT elemental_reactionsTargets
    NAMESPACE elemental::
    DESTINATION lib/cmake/elemental_reactions
)
