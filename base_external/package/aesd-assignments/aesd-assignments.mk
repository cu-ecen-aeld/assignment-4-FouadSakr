
##############################################################
#
# AESD-ASSIGNMENTS
#
##############################################################

# NOTE: Repository contains your aesd assignments content including the `server/` directory.
# Pin to a specific commit for reproducible builds.
AESD_ASSIGNMENTS_VERSION = 2222ce7da6199407a8d258cdbf9ad080789bee9e
# Note: Be sure to reference the *ssh* repository URL here (not https) to work properly
# with ssh keys and the automated build/test system.
# Your site should start with git@github.com:
AESD_ASSIGNMENTS_SITE = git@github.com:cu-ecen-aeld/assignment-4-FouadSakr.git
AESD_ASSIGNMENTS_SITE_METHOD = git
AESD_ASSIGNMENTS_GIT_SUBMODULES = YES

define AESD_ASSIGNMENTS_BUILD_CMDS
	$(MAKE) -C $(@D)/server clean
	$(MAKE) -C $(@D)/server \
		CC="$(TARGET_CC)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		all
endef

define AESD_ASSIGNMENTS_INSTALL_TARGET_CMDS
	# Install aesdsocket binary
	$(INSTALL) -m 0755 $(@D)/server/aesdsocket $(TARGET_DIR)/usr/bin/
	# Install init script
	$(INSTALL) -D -m 0755 $(@D)/server/aesdsocket-start-stop \
		$(TARGET_DIR)/etc/init.d/S99aesdsocket
endef

$(eval $(generic-package))
