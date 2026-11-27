cmake_minimum_required(VERSION 3.16)

cmake_policy(SET CMP0048 NEW)  # project() manages VERSION variables
cmake_policy(SET CMP0071 NEW)  # AUTOMOC/AUTOUIC skip GENERATED files cleanly

project(BurstRF VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)              # sigmf_io requires C++23
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.8)

# ── FetchContent ──────────────────────────────────────────────────────────────
include(FetchContent)

# Cache FetchContent downloads so offline builds work after first configure
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# Disable warnings-as-errors globally for all fetched deps
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF CACHE BOOL "" FORCE)


# ── FetchContent for sigmf_io ─────────────────────────────────────────────────
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)   # don't build sigmf_io's test suite as a subproject

FetchContent_Declare(
    sigmf_io
    GIT_REPOSITORY https://github.com/Edwin-Sanchez2003/sigmf-io.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(sigmf_io)
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

# Re-enable warnings-as-errors globally for this project's source code.
set(CMAKE_COMPILE_WARNING_AS_ERROR ON CACHE BOOL "" FORCE)

qt_add_executable(appBurstRF
    src/main.cpp
)

qt_add_qml_module(appBurstRF
    URI BurstRF
    QML_FILES
        qml/Main.qml
        qml/components/FrequencyAxis.qml
        qml/components/TimeAxis.qml
    SOURCES
        # Backend
        include/backend/burstrf_backend.h
        include/backend/spectrogram_builder.h
        include/backend/params.h
        include/backend/colormaps.h
        include/backend/tools.h
        src/backend/burstrf_backend.cpp
        src/backend/spectrogram_builder.cpp
        src/backend/params.cpp

        # Frontend
        include/frontend/spectrogram_item.h
        # src/frontend/spectrogram_item.cpp   (add when it exists)
)

set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(BurstRFTheme
    URI BurstRF.Theme
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/BurstRF/Theme
    QML_FILES
        qml/theme/Theme.qml
    NO_PLUGIN
)

target_include_directories(appBurstRF PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/backend
    ${CMAKE_CURRENT_SOURCE_DIR}/include/frontend
    ${CMAKE_BINARY_DIR}/include
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
            kissfft::kissfft-float  # kissfft for std::complex<float>
            sigmf_io::sigmf_io
            BurstRFTheme
)

include(GNUInstallDirs)
install(TARGETS appBurstRF
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
