cmake_minimum_required(VERSION 3.20)

project(llfcchat VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Qml Widgets QuickControls2 Network Sql Concurrent)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)

set(QWINDOWKIT_BUILD_QUICK ON CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_DECLARATIVE ON CACHE BOOL "" FORCE)
add_subdirectory(qwindowkit-main)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appllfcchat
    main.cpp
    appicon.rc
)

qt_add_qml_module(appllfcchat
    URI llfcchat
    VERSION 1.0
    RESOURCES qml.qrc
    RESOURCES res.qrc
    SOURCES
    RESOURCES config.ini
    SOURCES global.h
    SOURCES global.cpp
    SOURCES Net_Layer/tcpmgr.cpp Net_Layer/tcpmgr.h
    SOURCES Net_Layer/tcpworker.h Net_Layer/tcpworker.cpp
    SOURCES Logic_Layer/logicmgr.h Logic_Layer/logicmgr.cpp
    SOURCES Http_Layer/httpmgr.cpp Http_Layer/httpmgr.h Sqlite_Layer/sqlitemgr.cpp Sqlite_Layer/sqlitemgr.h UserData_Layer/usermgr.cpp UserData_Layer/usermgr.h
    SOURCES ChatMsg_Layer/chatmgr.cpp ChatMsg_Layer/chatmgr.h Model_Layer/chatmsgmodel.cpp Model_Layer/chatmsgmodel.h Model_Layer/friendlistmodel.cpp Model_Layer/friendlistmodel.h Model_Layer/searchmodel.cpp Model_Layer/searchmodel.h
    SOURCES Sqlite_Layer/sqlworker.h Sqlite_Layer/sqlworker.cpp
    SOURCES Model_Layer/conversationsmodel.h Model_Layer/conversationsmodel.cpp
    SOURCES ChatMsg_Layer/chatsession.h ChatMsg_Layer/chatsession.cpp
    SOURCES ChatMsg_Layer/sendratelimiter.h ChatMsg_Layer/sendratelimiter.cpp
    SOURCES Model_Layer/friendapplymodel.h Model_Layer/friendapplymodel.cpp
    SOURCES Logic_Layer/actiondebouncer.h Logic_Layer/actiondebouncer.cpp
)

target_include_directories(appllfcchat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Net_Layer
    ${CMAKE_CURRENT_SOURCE_DIR}/Logic_Layer
    ${CMAKE_CURRENT_SOURCE_DIR}/Http_Layer
    ${CMAKE_CURRENT_SOURCE_DIR}/Sqlite_Layer
    ${CMAKE_CURRENT_SOURCE_DIR}/UserData_Layer
    ${CMAKE_CURRENT_SOURCE_DIR}/ChatMsg_Layer
    ${CMAKE_CURRENT_SOURCE_DIR}/Model_Layer
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appllfcchat PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appllfcchat
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appllfcchat
    PRIVATE Qt6::Quick Qt6::Qml Qt6::Widgets Qt6::QuickControls2 Qt6::Network Qt6::Core Qt6::Sql Qt6::Concurrent
    QWindowKit::Quick
)
target_link_libraries(appllfcchat PRIVATE Qt6::Core)
target_link_libraries(appllfcchat PRIVATE Qt6::Core)
target_link_libraries(appllfcchat PRIVATE Qt6::Core)

include(GNUInstallDirs)
install(TARGETS appllfcchat
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (WIN32)

add_custom_command(TARGET appllfcchat
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config.ini
            $<TARGET_FILE_DIR:appllfcchat>
)

endif()
