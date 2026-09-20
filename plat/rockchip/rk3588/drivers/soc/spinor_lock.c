/*
 * Copyright (c) 2026, Nikolay Bryskin
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Write-protect the boot firmware in the SPI NOR at every boot: block-protect
 * the bottom of the chip and set the status-register lock-down, which nothing
 * can undo until the chip loses power. Reads are unaffected. Assumes the
 * Winbond-style status registers most 3V quad SPI NOR parts share (W25Qxx,
 * XT25Fxx, GD25Qxx): SR1 = SRP0 SEC TB BP2 BP1 BP0 WEL WIP, SR2 bit 0 = SRP1.
 * SRP1=1 with SRP0=0 is "power supply lock-down".
 */

#include <string.h>

#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>
#include <lib/utils_def.h>

#include "soc.h"

#define SFC_BASE		0xfe2b0000
#define SFC_CTRL		0x000
#define SFC_CTRL_PHASE_SEL_NEG	BIT(1)
#define SFC_IMR			0x004
#define SFC_ICLR		0x008
#define SFC_RCVR		0x010
#define SFC_RCVR_RESET		BIT(0)
#define SFC_FSR			0x020
#define SFC_FSR_TXLV_MASK	GENMASK(12, 8)
#define SFC_FSR_RXLV_MASK	GENMASK(20, 16)
#define SFC_SR			0x024
#define SFC_SR_BUSY		BIT(0)
#define SFC_VER			0x02c
#define SFC_LEN_CTRL		0x088
#define SFC_LEN_CTRL_TRB_SEL	BIT(0)
#define SFC_LEN_EXT		0x08c
#define SFC_CMD			0x100
#define SFC_CMD_DIR_WR		BIT(12)
#define SFC_CMD_TRAN_BYTES_SHIFT 16
#define SFC_DATA		0x108

#define NOR_WRSR		0x01
#define NOR_RDSR1		0x05
#define NOR_RDSR2		0x35
#define NOR_WREN_VOLATILE	0x50
#define NOR_SR1_WIP		BIT(0)
#define NOR_SR2_SRP1		BIT(0)

static int sfc_wait(uint32_t reg, uint32_t mask, bool set, uint32_t us)
{
	while (us-- > 0) {
		if (!!(mmio_read_32(SFC_BASE + reg) & mask) == set)
			return 0;
		udelay(1);
	}
	return -1;
}

static int sfc_init(void)
{
	mmio_write_32(SFC_BASE + SFC_RCVR, SFC_RCVR_RESET);
	if (sfc_wait(SFC_RCVR, SFC_RCVR_RESET, false, 100000))
		return -1;
	mmio_write_32(SFC_BASE + SFC_ICLR, 0xffffffff);
	mmio_write_32(SFC_BASE + SFC_CTRL, 0);
	mmio_write_32(SFC_BASE + SFC_IMR, 0xffffffff);
	if ((mmio_read_32(SFC_BASE + SFC_VER) & 0xffff) >= 4)
		mmio_write_32(SFC_BASE + SFC_LEN_CTRL, SFC_LEN_CTRL_TRB_SEL);
	return 0;
}

/* One single-line command without address and with up to 4 data bytes, chip select 0 */
static int sfc_xfer(uint8_t opcode, uint8_t *buf, uint32_t len, bool out)
{
	uint32_t cmd = opcode | (out ? SFC_CMD_DIR_WR : 0);
	uint32_t word = 0;

	if (sfc_wait(SFC_SR, SFC_SR_BUSY, false, 100000))
		return -1;
	if ((mmio_read_32(SFC_BASE + SFC_VER) & 0xffff) >= 4)
		mmio_write_32(SFC_BASE + SFC_LEN_EXT, len);
	else
		cmd |= len << SFC_CMD_TRAN_BYTES_SHIFT;
	mmio_write_32(SFC_BASE + SFC_CTRL, SFC_CTRL_PHASE_SEL_NEG);
	mmio_write_32(SFC_BASE + SFC_CMD, cmd);
	if (len != 0 && out) {
		memcpy(&word, buf, len);
		if (sfc_wait(SFC_FSR, SFC_FSR_TXLV_MASK, true, 1000))
			return -1;
		mmio_write_32(SFC_BASE + SFC_DATA, word);
	} else if (len != 0) {
		if (sfc_wait(SFC_FSR, SFC_FSR_RXLV_MASK, true, 1000))
			return -1;
		word = mmio_read_32(SFC_BASE + SFC_DATA);
		memcpy(buf, &word, len);
	}
	return sfc_wait(SFC_SR, SFC_SR_BUSY, false, 100000);
}

void rk3588_spinor_lock(void)
{
	uint8_t sr[2] = { RK3588_SPINOR_LOCK_SR1, 0 };
	uint8_t sr1 = 0, sr2 = 0;
	int i;

	if (sfc_init() || sfc_xfer(NOR_RDSR1, &sr1, 1, false) ||
	    sfc_xfer(NOR_RDSR2, &sr2, 1, false)) {
		ERROR("SPI NOR: controller not responding, firmware left writable\n");
		return;
	}
	if (sr1 == sr[0] && (sr2 & NOR_SR2_SRP1) != 0) {
		NOTICE("SPI NOR: already locked (warm reboot)\n");
		return;
	}
	sr[1] = sr2 | NOR_SR2_SRP1;	/* keep QE and the rest of SR2 */
	if (sfc_xfer(NOR_WREN_VOLATILE, NULL, 0, false) ||
	    sfc_xfer(NOR_WRSR, sr, 2, true)) {
		ERROR("SPI NOR: lock command failed\n");
		return;
	}
	for (i = 0; i < 1000; i++) {
		sfc_xfer(NOR_RDSR1, &sr1, 1, false);
		if ((sr1 & NOR_SR1_WIP) == 0)
			break;
		udelay(10);
	}
	sfc_xfer(NOR_RDSR2, &sr2, 1, false);
	if (sr1 == sr[0] && (sr2 & NOR_SR2_SRP1) != 0)
		NOTICE("SPI NOR: locked, SR1 0x%x SR2 0x%x\n", sr1, sr2);
	else
		ERROR("SPI NOR: lock FAILED, SR1 0x%x SR2 0x%x\n", sr1, sr2);
}
