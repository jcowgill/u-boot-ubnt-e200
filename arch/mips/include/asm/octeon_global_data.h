#ifndef __OCTEON_GLOBAL_DATA_H__
#define __OCTEON_GLOBAL_DATA_H__

#ifndef __U_BOOT__
# define k0 $26
# include "asm/u-boot.h"
#endif

#ifdef __U_BOOT__
# include <asm/arch/octeon_eeprom_types.h>
#else
# include <octeon_eeprom_types.h>
# define u32 uint32_t
# define u64 uint64_t
#endif

#define SERIAL_LEN              20
#define GD_TMP_STR_SIZE         30

/**
 * These values are used in the bd->bi_bootflags parameter
 */
#define OCTEON_BD_FLAG_BOOT_RAM		1
#define OCTEON_BD_FLAG_BOOT_CACHE	2
#define OCTEON_BD_FLAG_BOOT_FLASH	4

typedef struct global_data_s {
       /* EEPROM data structures as read from EEPROM or populated by other
        * means on boards without an EEPROM.
        */
        octeon_eeprom_board_desc_t board_desc;
        octeon_eeprom_clock_desc_t clock_desc;
        octeon_eeprom_mac_addr_t   mac_desc;

        uint32_t	cpu_clock_mhz;  /* CPU clock speed in MHz */
        uint32_t	console_uart;	/* Console UART number */
        uint32_t	ddr_clock_mhz;  /* DDR clock (not data rate) in MHz */
        uint32_t	ddr_ref_hertz;  /* DDR reference clock in hz */
        uint64_t	dfm_ram_size;   /* DFM ram size */
        char		*err_msg;       /* Save error msg until console up */
        /* Temporary string used in ddr init code before DRAM is up */
	ulong		uboot_flash_address;    /* Address of normal
                                                 * bootloader in flash
                                                 */
	ulong		uboot_flash_size;   /* Size of normal bootloader */
	int		mcu_rev_maj;
	int		mcu_rev_min;
        char		tmp_str[GD_TMP_STR_SIZE];
	uint32_t	current_i2c_bus; 	/* Current I2C bus number */
	uint32_t	current_i2c_mux_bus;	/* Current I2C mux setting */
	uint32_t	flash_base_address;	/* Base address of flash */
	uint32_t	flash_size;		/* Size of flash */
	/* These really belong in gd->bd though unfortunately it isn't
	 * initialized until later.
	 */
	uint8_t		tlv_addr;		/* TWSI address of TLV EEPROM */
# ifdef CONFIG_OCTEON_ENABLE_PAL
	uint32_t	pal_addr;		/* Address of PAL on boot bus */
# endif
# ifdef CONFIG_OCTEON_ENABLE_LED_DISPLAY
	uint32_t	led_addr;		/* LED display address */
# endif

} octeon_global_data_t;

/*
 * This is used for compatibility and is passed to the Linux kernel and Octeon
 * simple executive applications since the definition of gd_t has changed with
 * more recent versions of U-Boot.
 */
typedef struct octeon_boot_gd_s {
	bd_t                *bd;
	unsigned long       flags;
	unsigned long       baudrate;
	unsigned long       have_console;   /* serial_init() was called */
	uint64_t            ram_size;       /* RAM size */
	uint64_t            reloc_off;      /* Relocation Offset */
	unsigned long       env_addr;       /* Address  of Environment struct */
	unsigned long       env_valid;      /* Checksum of Environment valid? */
	unsigned long       cpu_clock_mhz;  /* CPU clock speed in Mhz */
	unsigned long       ddr_clock_mhz;  /* DDR clock (not data rate!) in Mhz */
	unsigned long       ddr_ref_hertz;   /* DDR Ref clock Hertz */
	int                 mcu_rev_maj;
	int                 mcu_rev_min;
	int                 console_uart;

	/* EEPROM data structures as read from EEPROM or populated by other
	 * means on boards without an EEPROM
	 */
	octeon_eeprom_board_desc_t board_desc;
	octeon_eeprom_clock_desc_t clock_desc;
	octeon_eeprom_mac_addr_t   mac_desc;

	void                **jt;               /* jump table */
	char                *err_msg;           /* pointer to error message to
                                                 * save until console is up
                                                 */
	char                tmp_str[GD_TMP_STR_SIZE];  /* Temporary string used
                                                        * in ddr init code
                                                        * before DRAM is up
                                                        */
	unsigned long       uboot_flash_address;    /* Address of normal
                                                     * bootloader in flash
                                                     */
	unsigned long       uboot_flash_size;   /* Size of normal bootloader */
	uint64_t            dfm_ram_size;       /* DFM RAM size */
} octeon_boot_gd_t;

#endif /* __OCTEON_GLOBAL_DATA_H__ */
