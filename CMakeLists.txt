cmake_minimum_required(VERSION 3.16)
project(mimenotes VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

set(PROJECT_SOURCES
    main.cpp
    resources.qrc
)

if(APPLE)
    set(MACOS_ICON "${CMAKE_CURRENT_BINARY_DIR}/mimenotes.icns")
    if(EXISTS "${MACOS_ICON}")
        list(APPEND PROJECT_SOURCES "${MACOS_ICON}")
        set_source_files_properties("${MACOS_ICON}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
    endif()
elseif(WIN32)
    set(WIN_RC "${CMAKE_CURRENT_BINARY_DIR}/mimenotes.rc")
    if(EXISTS "${WIN_RC}")
        list(APPEND PROJECT_SOURCES "${WIN_RC}")
    endif()
endif()

add_executable(mimenotes WIN32 MACOSX_BUNDLE ${PROJECT_SOURCES})

target_link_libraries(mimenotes PRIVATE Qt6::Widgets Qt6::Svg)

if(MSVC)
    target_compile_options(mimenotes PRIVATE /W4 /utf-8)
else()
    target_compile_options(mimenotes PRIVATE -Wall -Wextra)
endif()

if(APPLE)
    set_target_properties(mimenotes PROPERTIES
        OUTPUT_NAME "Mime Notes"
        MACOSX_BUNDLE_BUNDLE_NAME "Mime Notes"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.psurrena.mimenotes"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "mimenotes.icns"
    )
endif()
