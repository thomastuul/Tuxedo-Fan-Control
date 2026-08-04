cmake_minimum_required(VERSION 3.16)

project(Tuxedo-Fan-Control LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SYSTEMD_UNIT_DIR "/etc/systemd/system" CACHE PATH
    "Installation directory for the systemd unit")

add_executable(Tuxedo-Fan-Control Tuxedo-Fan-Control.cpp fan_curve.cpp)

target_compile_features(Tuxedo-Fan-Control PRIVATE cxx_std_11)

install(TARGETS Tuxedo-Fan-Control
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT runtime
)

install(FILES Tuxedo-Fan-Control.service
    DESTINATION ${SYSTEMD_UNIT_DIR}
    COMPONENT service
)

include(CTest)
enable_testing()

if(BUILD_TESTING)
  add_executable(Tuxedo-Fan-ControlTests
      tests/fan_curve_test.cpp
      fan_curve.cpp
  )
  target_include_directories(Tuxedo-Fan-ControlTests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  target_compile_features(Tuxedo-Fan-ControlTests PRIVATE cxx_std_11)
  add_test(NAME fan_curve_boundaries COMMAND Tuxedo-Fan-ControlTests)
endif()
