/*
 * Copyright (c) 2016-2023, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <platform_def.h>

#include <common/bl_common.h>
#include <common/debug.h>
#include <lib/utils.h>
#include <common/desc_image_load.h>
#include <drivers/console.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/ti/uart/uart_16550.h>
#include <lib/mmio.h>
#include <plat_private.h>
#include <plat/common/platform.h>

#ifndef PLAT_RK_SEC_DRAM_BASE
#define PLAT_RK_SEC_DRAM_BASE	0
#define PLAT_RK_SEC_DRAM_SIZE	0
#endif

static entry_point_info_t bl32_ep_info;
static entry_point_info_t bl33_ep_info;

/*******************************************************************************
 * Return a pointer to the 'entry_point_info' structure of the next image for
 * the security state specified. BL33 corresponds to the non-secure image type
 * while BL32 corresponds to the secure image type. A NULL pointer is returned
 * if the image does not exist.
 ******************************************************************************/
entry_point_info_t *bl31_plat_get_next_image_ep_info(uint32_t type)
{
	entry_point_info_t *next_image_info;

	next_image_info = (type == NON_SECURE) ? &bl33_ep_info : &bl32_ep_info;
	assert(next_image_info->h.type == PARAM_EP);

	/* None of the images on this platform can have 0x0 as the entrypoint */
	if (next_image_info->pc)
		return next_image_info;
	else
		return NULL;
}

#pragma weak params_early_setup
void params_early_setup(u_register_t plat_param_from_bl2)
{
}

/*******************************************************************************
 * Perform any BL3-1 early platform setup. Here is an opportunity to copy
 * parameters passed by the calling EL (S-EL1 in BL2 & EL3 in BL1) before they
 * are lost (potentially). This needs to be done before the MMU is initialized
 * so that the memory layout can be used while creating page tables.
 * BL2 has flushed this information to memory, so we are guaranteed to pick up
 * good data.
 ******************************************************************************/
void bl31_early_platform_setup2(u_register_t arg0, u_register_t arg1,
				u_register_t arg2, u_register_t arg3)
{
	static console_t console;

	params_early_setup(arg1);

	if (rockchip_get_uart_base() != 0)
		console_16550_register(rockchip_get_uart_base(),
				       rockchip_get_uart_clock(),
				       rockchip_get_uart_baudrate(), &console);

	VERBOSE("bl31_setup\n");

	bl31_params_parse_helper(arg0, &bl32_ep_info, &bl33_ep_info);

	/*
	 * Rockchip's SPL loads a FIT "optee" image at its load address but
	 * passes no BL32 entry point, so OP-TEE never starts. Use the FIT's
	 * fixed load address when the loader passed nothing. The SPL leaves
	 * the BL32 arguments uninitialised too; the OP-TEE dispatcher reads
	 * arg0 as the AArch32/AArch64 selector, so clear them.
	 */
	if (bl32_ep_info.pc == 0 && PLAT_RK_SEC_DRAM_BASE != 0) {
		SET_PARAM_HEAD(&bl32_ep_info, PARAM_EP, VERSION_1, 0);
		SET_SECURITY_STATE(bl32_ep_info.h.attr, SECURE);
		bl32_ep_info.pc = PLAT_RK_SEC_DRAM_BASE;
		bl32_ep_info.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX,
					    DISABLE_ALL_EXCEPTIONS);
		zeromem(&bl32_ep_info.args, sizeof(bl32_ep_info.args));
	}

	/*
	 * A loader that does pass a BL32 entry point decides where BL32 runs,
	 * so check it against the window this platform firewalls rather than
	 * assuming the two agree.  Only the entry point is available here (the
	 * handover carries no BL32 size), so a BL32 that starts inside the
	 * window but outgrows it is left for BL32 itself to notice.
	 */
	if (PLAT_RK_SEC_DRAM_SIZE != 0 && bl32_ep_info.pc != 0 &&
	    (bl32_ep_info.pc < PLAT_RK_SEC_DRAM_BASE ||
	     bl32_ep_info.pc >= PLAT_RK_SEC_DRAM_BASE + PLAT_RK_SEC_DRAM_SIZE)) {
		ERROR("BL32 entry 0x%lx outside the secure DRAM window 0x%x-0x%x\n",
		      (unsigned long)bl32_ep_info.pc, PLAT_RK_SEC_DRAM_BASE,
		      PLAT_RK_SEC_DRAM_BASE + PLAT_RK_SEC_DRAM_SIZE);
		panic();
	}
}

/*******************************************************************************
 * Perform any BL3-1 platform setup code
 ******************************************************************************/
void bl31_platform_setup(void)
{
	generic_delay_timer_init();
	plat_rockchip_soc_init();

	/* Initialize the gic cpu and distributor interfaces */
	plat_rockchip_gic_driver_init();
	plat_rockchip_gic_init();
	plat_rockchip_pmu_init();

#ifdef BOARD
	plat_rockchip_board_init();
#endif
}

/*******************************************************************************
 * Perform the very early platform specific architectural setup here. At the
 * moment this is only initializes the mmu in a quick and dirty way.
 ******************************************************************************/
void bl31_plat_arch_setup(void)
{
	plat_cci_init();
	plat_cci_enable();
#if USE_COHERENT_MEM
	plat_configure_mmu_el3(BL_CODE_BASE,
			       BL_COHERENT_RAM_END - BL_CODE_BASE,
			       BL_CODE_BASE,
			       BL_CODE_END,
			       BL_COHERENT_RAM_BASE,
			       BL_COHERENT_RAM_END);
#else
	plat_configure_mmu_el3(BL31_START,
			       BL31_END - BL31_START,
			       BL_CODE_BASE,
			       BL_CODE_END,
			       0,
			       0);
#endif
}
