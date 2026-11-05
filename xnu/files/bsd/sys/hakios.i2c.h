/*
 * Hakios I2C test framework header.
 * Defines ioctls and structs for /dev/hakios-i2c.
 */
#ifndef _SYS_HAKIOS_I2C_H_
#define _SYS_HAKIOS_I2C_H_

#ifdef __has_include
#if __has_include(<sys/ioccom.h>)
#include <sys/ioccom.h>
#endif
#endif

#ifndef _IOWR
#define _IOC(inout, group, num, len) \
	((unsigned long)((inout) | (((len) & 0x1fff) << 16) | ((group) << 8) | (num)))
#define _IO(g, n)         _IOC(0x20000000, (g), (n), 0)
#define _IOR(g, n, t)     _IOC(0x40000000, (g), (n), sizeof(t))
#define _IOW(g, n, t)     _IOC(0x80000000, (g), (n), sizeof(t))
#define _IOWR(g, n, t)    _IOC(0xc0000000, (g), (n), sizeof(t))
#endif

#define HAKIOS_I2C_MAX_DATA 64

struct hakios_i2c_msg {
	uint16_t addr;                  /* 7-bit slave address */
	uint16_t flags;                 /* bit 0: 0=write, 1=read; bit 1: stop_stretch */
	uint16_t write_len;             /* bytes to write */
	uint16_t read_len;              /* bytes to read */
	uint8_t  write_buf[HAKIOS_I2C_MAX_DATA];
	uint8_t  read_buf[HAKIOS_I2C_MAX_DATA];
};

struct hakios_i2c_scan {
	uint8_t  found[128];            /* 1 if slave acknowledged, 0 otherwise */
	uint32_t count;
};

struct hakios_i2c_status {
	uint32_t geni_status;
	uint32_t geni_ios;
	uint32_t m_irq_status;
	uint32_t clk_sel;
	uint32_t scl_counters;
	uint32_t ser_m_clk_cfg;
	uint32_t pin_vddio;
	uint32_t pin_int;
	uint32_t pin_reset;
	uint32_t pin_sda;
	uint32_t pin_scl;
	uint32_t gcc_m_ahb;
	uint32_t gcc_s_ahb;
	uint32_t gcc_se6;
};

#define HAKIOS_I2C_IOC_XFER    _IOWR('i', 1, struct hakios_i2c_msg)
#define HAKIOS_I2C_IOC_SCAN    _IOR('i', 2, struct hakios_i2c_scan)
#define HAKIOS_I2C_IOC_RESET   _IO('i', 3)
#define HAKIOS_I2C_IOC_STATUS  _IOR('i', 4, struct hakios_i2c_status)
#define HAKIOS_I2C_IOC_SET_CLK _IOW('i', 5, uint32_t)

#endif /* _SYS_HAKIOS_I2C_H_ */
