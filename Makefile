QED_DIR := ${PWD}/qed-8.70.12.0/src/
QEDE_DIR := ${PWD}/qede-8.70.12.0/src/
QEDR_DIR := ${PWD}/qedr-8.70.12.0/src/
QEDF_DIR := ${PWD}/qedf-8.70.12.0
QEDI_DIR := ${PWD}/qedi-8.70.12.0
LIBQEDR_DIR := ${PWD}//
SUBDIRS := $(QED_DIR) $(QEDE_DIR) $(QEDR_DIR) $(QEDF_DIR) $(QEDI_DIR)
export QED_DIR
export QEDE_DIR

UBUNTU_DISTRO := $(shell lsb_release -is 2> /dev/null | grep Ubuntu)
ifeq ($(UBUNTU_DISTRO),)
    LIBQEDR_CONFIGURE_CMD := ./configure --prefix=/usr --libdir=${exec_prefix}/lib64 --sysconfdir=/etc
else
    LIBQEDR_CONFIGURE_CMD := ./configure --prefix=/usr --libdir=${exec_prefix}/lib --sysconfdir=/etc
endif

.PHONY: subsystem udev_install udev_uninstall install light_install clean libqedr_uninstall libqedr libqedr_install libqedr_clean

ADDONS_DIR := add-ons

subsystem:
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir || exit 1;	\
	done

udev_install:
	@ - $(ADDONS_DIR)/udev/udev_install.sh --install

udev_uninstall:
	@ - $(ADDONS_DIR)/udev/udev_install.sh --uninstall

install: udev_install
	@for dir in $(SUBDIRS); do			\
		$(MAKE) -C $$dir install || exit 1;	\
	done

light_install: udev_install
	@for dir in $(SUBDIRS); do                              \
		$(MAKE) -C $$dir light_install || exit 1;       \
	done

clean:
	@for dir in $(SUBDIRS); do			\
		$(MAKE) -C $$dir clean || exit 1;	\
	done

libqedr_uninstall:
	- rm -f /etc/libibverbs.d/qedr.driver
	- rm -f /usr/local/etc/libibverbs.d/qedr.driver
	- rm -f /usr/lib64/libqedr*
	- rm -f /usr/lib64/libibverbs/libqedr*
	- rm -f /usr/lib/libqedr*
	- rm -f /usr/lib/libibverbs/libqedr*
	- rm -f /lib64/libqedr*
	- rm -f /lib64/libibverbs/libqedr*
	- rm -f /lib/libqedr*
	- rm -f /lib/libibverbs/libqedr*

libqedr:
	@(cd $(LIBQEDR_DIR) && exec $(LIBQEDR_CONFIGURE_CMD)) && $(MAKE) -C $(LIBQEDR_DIR)

libqedr_install:
	@(cd $(LIBQEDR_DIR) && exec $(LIBQEDR_CONFIGURE_CMD)) && $(MAKE) -C $(LIBQEDR_DIR) install

libqedr_clean:
	$(MAKE) clean -C $(LIBQEDR_DIR)
