BUILD_DIR ?= build
CMAKE_BUILD_TYPE ?= Release

CMAKE_CONFIGURE_ARGS = \
	-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

ifeq ($(VERBOSE),ON)
CMAKE_CONFIGURE_ARGS += -DCMAKE_VERBOSE_MAKEFILE=ON
endif

.PHONY: configure compile install-bin install-service all clean uninstall

configure:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_CONFIGURE_ARGS)

compile: configure
	cmake --build $(BUILD_DIR)

install-bin: compile
	cmake --install $(BUILD_DIR) --component runtime

install-service: install-bin
	cmake --install $(BUILD_DIR) --component service
	systemctl enable Tuxedo-Fan-Control.service

all: install-service
	service Tuxedo-Fan-Control start

clean:
	cmake --build $(BUILD_DIR) --target clean

uninstall:
	-service Tuxedo-Fan-Control stop
	-systemctl disable Tuxedo-Fan-Control.service
	-rm /etc/systemd/system/Tuxedo-Fan-Control.service
	-rm /usr/local/bin/Tuxedo-Fan-Control
	-systemctl daemon-reload
