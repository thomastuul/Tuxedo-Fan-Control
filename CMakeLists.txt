cmake_minimum_required(VERSION 3.16)

project(Tuxedo-Fan-Control LANGUAGES CXX)

include(GNUInstallDirs)

set(TUXEDO_FAN_CONTROL_VERSION "0.0.0+local" CACHE STRING
    "Version used for generated packages")

if(NOT TUXEDO_FAN_CONTROL_VERSION MATCHES "^[0-9][A-Za-z0-9.+~-]*$")
  message(FATAL_ERROR
      "TUXEDO_FAN_CONTROL_VERSION is not a valid Debian package version")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SYSTEMD_UNIT_DIR "/etc/systemd/system" CACHE STRING
    "Installation directory for the systemd unit")

add_library(fan_curve_logic STATIC fan_curve.cpp)
target_include_directories(fan_curve_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(fan_curve_logic PUBLIC cxx_std_11)

add_library(ec_controller_logic STATIC ec_controller.cpp)
target_include_directories(ec_controller_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(ec_controller_logic PUBLIC cxx_std_11)

add_executable(Tuxedo-Fan-Control Tuxedo-Fan-Control.cpp)
target_link_libraries(Tuxedo-Fan-Control PRIVATE fan_curve_logic ec_controller_logic)

target_compile_features(Tuxedo-Fan-Control PRIVATE cxx_std_11)

configure_file(
    Tuxedo-Fan-Control.service
    ${CMAKE_CURRENT_BINARY_DIR}/Tuxedo-Fan-Control.service
    @ONLY
)

install(TARGETS Tuxedo-Fan-Control
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT runtime
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Tuxedo-Fan-Control.service
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

install(FILES LICENSE README.md TECHNICAL_ANALYSIS.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT runtime
)

include(CTest)
enable_testing()

if(BUILD_TESTING)
  add_executable(Tuxedo-Fan-ControlTests
      tests/ec_controller_test.cpp
      tests/fan_curve_test.cpp
  )
  target_link_libraries(Tuxedo-Fan-ControlTests PRIVATE
      ec_controller_logic
      fan_curve_logic
  )
  add_test(NAME unit_tests COMMAND Tuxedo-Fan-ControlTests)
endif()

set(CPACK_PACKAGE_NAME "tuxedo-fan-control")
set(CPACK_PACKAGE_VENDOR "Tuxedo-Fan-Control contributors")
set(CPACK_PACKAGE_CONTACT "Thomas Tuul")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "EC-based fan controller for compatible TUXEDO/Clevo laptops")
set(CPACK_PACKAGE_HOMEPAGE_URL
    "https://github.com/thomastuul/Tuxedo-Fan-Control")
set(CPACK_PACKAGE_VERSION "${TUXEDO_FAN_CONTROL_VERSION}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_NAME "tuxedo-fan-control")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "systemd")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
set(CPACK_GENERATOR "DEB")

include(CPack)
