#
# Copyright (c) 2013-2026, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

OPTEED_DIR		:=	services/spd/opteed
SPD_INCLUDES		:=

SPD_SOURCES		:=	services/spd/opteed/opteed_common.c	\
				services/spd/opteed/opteed_helpers.S	\
				services/spd/opteed/opteed_main.c	\
				services/spd/opteed/opteed_pm.c

NEED_BL32		:=	yes

# WARNING: This enables loading of OP-TEE via an SMC, which can be potentially
# insecure. This removes the boundary between the startup of the secure and
# non-secure worlds until the point where this SMC is invoked. Only use this
# setting if you can ensure that the non-secure OS can remain trusted up until
# the point where this SMC is invoked.
OPTEE_ALLOW_SMC_LOAD		:=	0
ifeq ($(OPTEE_ALLOW_SMC_LOAD),1)
ifeq ($(PLAT_XLAT_TABLES_DYNAMIC),0)
$(error When OPTEE_ALLOW_SMC_LOAD=1, PLAT_XLAT_TABLES_DYNAMIC must also be 1)
endif
$(warning "OPTEE_ALLOW_SMC_LOAD is enabled which may result in an insecure \
	platform")
$(eval $(call add_define,PLAT_XLAT_TABLES_DYNAMIC))
include lib/libfdt/libfdt.mk
endif
$(eval $(call add_define,OPTEE_ALLOW_SMC_LOAD))

# Only run an SMC-loaded image that carries a valid ed25519 signature (see
# opteed_sig.h). The image is copied into secure memory before it is checked, so
# the non-secure world cannot change it afterwards. Needs OPTEE_SIG_PUBKEY, the
# raw 32-byte public key; OPTEE_SIG_MIN_VERSION rejects older signed images.
OPTEE_SMC_LOAD_SIGNED		:=	0
ifeq ($(OPTEE_SMC_LOAD_SIGNED),1)
ifeq ($(OPTEE_ALLOW_SMC_LOAD),0)
$(error When OPTEE_SMC_LOAD_SIGNED=1, OPTEE_ALLOW_SMC_LOAD must also be 1)
endif
ifndef OPTEE_SIG_PUBKEY
$(error When OPTEE_SMC_LOAD_SIGNED=1, OPTEE_SIG_PUBKEY must name the public key file)
endif
OPTEE_SIG_MIN_VERSION		?=	0
SPD_SOURCES		+=	services/spd/opteed/opteed_sig.c	\
				services/spd/opteed/opteed_sig_pubkey.S	\
				lib/monocypher/monocypher.c		\
				lib/monocypher/monocypher-ed25519.c
SPD_INCLUDES		+=	-Ilib/monocypher
$(eval $(call add_define,OPTEE_SIG_MIN_VERSION))
$(eval $(call add_define_val,OPTEE_SIG_PUBKEY,'"$(OPTEE_SIG_PUBKEY)"'))
endif
$(eval $(call add_define,OPTEE_SMC_LOAD_SIGNED))

CROS_WIDEVINE_SMC		:=	0
ifeq ($(CROS_WIDEVINE_SMC),1)
ifeq ($(OPTEE_ALLOW_SMC_LOAD),0)
$(error When CROS_WIDEVINE_SMC=1, OPTEE_ALLOW_SMC_LOAD must also be 1)
endif
endif
$(eval $(call add_define,CROS_WIDEVINE_SMC))
