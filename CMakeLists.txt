cmake_minimum_required(VERSION 3.24)
project(RandomRogue VERSION 0.14.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---- raylib ----------------------------------------------------------------
set(FETCHCONTENT_QUIET OFF)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# ---- nlohmann/json (release tarball: small, no git history) ----------------
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# ---- game ------------------------------------------------------------------
add_executable(random_rogue
    src/main.cpp
    src/game.cpp
    src/grammar.cpp
    src/language.cpp
    src/world.cpp
    src/history.cpp
    src/events.cpp
    src/items.cpp
    src/audio.cpp
    src/save.cpp
    src/story.cpp
)

# ---- chronicle_dump: worldgen dev tool (native only, no raylib) -------------
if(NOT EMSCRIPTEN)
    add_executable(chronicle_dump
        tools/chronicle_dump.cpp
        src/grammar.cpp
        src/language.cpp
        src/world.cpp
        src/history.cpp
    )
    target_link_libraries(chronicle_dump PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(chronicle_dump PRIVATE src)

    # Content linter: dangling ids, unknown verbs, broken slot/grammar refs.
    add_executable(validate tools/validate.cpp)
    target_link_libraries(validate PRIVATE nlohmann_json::nlohmann_json)

    # Narrative coverage and structural similarity report for content authors.
    add_executable(analyze tools/analyze.cpp)
    target_link_libraries(analyze PRIVATE nlohmann_json::nlohmann_json)

    # Monte-Carlo balance bot: thousands of headless runs, data over vibes.
    add_executable(playtest
        tools/playtest.cpp
        src/events.cpp
        src/items.cpp
        src/language.cpp
    )
    target_link_libraries(playtest PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(playtest PRIVATE src)

    # Fast invariants that should never depend on graphics or a live run.
    add_executable(regression
        tools/regression.cpp
        src/grammar.cpp
        src/language.cpp
        src/world.cpp
        src/events.cpp
        src/story.cpp
    )
    target_link_libraries(regression PRIVATE nlohmann_json::nlohmann_json)
    target_include_directories(regression PRIVATE src)
endif()
target_link_libraries(random_rogue PRIVATE raylib nlohmann_json::nlohmann_json)
target_include_directories(random_rogue PRIVATE src)

if(EMSCRIPTEN)
    # Browser build: raylib needs GLFW-over-web + asyncify main loop support.
    set_target_properties(random_rogue PROPERTIES SUFFIX ".html")
    target_link_options(random_rogue PRIVATE
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sFORCE_FILESYSTEM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        # miniaudio's JS callback reads Module.HEAPF32; modern emscripten no
        # longer exports heap views by default, which silenced all web audio.
        "-sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAP32,HEAPU8,HEAPU32"
        "--shell-file" "${CMAKE_SOURCE_DIR}/web/shell.html"
        "--preload-file" "${CMAKE_SOURCE_DIR}/assets@assets"
    )
    target_compile_definitions(random_rogue PRIVATE PLATFORM_WEB)
else()
    # Ship assets next to the executable.
    add_custom_command(TARGET random_rogue POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:random_rogue>/assets")
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # No console window in release; keep it in debug for logs. (-mwindows
        # is a GCC flag; MSVC on CI uses its own defaults.)
        target_link_options(random_rogue PRIVATE $<$<CONFIG:Release>:-mwindows>)
    endif()
endif()
