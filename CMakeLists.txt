cmake_minimum_required(VERSION 3.16)
project(SpaceDregs C)

#===========================================================================
# C Standard & Compiler Flags
#===========================================================================
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)  # Strict C99, no GNU extensions

#===========================================================================
# Directory Structure
#===========================================================================
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(ASSETS_DIR ${CMAKE_SOURCE_DIR}/assets)
set(BUILD_ASSETS_DIR ${CMAKE_BINARY_DIR}/assets)

#===========================================================================
# SDL3 Dependency Options
#===========================================================================
option(USE_SYSTEM_SDL3 "Use system-installed SDL3" OFF)
option(USE_EMSCRIPTEN "Build for Emscripten/WebAssembly" OFF)

#===========================================================================
# Source Files
#===========================================================================
file(GLOB_RECURSE SOURCES "${SRC_DIR}/*.c")
file(GLOB_RECURSE HEADERS "${SRC_DIR}/*.h")

#===========================================================================
# Platform-Specific Configuration
#===========================================================================
if(USE_EMSCRIPTEN)
    message(STATUS "Configuring for Emscripten (WebAssembly)")

    # Emscripten toolchain configuration
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    # SDL3 via emscripten's bundled port (needs emsdk >= 4.0.2)
    add_compile_options(--use-port=sdl3)

    # Linker-only settings
    add_link_options(
        --use-port=sdl3
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=67108864  # 64MB
        -sFORCE_FILESYSTEM=1
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
        -lidbfs.js                # IndexedDB-backed FS for config persistence
        --preload-file ${ASSETS_DIR}@/assets  # game data -> SpaceDregs.data
        --shell-file ${CMAKE_SOURCE_DIR}/shell.html
    )
else()
    message(STATUS "Configuring for Native Build")

    if(NOT USE_SYSTEM_SDL3)
        message(STATUS "Fetching SDL3 from GitHub")

        # SDL3 via FetchContent (CMake 3.14+)
        include(FetchContent)

        FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.8  # Pin specific version
        )

        # SDL3 options: static-only so packaged binaries are self-contained
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)

        FetchContent_MakeAvailable(SDL3)

        set(SDL3_INCLUDE_DIR ${SDL3_SOURCE_DIR}/include)
        set(SDL3_LIB SDL3::SDL3)
    else()
        # Find system SDL3
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL3 REQUIRED sdl3)
        set(SDL3_LIB ${SDL3_LIBRARIES})
        set(SDL3_INCLUDE_DIR ${SDL3_INCLUDEDIR})
    endif()
endif()

#==============================================================================
# Executable
#==============================================================================
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Git version stamp: regenerated each build into version.h
set(VERSION_HEADER ${CMAKE_BINARY_DIR}/generated/version.h)
add_custom_target(gitversion
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DOUTPUT_FILE=${VERSION_HEADER}
        -P ${CMAKE_SOURCE_DIR}/cmake/GitVersion.cmake
    BYPRODUCTS ${VERSION_HEADER}
    COMMENT "Stamping git version"
)
add_dependencies(${PROJECT_NAME} gitversion)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SRC_DIR}
    ${CMAKE_BINARY_DIR}/generated
    ${SDL3_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SDL3_LIB}
)

if(WIN32)
    # GUI subsystem (no console window); SDL_main.h supplies the entry point
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MINGW)
        target_link_options(${PROJECT_NAME} PRIVATE -static)  # no dll deps
    endif()
endif()

#==========================================================================
# Optimization Flags (Release Builds)
#==========================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(USE_EMSCRIPTEN)
        target_compile_options(${PROJECT_NAME} PRIVATE -O3)
        target_link_options(${PROJECT_NAME} PRIVATE
            -O3
            -flto
        )
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE
            -O3
            -march=native
            -DNDEBUG
        )
    endif()
endif()

#==========================================================================
# Debug Symbols
#==========================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -g
        -O0
        -fsanitize=address
    )
    if(NOT USE_EMSCRIPTEN)
        target_link_options(${PROJECT_NAME} PRIVATE
            -fsanitize=address
        )
    endif()
endif()

#==========================================================================
# Copy Assets to Build Directory
#==========================================================================
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BUILD_ASSETS_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${ASSETS_DIR}
        ${BUILD_ASSETS_DIR}
    DEPENDS ${ASSETS_DIR}
)

add_dependencies(${PROJECT_NAME} copy_assets)

#==========================================================================
# Installation Target
#==========================================================================
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${ASSETS_DIR}/ DESTINATION share/${PROJECT_NAME}/assets)

#==========================================================================
# Convenience Targets
#==========================================================================
if(NOT USE_EMSCRIPTEN)
    add_custom_target(run
        COMMAND $<TARGET_FILE:${PROJECT_NAME}>
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Desktop packaging: dist builds the platform's shippable zip into dist/.
    # macOS: signed (ad hoc) .app bundle. Windows/Linux: binary + assets.
    set(DIST_DIR ${CMAKE_SOURCE_DIR}/dist)
    if(APPLE)
        set(BUNDLE ${CMAKE_BINARY_DIR}/SpaceDregs.app)
        add_custom_target(dist
            COMMAND ${CMAKE_COMMAND} -E rm -rf ${BUNDLE}
            COMMAND ${CMAKE_COMMAND} -E make_directory
                ${BUNDLE}/Contents/MacOS ${BUNDLE}/Contents/Resources
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/cmake/Info.plist
                ${BUNDLE}/Contents/Info.plist
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}>
                ${BUNDLE}/Contents/MacOS/SpaceDregs
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR}
                ${BUNDLE}/Contents/Resources/assets
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE
                ${BUNDLE}/Contents/Resources/LICENSE
            COMMAND codesign --force --deep --sign - ${BUNDLE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DIST_DIR}
            COMMAND ditto -c -k --keepParent --norsrc ${BUNDLE}
                ${DIST_DIR}/spacedregs-macos.zip
            DEPENDS ${PROJECT_NAME}
            COMMENT "Packaging dist/spacedregs-macos.zip"
        )
    else()
        if(WIN32)
            set(DIST_ZIP spacedregs-windows.zip)
        else()
            set(DIST_ZIP spacedregs-linux.zip)
        endif()
        set(STAGE ${CMAKE_BINARY_DIR}/dist-stage/SpaceDregs)
        add_custom_target(dist
            COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/dist-stage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${STAGE}
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${STAGE}
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_DIR} ${STAGE}/assets
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE ${STAGE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DIST_DIR}
            COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/dist-stage
                ${CMAKE_COMMAND} -E tar cf ${DIST_DIR}/${DIST_ZIP} --format=zip
                SpaceDregs
            DEPENDS ${PROJECT_NAME}
            COMMENT "Packaging dist/${DIST_ZIP}"
        )
    endif()
endif()

if(USE_EMSCRIPTEN)
    add_custom_target(emrun
        COMMAND emrun --no_browser --port 8888 .
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ${PROJECT_NAME}
    )

    # Bundle the browser build for itch.io: dist/spacedregs-web.zip with
    # index.html as the entry point. Configure with -DCMAKE_BUILD_TYPE=Release
    # for a shippable build, then: cmake --build build-web --target itch
    # Relink when embedded assets change (preload happens at link time)
    file(GLOB_RECURSE ASSET_FILES "${ASSETS_DIR}/*")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_DEPENDS "${ASSET_FILES}")

    set(DIST_DIR ${CMAKE_SOURCE_DIR}/dist)
    add_custom_target(itch
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DIST_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${DIST_DIR}/index.html
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.js
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.wasm
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.data
            ${DIST_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE ${DIST_DIR}
        COMMAND ${CMAKE_COMMAND} -E chdir ${DIST_DIR} ${CMAKE_COMMAND} -E tar cf
            spacedregs-web.zip --format=zip
            index.html ${PROJECT_NAME}.js ${PROJECT_NAME}.wasm ${PROJECT_NAME}.data
            LICENSE
        DEPENDS ${PROJECT_NAME}
        COMMENT "Bundling itch.io web package into dist/spacedregs-web.zip"
    )
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
