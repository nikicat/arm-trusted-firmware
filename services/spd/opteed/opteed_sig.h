/*
 * Copyright (c) 2026, Nikolay Bryskin
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef OPTEED_SIG_H
#define OPTEED_SIG_H

#include <stddef.h>
#include <stdint.h>

/*
 * Signed OP-TEE image, as handed over with OPTEE_SMC_LOAD_SIGNED=1:
 *   struct opteed_sig_hdr | OP-TEE image (header v2 + images) | 64-byte ed25519
 * signature over the SHA-512 of everything before it. Little-endian.
 */
struct opteed_sig_hdr {
	uint8_t magic[8];	/* "OPTEESIG" */
	uint32_t hdr_version;	/* 1 */
	uint32_t image_version;	/* rejected below OPTEE_SIG_MIN_VERSION */
};

#define OPTEED_SIG_LEN		64U
#define OPTEED_SIG_PUBKEY_LEN	32U

/*
 * Check the signature and the version of a signed image. On success the OP-TEE
 * image is blob[*payload_off .. *payload_off + *payload_len).
 * Returns 0, or -1 (format), -2 (signature), -3 (version too old).
 */
int opteed_sig_verify(const uint8_t *blob, size_t len, uint32_t min_version,
		      const uint8_t pubkey[OPTEED_SIG_PUBKEY_LEN],
		      size_t *payload_off, size_t *payload_len,
		      uint32_t *image_version);

#endif /* OPTEED_SIG_H */
