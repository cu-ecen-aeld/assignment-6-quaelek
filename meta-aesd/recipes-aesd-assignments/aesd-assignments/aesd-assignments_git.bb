# License information with a reference to the common licenses
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# Source URI for fetching the application source code using the ssh protocol
SRC_URI = "git@github.com:cu-ecen-aeld/assignments-3-and-later-ericksyndrome.git;protocol=ssh;branch=main"

# Package version with Git SRCREV information
PV = "1.0+git${SRCPV}"
SRCREV = "bbe896292e96e9972af57c65fc29cbf31b02365c"

# Set the source directory for building the application
S = "${WORKDIR}/git/server"

# Inherit the update-rc.d class for managing init scripts
inherit update-rc.d

# Define init package and script name
INITSCRIPT_PACKAGES = "${PN}"
INITSCRIPT_NAME:${PN} = "aesdsocket-start-stop.sh"

# Define binary installation path and library flags
FILES:${PN} += "${bindir}/aesdsocket"
TARGET_LDFLAGS += "-pthread -lrt"

do_configure () {
	:
}

do_compile () {
	oe_runmake
}

do_install () {
	# Create target directories for binaries and init scripts
	install -d ${D}${bindir}/
	install -m 0755 ${S}/aesdsocket ${D}${bindir}/
	install -d ${D}${sysconfdir}/init.d
	install -m 0755 ${S}/aesdsocket-start-stop.sh ${D}${sysconfdir}/init.d
}