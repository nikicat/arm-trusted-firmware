/*
 * Copyright (c) 2026, Nikolay Bryskin
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* The raw 32-byte ed25519 public key signed OP-TEE images are checked against */
	.global opteed_sig_pubkey
	.section .rodata.opteed_sig_pubkey, "a"
	.align 3
opteed_sig_pubkey:
	.incbin OPTEE_SIG_PUBKEY
opteed_sig_pubkey_end:
	.if opteed_sig_pubkey_end - opteed_sig_pubkey != 32
	.error "OPTEE_SIG_PUBKEY must be a raw 32-byte ed25519 public key"
	.endif
