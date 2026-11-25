cmake_minimum_required(VERSION 3.16)
project(mimenotes VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

set(PROJECT_SOURCES
    main.cpp
    resources.qrc
)

if(APPLE AND NOT IOS)
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

qt_add_executable(mimenotes MANUAL_FINALIZATION ${PROJECT_SOURCES})

target_link_libraries(mimenotes PRIVATE Qt6::Widgets Qt6::Svg)

if(MSVC)
    target_compile_options(mimenotes PRIVATE /W4 /utf-8)
else()
    target_compile_options(mimenotes PRIVATE -Wall -Wextra)
endif()

if(WIN32)
    set_target_properties(mimenotes PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE AND NOT IOS)
    set_target_properties(mimenotes PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Mime Notes"
        MACOSX_BUNDLE_BUNDLE_NAME "Mime Notes"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.psurrena.mimenotes"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "mimenotes.icns"
    )
elseif(ANDROID)
    set_target_properties(mimenotes PROPERTIES
        QT_ANDROID_PACKAGE_NAME "com.psurrena.mimenotes"
        QT_ANDROID_APP_NAME "Mime Notes"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 2
        QT_ANDROID_MIN_SDK_VERSION 26
        QT_ANDROID_TARGET_SDK_VERSION 34
    )
endif()

qt_finalize_executable(mimenotes)
