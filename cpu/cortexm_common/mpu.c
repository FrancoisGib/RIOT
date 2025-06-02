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

int mpu_disable(void)
{
#if __MPU_PRESENT
    MPU->CTRL &= ~MPU_CTRL_ENABLE_Msk;
    return 0;
#else
    return -1;
#endif
}

int mpu_enable(void)
{
#if __MPU_PRESENT
    MPU->CTRL |= MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
#  ifdef SCB_SHCSR_MEMFAULTENA_Msk
    /* Enable the memory fault exception if SCB SHCSR (System Handler Control
     * and State Register) has a separate bit for mem faults. That is the case
     * on ARMv7-M. ARMv6-M does not support separate exception enable for mem
     * faults and all fault conditions cause a HardFault. */
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
#  endif

    return 0;
#else
    return -1;
#endif
}

bool mpu_enabled(void)
{
#if __MPU_PRESENT
    return (MPU->CTRL & MPU_CTRL_ENABLE_Msk) != 0;
#else
    return false;
#endif
}

int mpu_configure(uint_fast8_t region, uintptr_t base, uint_fast32_t attr)
{
    /* Todo enable MPU support for Cortex-M23/M33 */
#if __MPU_PRESENT && !defined(__ARM_ARCH_8M_MAIN__) && !defined(__ARM_ARCH_8M_BASE__)
    MPU->RNR = region;
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

/**
 * @brief Initialize the MPU regions
 */
void init_mpu(void)
{
    for (uint8_t i = 0; i < (int8_t)MPU_NUM_REGIONS - 1; i++) {
        regions[i] = i + 1;
    }
    regions[MPU_NUM_REGIONS - 1] = -1;

    for (uint8_t i = 0; i < (int8_t)MPU_NUM_REGIONS; i++) {
        MPU->RNR = i;
        MPU->RBAR = 0;
        MPU->RASR = 0;
    }
}

/**
 * @pre init_mpu must be called before allocating regions
 * 
 * @brief Allocate a free MPU region
 *
 * @return The newly allocated region index, or -1 if no region is available
 */
int8_t alloc_region(void)
{
    if (first_free_region == -1) {
        return -1;
    }
    int8_t region = first_free_region;
    first_free_region = regions[region];
    regions[region] = -1;
    return region;
}

/**
 * @brief Free an allocated MPU region
 *
 * @param region The index of the region to free. Ignored if invalid
 */
void free_region(int8_t region)
{
    if (region >= (int8_t)MPU_NUM_REGIONS || region == -1) {
        return;
    }
    regions[region] = first_free_region;
    first_free_region = region;

    MPU->RNR = region;
    MPU->RASR &= ~MPU_RASR_ENABLE_Msk;
}

/**
 * @internal
 *
 * @brief Round up to the next power of 2
 *
 * @param v The value to round
 *
 * @return The next power of two of the specified number
 */
static uint32_t next_power_of_two(uint32_t v)
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

/**
 * @internal
 *
 * @brief Get the base 2 logarithm of a number
 *
 * @param n The value to compute the logarithm for
 *
 * @return The base-2 logarithm of the specified number
 */
static uint8_t log2_n(uint32_t n)
{
    uint8_t power = 0;
    while (n >>= 1) {
        power++;
    }
    return power;
}

/**
 * @internal
 *
 * @brief Align an address down to the specified size
 *
 * @param addr The address to align
 * 
 * @param size The size of the region (must be a power of two)
 *
 * @return The aligned address.
 */
static inline uint32_t align_address_to_region_size(void *addr, uint32_t size)
{
    return (uint32_t)addr & ~(size - 1);
}

/**
 * @brief Configure a memory region in the MPU
 * Allocates an MPU region and configures it based on the provided parameters
 * The size is rounded up to the nearest power of two, and the base address is aligned to the region size
 *
 * @param addr The base address of the memory region
 * 
 * @param size The size of the memory region in bytes
 * 
 * @param xn Execute Never flag (1 = EXC_NO, 0 = EXC_OK)
 * 
 * @param ap Access permission value
 *
 * @return The index of the configured region, or -1 on failure
 */
int8_t configure_region(void *addr, uint32_t size, uint8_t xn, uint8_t ap)
{
    if (size == 0) {
        return -1;
    }

    int8_t region = alloc_region();

    if (region == -1) {
        return -1;
    }

    size = next_power_of_two(size);
    uint32_t aligned_address = align_address_to_region_size(addr, size);
    uint32_t pow = log2_n(size) - 1; // -1 because MPU regions sizes are 2^n+1

    uint32_t attr = MPU_ATTR(xn, ap, 0, 1, 0, 1, pow);
    mpu_configure(region, aligned_address, attr);
    return region;
}
