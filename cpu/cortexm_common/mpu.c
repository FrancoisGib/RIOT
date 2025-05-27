/*
 * Copyright (C) 2016 Loci Controls Inc.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_cortexm_common
 * @{
 *
 * @file        mpu.c
 * @brief       Cortex-M Memory Protection Unit (MPU) Driver
 *
 * @author      Ian Martin <ian@locicontrols.com>
 *
 * @}
 */

#include "cpu.h"
#include "mpu.h"

int mpu_disable(void) {
#if __MPU_PRESENT
    MPU->CTRL &= ~MPU_CTRL_ENABLE_Msk;
    return 0;
#else
    return -1;
#endif
}

int mpu_enable(void) {
#if __MPU_PRESENT
    MPU->CTRL |= MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
#ifdef SCB_SHCSR_MEMFAULTENA_Msk
    /* Enable the memory fault exception if SCB SHCSR (System Handler Control
     * and State Register) has a separate bit for mem faults. That is the case
     * on ARMv7-M. ARMv6-M does not support separate exception enable for mem
     * faults and all fault conditions cause a HardFault. */
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
#endif

    return 0;
#else
    return -1;
#endif
}

bool mpu_enabled(void) {
#if __MPU_PRESENT
    return (MPU->CTRL & MPU_CTRL_ENABLE_Msk) != 0;
#else
    return false;
#endif
}

int mpu_configure(uint_fast8_t region, uintptr_t base, uint_fast32_t attr) {
    /* Todo enable MPU support for Cortex-M23/M33 */
#if __MPU_PRESENT && !defined(__ARM_ARCH_8M_MAIN__) && !defined(__ARM_ARCH_8M_BASE__)
    MPU->RNR  = region;
    MPU->RBAR = base & MPU_RBAR_ADDR_Msk;
    MPU->RASR = attr | MPU_RASR_ENABLE_Msk;

    return 0;
#else
    (void)region;
    (void)base;
    (void)attr;
    return -1;
#endif
}


int8_t regions[MPU_NUM_REGIONS];
int8_t first_free_region = 0;

uint8_t init_mpu(void) {
    if (MPU_NUM_REGIONS == 0) {
        return 1;
    }
    for (uint8_t i = 0; i < (int8_t)MPU_NUM_REGIONS - 1; i++) {
        regions[i] = i + 1;
    }
    regions[MPU_NUM_REGIONS - 1] = -1;
    
    for (uint8_t i = 0; i < (int8_t)MPU_NUM_REGIONS; i++)
    {
        MPU->RNR = i;
        MPU->RBAR = 0;
        MPU->RASR = 0;
    }
    return 0;
}

int8_t alloc_region(void) {
    if (first_free_region == -1) {
        return -1;
    }
    int8_t region = first_free_region;
    first_free_region = regions[region];
    regions[region] = -1;
    return region;
}

void free_region(int8_t region) {
    if (region >= (int8_t)MPU_NUM_REGIONS || region == -1) {
        return;
    }
    regions[region] = first_free_region;
    first_free_region = region;
}

static uint32_t build_rasr(uint8_t xn, uint8_t ap, uint8_t size)
{
    return 1 | (xn << 28) | (ap << 24) | (size << 1);
}

static uint32_t build_rbar(uint32_t addr)
{
    return addr & ~0b11111;
}

static uint32_t next_pow2(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

#include <stdio.h>

static uint8_t get_next_log2_from_n(uint32_t n) {
    uint32_t pow2_size = next_pow2(n);
    // printf("next %ld %ld\n", pow2_size, n);
    uint8_t power = 0;
    while (pow2_size >>= 1) {
        power++;
    }
    
    return power;
}


int8_t configure_region(void* addr, uint32_t size, uint8_t xn, uint8_t ap)
{
    int8_t region = alloc_region();
    if (region != -1) {
        MPU->RNR = region;
        MPU->RBAR = build_rbar((uint32_t)addr);
        uint32_t pow = get_next_log2_from_n(size);
        MPU->RASR = build_rasr(xn, ap, pow - 1); // -1 because MPU regions sizes are 2^n+1
        // printf("%p - %p, pow %ld, size %ld\n", addr, addr + size, pow - 1, size);
    }
    return region;
}

int8_t configure_region_if_in_range(void* addr, uint32_t size, uint8_t xn, uint8_t ap, void* begin_address, void* end_address) {
    if (addr >= begin_address && (addr + size) <= end_address) {
        return configure_region(addr, size, xn, ap);
    }
    return -1;
}
