cmake_minimum_required(VERSION 3.21)

project(Deckade VERSION 0.2.10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Network
    WebSockets
    Svg
    Concurrent
)
find_package(SDL2 REQUIRED)

qt_standard_project_setup()

set(DECKADE_SOURCES
    src/main.cpp
    src/core/settingsstore.cpp
    src/core/settingsstore.h
    src/core/theme.cpp
    src/core/theme.h
    src/core/gamepadmanager.cpp
    src/core/gamepadmanager.h
    src/core/focuscontroller.cpp
    src/core/focuscontroller.h
    src/core/appcontroller.cpp
    src/core/appcontroller.h
    src/core/matchhistory.cpp
    src/core/matchhistory.h
    src/core/friendsstore.cpp
    src/core/friendsstore.h
    src/fightcade/fightcadeclient.cpp
    src/fightcade/fightcadeclient.h
    src/fightcade/fightcadelauncher.cpp
    src/fightcade/fightcadelauncher.h
    src/fightcade/romlibrary.cpp
    src/fightcade/romlibrary.h
    src/fightcade/controllerprofile.cpp
    src/fightcade/controllerprofile.h
    src/fightcade/protocol.cpp
    src/fightcade/protocol.h
    src/fightcade/netplayproxy.cpp
    src/fightcade/netplayproxy.h
    src/fightcade/netplaypreflight.cpp
    src/fightcade/netplaypreflight.h
    src/fightcade/portmapping.cpp
    src/fightcade/portmapping.h
    src/fightcade/userstatus.cpp
    src/fightcade/userstatus.h
)

qt_add_executable(deckade ${DECKADE_SOURCES})

target_compile_definitions(deckade PRIVATE DECKADE_VERSION="${PROJECT_VERSION}")

target_include_directories(deckade PRIVATE src)

target_link_libraries(deckade PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::WebSockets
    Qt6::Svg
    Qt6::Concurrent
    SDL2::SDL2
)

file(GLOB_RECURSE DECKADE_QML CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/qmldir"
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.js"
)
file(GLOB_RECURSE DECKADE_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/flags/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ranks/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/rom-json/*.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/rom-json/*.txt"
)

qt_add_resources(deckade "deckade_resources"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        ${DECKADE_QML}
        ${DECKADE_ASSETS}
)

set_target_properties(deckade PROPERTIES
    OUTPUT_NAME Deckade
    MACOSX_BUNDLE FALSE
    WIN32_EXECUTABLE FALSE
)

install(TARGETS deckade RUNTIME DESTINATION bin)
install(FILES packaging/com.deckade.Deckade.desktop DESTINATION share/applications)
install(FILES packaging/com.deckade.Deckade.appdata.xml DESTINATION share/metainfo)
install(FILES assets/icons/deckade.svg DESTINATION share/icons/hicolor/scalable/apps RENAME com.deckade.Deckade.svg)
install(FILES assets/icons/deckade.png DESTINATION share/icons/hicolor/256x256/apps RENAME com.deckade.Deckade.png)

# Development smoke test: verifies the live Fightcade bootstrap + websocket login path.
add_executable(deckade_client_smoke
    tests/client_smoke.cpp
    src/fightcade/fightcadeclient.cpp
    src/fightcade/protocol.cpp
    src/fightcade/userstatus.cpp
)
target_include_directories(deckade_client_smoke PRIVATE src)
target_link_libraries(deckade_client_smoke PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
)

add_executable(deckade_live_probe
    tests/live_probe.cpp
    src/core/settingsstore.cpp
    src/fightcade/fightcadeclient.cpp
    src/fightcade/protocol.cpp
    src/fightcade/userstatus.cpp
)
target_include_directories(deckade_live_probe PRIVATE src)
target_link_libraries(deckade_live_probe PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
)

add_executable(deckade_launcher_smoke
    tests/launcher_smoke.cpp
    src/core/settingsstore.cpp
    src/fightcade/fightcadelauncher.cpp
    src/fightcade/protocol.cpp
    src/fightcade/netplayproxy.cpp
    src/fightcade/netplaypreflight.cpp
    src/fightcade/portmapping.cpp
)
target_include_directories(deckade_launcher_smoke PRIVATE src)
target_link_libraries(deckade_launcher_smoke PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
)

add_executable(deckade_rom_probe
    tests/rom_probe.cpp
    src/core/settingsstore.cpp
    src/fightcade/fightcadelauncher.cpp
    src/fightcade/romlibrary.cpp
    src/fightcade/protocol.cpp
    src/fightcade/netplayproxy.cpp
    src/fightcade/netplaypreflight.cpp
    src/fightcade/portmapping.cpp
    src/fightcade/userstatus.cpp
)
target_include_directories(deckade_rom_probe PRIVATE src)
target_link_libraries(deckade_rom_probe PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
)

enable_testing()

add_executable(deckade_protocol_tests
    tests/protocol_tests.cpp
    src/fightcade/protocol.cpp
    src/fightcade/userstatus.cpp
)
target_include_directories(deckade_protocol_tests PRIVATE src)
target_compile_definitions(deckade_protocol_tests PRIVATE
    DECKADE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(deckade_protocol_tests PRIVATE Qt6::Core)
add_test(NAME deckade_protocol_tests COMMAND deckade_protocol_tests)

add_executable(deckade_netplay_tests
    tests/netplay_tests.cpp
    src/fightcade/netplayprotocol.cpp
)
target_include_directories(deckade_netplay_tests PRIVATE src)
target_link_libraries(deckade_netplay_tests PRIVATE Qt6::Core)
add_test(NAME deckade_netplay_tests COMMAND deckade_netplay_tests)

add_executable(deckade_netplay_proxy_tests
    tests/netplay_proxy_tests.cpp
    src/fightcade/netplayproxy.cpp
    src/fightcade/netplaypreflight.cpp
    src/fightcade/portmapping.cpp
)
target_include_directories(deckade_netplay_proxy_tests PRIVATE src)
target_link_libraries(deckade_netplay_proxy_tests PRIVATE Qt6::Core Qt6::Network)
add_test(NAME deckade_netplay_proxy_tests COMMAND deckade_netplay_proxy_tests)

add_executable(deckade_social_tests
    tests/social_tests.cpp
    src/core/matchhistory.cpp
    src/core/friendsstore.cpp
    src/fightcade/netplaypreflight.cpp
)
target_include_directories(deckade_social_tests PRIVATE src)
target_link_libraries(deckade_social_tests PRIVATE Qt6::Core Qt6::Network)
add_test(NAME deckade_social_tests COMMAND deckade_social_tests)

# Ship the native runtimes and their build notes alongside the app.
install(DIRECTORY runtime/ DESTINATION share/deckade/runtime USE_SOURCE_PERMISSIONS)
install(FILES README.md LICENSE DESTINATION share/doc/deckade)

set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME "Deckade")
set(CPACK_PACKAGE_VENDOR "Deckade contributors")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Controller-first Fightcade frontend for Linux and Steam Deck")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
include(CPack)
