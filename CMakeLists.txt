cmake_minimum_required(VERSION 3.16)

project(Tuxedo-Fan-Control LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SYSTEMD_UNIT_DIR "/etc/systemd/system" CACHE PATH
    "Installation directory for the systemd unit")

add_library(fan_curve_logic STATIC fan_curve.cpp)
target_include_directories(fan_curve_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(fan_curve_logic PUBLIC cxx_std_11)

add_executable(Tuxedo-Fan-Control Tuxedo-Fan-Control.cpp)
target_link_libraries(Tuxedo-Fan-Control PRIVATE fan_curve_logic)

target_compile_features(Tuxedo-Fan-Control PRIVATE cxx_std_11)

install(TARGETS Tuxedo-Fan-Control
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT runtime
)

install(FILES Tuxedo-Fan-Control.service
    DESTINATION ${SYSTEMD_UNIT_DIR}
    COMPONENT service
)

install(FILES doc/Tuxedo-Fan-Control.8
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man8
    COMPONENT runtime
)

install(FILES doc/fan-curve.png
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT runtime
)

include(CTest)
enable_testing()

if(BUILD_TESTING)
  add_executable(Tuxedo-Fan-ControlTests
      tests/fan_curve_test.cpp
  )
  target_link_libraries(Tuxedo-Fan-ControlTests PRIVATE fan_curve_logic)
  add_test(NAME fan_curve_boundaries COMMAND Tuxedo-Fan-ControlTests)
endif()
