cmake_minimum_required(VERSION 3.16)

cmake_policy(SET CMP0048 NEW)  # project() manages VERSION variables
cmake_policy(SET CMP0071 NEW)  # AUTOMOC/AUTOUIC skip GENERATED files cleanly

project(BurstRF VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)              # libsigmf v1.0 requires C++17 for std::optional
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.8)

# ── FetchContent for libsigmf ─────────────────────────────────────────────────
include(FetchContent)

# Cache FetchContent downloads so offline builds work after first configure
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# Disable warnings-as-errors globally for all fetched deps
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF CACHE BOOL "" FORCE)

# Also explicitly tell flatbuffers not to build as werror
set(FLATBUFFERS_WERROR OFF CACHE BOOL "" FORCE)

# Tell libsigmf to use system-installed flatbuffers and nlohmann-json
# (requires: sudo dnf install  nlohmann-json-devel )
# flatbuffers-devel flatbuffers-compiler -> don't work, have to let libsigmf pull the dependency.
set(USE_SYSTEM_JSON        ON   CACHE BOOL "" FORCE)
set(USE_SYSTEM_FLATBUFFERS OFF  CACHE BOOL "" FORCE)

FetchContent_Declare(
    libsigmf
    GIT_REPOSITORY https://github.com/SigMF/libsigmf.git
    GIT_TAG        fc8f6e8346c4fc8a5440b97045de10271e0ab94e     # Pin to a commit hash for reproducibility.
    GIT_SHALLOW    TRUE                                         # Only fetch latest, saves time/space
)
FetchContent_MakeAvailable(libsigmf)

# Patch bundled flatbuffers stl_emulation.h - const member assignment
# breaks on GCC 13+ with -Wtemplate-body
include(patches/fix_flatbuffers_stl_emulation.cmake)
# ─────────────────────────────────────────────────────────────────────────────

# ── FetchContent for kissfft ──────────────────────────────────────────────────
set(KISSFFT_DATATYPE    float  CACHE STRING "" FORCE)  # matches std::complex<float>
set(KISSFFT_STATIC      ON     CACHE BOOL   "" FORCE)  # static lib, no install headaches
set(KISSFFT_TEST        OFF    CACHE BOOL   "" FORCE)  # don't build kissfft tests
set(KISSFFT_TOOLS       OFF    CACHE BOOL   "" FORCE)  # don't build CLI tools

FetchContent_Declare(
    kissfft
    GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
    GIT_TAG        131.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(kissfft)
# ─────────────────────────────────────────────────────────────────────────────

qt_add_executable(appBurstRF
    main.cpp
)

qt_add_qml_module(appBurstRF
    URI BurstRF
    QML_FILES
        Main.qml
        RESOURCES README.md
        SOURCES ImageBackend.h
        SOURCES ChunkImageResponse.h
        SOURCES ChunkImageProvider.h
        SOURCES SpectrogramBuilder.h SpectrogramBuilder.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appBurstRF PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appBurstRF
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appBurstRF
    PRIVATE Qt6::Quick
            libsigmf                # libsigmf target
            flatbuffers             # flatbuffers is needed at link time per their README
            kissfft::kissfft-float  # kissfft for std::complex<float>
)

# libsigmf generates headers into the build tree; this makes them findable
target_include_directories(appBurstRF
    PRIVATE
        ${CMAKE_BINARY_DIR}/include
)

include(GNUInstallDirs)
install(TARGETS appBurstRF
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
