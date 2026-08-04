service_active:=$(shell systemctl is-active Tuxedo-Fan-Control.service)

ifneq ($(VERBOSE) , ON)
	VERBOSE=OFF
endif

compile:
ifeq ($(VERBOSE) , ON)
	g++ -DVERBOSE Tuxedo-Fan-Control.cpp -o Tuxedo-Fan-Control
else
	g++ Tuxedo-Fan-Control.cpp -o Tuxedo-Fan-Control
endif

install-bin: compile
	chmod +x Tuxedo-Fan-Control
ifeq ($(service_active) , active)
	service Tuxedo-Fan-Control stop
endif
	cp Tuxedo-Fan-Control /usr/local/bin/
ifeq ($(service_active) , active)
	service Tuxedo-Fan-Control start
endif

install-service:install-bin
	cp Tuxedo-Fan-Control.service /etc/systemd/system/
	systemctl enable Tuxedo-Fan-Control.service

all: install-service
	service Tuxedo-Fan-Control start
	
clean:
	rm Tuxedo-Fan-Control

uninstall:
	service Tuxedo-Fan-Control stop
	systemctl disable Tuxedo-Fan-Control.service
	rm /etc/systemd/system/Tuxedo-Fan-Control.service
	rm /usr/local/bin/Tuxedo-Fan-Control
