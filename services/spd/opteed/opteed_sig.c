/*
 * Copyright (c) 2026, Nikolay Bryskin
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include <monocypher-ed25519.h>

#include "opteed_sig.h"

int opteed_sig_verify(const uint8_t *blob, size_t len, uint32_t min_version,
		      const uint8_t pubkey[OPTEED_SIG_PUBKEY_LEN],
		      size_t *payload_off, size_t *payload_len,
		      uint32_t *image_version)
{
	const struct opteed_sig_hdr *hdr = (const struct opteed_sig_hdr *)blob;
	size_t signed_len;

	if (len < sizeof(*hdr) + OPTEED_SIG_LEN)
		return -1;
	if (memcmp(hdr->magic, "OPTEESIG", sizeof(hdr->magic)) != 0 ||
	    hdr->hdr_version != 1U)
		return -1;

	signed_len = len - OPTEED_SIG_LEN;
	if (crypto_ed25519_check(blob + signed_len, pubkey, blob,
				 signed_len) != 0)
		return -2;
	if (hdr->image_version < min_version)
		return -3;

	*payload_off = sizeof(*hdr);
	*payload_len = signed_len - sizeof(*hdr);
	*image_version = hdr->image_version;
	return 0;
}
