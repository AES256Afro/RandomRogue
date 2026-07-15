cmake_minimum_required(VERSION 3.24)
project(RandomRogue VERSION 0.1.0 LANGUAGES C CXX)

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
    src/events.cpp
)
target_link_libraries(random_rogue PRIVATE raylib nlohmann_json::nlohmann_json)
target_include_directories(random_rogue PRIVATE src)

if(EMSCRIPTEN)
    # Browser build: raylib needs GLFW-over-web + asyncify main loop support.
    set_target_properties(random_rogue PROPERTIES SUFFIX ".html")
    target_link_options(random_rogue PRIVATE
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sFORCE_FILESYSTEM=1"
        "--shell-file" "${CMAKE_SOURCE_DIR}/web/shell.html"
        "--preload-file" "${CMAKE_SOURCE_DIR}/assets@assets"
    )
    target_compile_definitions(random_rogue PRIVATE PLATFORM_WEB)
else()
    # Ship assets next to the executable.
    add_custom_command(TARGET random_rogue POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:random_rogue>/assets")
    if(WIN32)
        # No console window in release; keep it in debug for logs.
        target_link_options(random_rogue PRIVATE $<$<CONFIG:Release>:-mwindows>)
    endif()
endif()
