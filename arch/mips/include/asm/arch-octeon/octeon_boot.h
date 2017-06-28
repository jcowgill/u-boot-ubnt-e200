/*
 * (C) Copyright 2004-2011
 * Cavium Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/**
 * @file octeon_boot.h
 *
 * $Id: octeon_boot.h 69916 2012-02-14 14:47:03Z bprakash $
 *
 */

#ifndef __OCTEON_BOOT_H__
#define __OCTEON_BOOT_H__
#include <octeon-app-init.h>
#include <asm/arch/octeon_mem_map.h>
#include <asm/arch/octeon-boot-info.h>

/* Defines for GPIO switch usage */
/* Switch 0 is failsafe/normal bootloader */
#define OCTEON_GPIO_SHOW_FREQ       (0x1 << 1)	/* switch 1 set shows freq
						 * display
						 */

#define OCTEON_BOOT_DESC_IMAGE_LINUX	(1 << 0)
#define OCTEON_BOOT_DESC_LITTLE_ENDIAN	(1 << 1)

uint64_t octeon_get_cycles (void);
extern uint32_t cur_exception_base;

#if defined(__U_BOOT__)
extern octeon_boot_descriptor_t boot_desc[CVMX_MAX_CORES];
#endif

extern uint32_t coremask_iter_core;
extern uint32_t coremask_iter_mask;
extern int coremask_iter_first_core;
extern uint32_t coremask_to_run;
extern uint64_t boot_cycle_adjustment;
extern uint32_t coremask_from_eeprom;

int octeon_bist (void);
uint32_t get_except_base_reg (void);
uint32_t get_core_num (void);
uint32_t get_except_base_addr (void);
void set_except_base_addr (uint32_t addr);
void copy_default_except_handlers (uint32_t addr);
void cvmx_set_cycle (uint64_t cycle);
uint32_t get_coremask_override (void);
uint32_t octeon_coremask_num_cores (int num_cores);
int coremask_iter_get_first_core (void);
int coremask_iter_next (void);
int coremask_iter_init (uint32_t mask);
uint64_t get_cop0_cvmctl_reg (void);
void set_cop0_cvmctl_reg (uint64_t val);
uint64_t get_cop0_cvmmemctl_reg (void);
void set_cop0_cvmmemctl_reg (uint64_t val);
uint64_t octeon_phy_mem_list_init (uint64_t mem_size, uint32_t low_reserved_bytes);
uint32_t octeon_get_cop0_status (void);
void octeon_display_mem_config (void);
void octeon_led_str_write (const char *str);
int octeon_mcu_read_ddr_clock (void);
int octeon_mcu_read_cpu_ref (void);
int octeon_ebt3000_rev1 (void);
void octeon_delay_cycles (uint64_t cycles);
int octeon_boot_bus_init (void);
void octeon_write64_byte (uint64_t csr_addr, uint8_t val);
uint8_t octeon_read64_byte (uint64_t csr_addr);
void clear_tlb (int max_entry);
int get_first_core(uint32_t core_mask);

#define OCTEON_BZERO_PFS_STRIDE (8*128ULL)
#define OCTEON_BZERO_PFS_STRIDE_MASK (OCTEON_BZERO_PFS_STRIDE - 1)
void octeon_bzero64_pfs (uint64_t start_addr, uint64_t count);
int64_t validate_coremask (int64_t core_mask);
void octeon_translate_gd_to_linux_app_global_data(linux_app_global_data_t *lagd);
int octeon_setup_boot_desc_block (uint32_t core_mask,
                                  int argc, char * const argv[],
				  uint64_t entry_point, uint32_t stack_size,
				  uint32_t heap_size, uint32_t boot_flags,
				  uint64_t stack_heep_base_addr,
				  uint32_t image_flags, uint32_t config_flags,
				  uint32_t new_core_mask, int app_index);
int octeon_setup_boot_vector (uint32_t func_addr, uint32_t core_mask);
void start_cores (uint32_t coremask_to_start);
int cvmx_spi4000_initialize (int interface);
int cvmx_spi4000_detect (int interface);
void octeon_flush_l2_cache (void);
uint32_t octeon_get_available_coremask (void);
uint32_t octeon_get_available_core_count (void);
void octeon_check_mem_load_range (void);
void octeon_setup_cvmx_sysinfo (void);
int octeon_check_mem_errors (void);
int octeon_check_nor_flash_enabled (uint32_t addr);
void octeon_setup_ram_env (void);
int octeon_uart_setup2 (int uart_index, int cpu_clock_hertz, int baudrate);
int octeon_uart_setup (int uart_index);
uint64_t uboot_tlb_ptr_to_phys (void *ptr);
char *lowcase (char *str_arg);
void relocate_code_octeon (ulong, gd_t *, uint64_t, int)
    __attribute__ ((noreturn));

#define OCTEON_BREAK {asm volatile ("break");}
#define OCTEON_SYNC asm volatile ("sync" : : :"memory")
#define OCTEON_SYNCW asm volatile ("syncw" : : :"memory")

#define CAST64(v) ((long long)(long)(v))
#define CASTPTR(type, v) ((type *)(long)(v))

#include "executive-config.h"
#include "cvmx-config.h"
#include "octeon_hal.h"
#include "cvmx-bootmem.h"
#include "cvmx-twsi.h"

#define CVMX_CACHE_LINE_SIZE    (128)	/* In bytes */
#define CVMX_CACHE_LINE_MASK    (CVMX_CACHE_LINE_SIZE - 1) /* In bytes */

/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/

/* Move these out of octeon-app-init with new names, change to orig names once
** removed from octeon-app-init.h
*/
#define NUM_OCTEON_TLB_ENTRIES  128	/* Use maximum number for data
					 * structure
					 */
/** Description of TLB entry, used by bootloader */
typedef struct {
	uint64_t ri:1;
	uint64_t xi:1;
	uint64_t cca:3;
	uint64_t seg_type:3;
} tlb_flags_t;

typedef enum {
	RO_TLB_SEGMENT,
	RW_TLB_SEGMENT,
	SHARED_TLB_SEGMENT,
	STACK_HEAP_TLB_SEGMENT
} TLB_SEG_TYPE;

typedef struct {
	uint64_t phy_page0;
	uint64_t phy_page1;
	uint64_t virt_base;
	uint32_t page_size;
	tlb_flags_t flags;
} octeon_tlb_entry_t_bl;

void print_tlb_array(octeon_tlb_entry_t_bl *tlb_array, int num_entries);
void print_boot_aray(uint32_t core_mask);

#if defined(__U_BOOT__)
uint32_t mem_copy_tlb (octeon_tlb_entry_t_bl * tlb_array, uint64_t dest_virt,
		  uint64_t src_addr, uint32_t len);
#else
uint64_t mem_copy_tlb (octeon_tlb_entry_t_bl * tlb_array, uint64_t dest_virt,
		  uint64_t src_addr, uint32_t len);
#endif
void memset64 (uint64_t start_addr, uint8_t value, uint64_t len);
uint64_t memcpy64 (uint64_t dest_addr, uint64_t src_addr, uint64_t count);
void octeon_free_tmp_named_blocks (void);
int octeon_bootloader_shutdown (void);
void octeon_restore_std_mips_config (void);

/* These macros simplify the process of creating common IO addresses */
#define OCTEON_IO_SEG 2LL
#define OCTEON_ADD_SEG(segment, add)	((((uint64_t)segment) << 62) | (add))
#define OCTEON_ADD_IO_SEG(add)		OCTEON_ADD_SEG(OCTEON_IO_SEG, (add))

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/* Bootloader internal structure for per-core boot information
 * This is used in addition the boot_init_vector_t structure to store
 * information that the cores need when starting an application.
 * This structure is only used by the bootloader.
 * The boot_desc_addr is the address of the boot descriptor
 * block that is used by the application startup code.  This descriptor
 * used to contain all the information that was used by the application - now
 * it just used for argc/argv, and heap.  The remaining fields are deprecated
 * and have been moved to the cvmx_descriptor, as they are all simple exec
 * related.
 *
 * The boot_desc_addr and cvmx_desc_addr addresses are physical for linux, and
 * virtual for simple exec applications.
*/
typedef struct {
	octeon_tlb_entry_t_bl tlb_entries[NUM_OCTEON_TLB_ENTRIES];
	uint64_t entry_point;
	uint64_t boot_desc_addr;	/* address in core's final memory map */
	uint64_t stack_top;
	uint32_t exception_base;
	uint64_t cvmx_desc_addr;	/* address in core's final memory map */
	uint64_t flags;
} boot_info_block_t;
void print_boot_block(boot_info_block_t *i);

#define BOOT_INFO_FLAG_DISABLE_ICACHE_PREFETCH	(1 << 0)
#define BOOT_INFO_FLAG_LITTLE_ENDIAN		(1 << 1)

/* In addition to these two structures, there are also two more:
** 1) the boot_descriptor, which is used by the toolchain's crt0
**    code to setup the stack, head, and command line arguments
**    for the application.
** 2) the cvmx_boot_descriptor, which is used to pass additional
**    information to the simple exec.
*/

/************************************************************************/
#if defined (__U_BOOT__)

#define  OCTEON_SPINLOCK_UNLOCKED_VAL  0
typedef struct {
	volatile unsigned int value;
} cvmx_spinlock_t;
static inline void cvmx_spinlock_unlock (cvmx_spinlock_t * lock)
{
	OCTEON_SYNC;
	lock->value = 0;
	OCTEON_SYNCW;
}

static inline void cvmx_spinlock_lock (cvmx_spinlock_t * lock)
{
	unsigned int tmp;

	__asm__ __volatile__ (".set push              \n"
			      ".set noreorder         \n"
			      "1: ll   %[tmp], %[val] \n"
			      "   bnez %[tmp], 1b     \n"
			      "   li   %[tmp], 1      \n"
			      "   sc   %[tmp], %[val] \n"
			      "   beqz %[tmp], 1b     \n"
			      "   sync                \n"
			      ".set pop               \n"
			      :[val] "+m" (lock->value),
			      [tmp] "=&r" (tmp)
			      ::"memory");

}
#endif
/************************************************************************/

/* Default stack and heap sizes, in bytes */
#define DEFAULT_STACK_SIZE              (1*1024*1024)
#define DEFAULT_HEAP_SIZE               (3*1024*1024)

#define ALIGN_MASK(x)   ((~0ULL) << (x))
#define TLB_PAGE_SIZE_FROM_EXP(x)   ((1UL) << (x))
#define ALIGN_ADDR_DOWN(addr, align)   ((addr) & (align))
#define MAKE_XKPHYS(x)       ((1ULL << 63) | (x))
#define MAKE_KSEG0(x)       ((1 << 31) | (x))

/* Normalize any xkphys addresses to have only bit 63 set,
** and strip off any cache coherency information.
** Don't do anything to 32 bit compatibility addresses */
static inline uint64_t octeon_fixup_xkphys (uint64_t addr)
{
	if ((addr & (0xffffffffull << 32)) == (0xffffffffull << 32))
		return addr;

	return addr & ~(0x78ull << 56);
}

#endif /* __OCTEON_BOOT_H__ */
