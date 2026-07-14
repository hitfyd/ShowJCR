cmake_minimum_required(VERSION 3.5)

project(ShowJCR LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check https://doc.qt.io/qt/deployment-android.html for more information.
# They need to be set before the find_package( ...) calls below.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
if(LINUX)
    include(GNUInstallDirs)
    if(POLICY CMP0072)
        cmake_policy(SET CMP0072 NEW)
        set(OpenGL_GL_PREFERENCE GLVND)
    endif()
    find_package(OpenGL REQUIRED)
endif()

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Sql REQUIRED)
set(PROJECT_SOURCES
        main.cpp
        showjcr.cpp
        showjcr.h
        sqlitedb.cpp
        sqlitedb.h
        aboutdialog.cpp
        aboutdialog.h
        tableselectordialog.h
        tableselectordialog.cpp
        showjcr.ui
        aboutdialog.ui
        tableselectordialog.ui
)
set(app_WINRC resources/logo.rc)
set(qrc_SOURCES resources/resources.qrc)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ShowJCR WIN32 #增加WIN32，否则独立运行时会出现命令行
        ${PROJECT_SOURCES}
        ${app_WINRC}
        ${qrc_SOURCES}

    )
else()
    if(ANDROID)
        add_library(ShowJCR SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(ShowJCR
            ${PROJECT_SOURCES}
        )
    endif()
endif()
if(LINUX)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core DBus)
    target_link_libraries(ShowJCR PRIVATE Qt${QT_VERSION_MAJOR}::DBus)
    set_target_properties(ShowJCR PROPERTIES OUTPUT_NAME "showjcr")
    install(TARGETS ShowJCR
            RUNTIME
        )
    install(FILES 中科院分区表及JCR原始数据文件/jcr.db DESTINATION ${CMAKE_INSTALL_DATADIR}/ShowJCR)
    install(FILES resources/io.hitfyd.ShowJCR.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/logo-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME io.hitfyd.ShowJCR.png)
endif()
target_link_libraries(ShowJCR PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(ShowJCR PRIVATE Qt${QT_VERSION_MAJOR}::Sql)

# ============================================================
# 部署目标: cmake --build . --target deploy
# 运行 windeployqt 并清理冗余文件（Qt6Network、触摸插件、OpenGL软件渲染器等）
# ============================================================
add_custom_target(deploy
    COMMAND "${CMAKE_COMMAND}"
        -DEXE_PATH="$<TARGET_FILE:ShowJCR>"
        -DQT_BIN_DIR="$<TARGET_FILE_DIR:Qt6::Core>"
        -P "${CMAKE_SOURCE_DIR}/deploy.cmake"
    COMMENT "Deploying ShowJCR with minimal dependencies..."
    DEPENDS ShowJCR
)
