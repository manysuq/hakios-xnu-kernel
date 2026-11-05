/*
 * i2ctest: test harness for QUPv3 GENI I2C and touchscreen on Xiaomi Mi 8 (dipper).
 * Standalone tool linked with libmini (no iOS SDK needed).
 * Communicates with /dev/hakios-i2c.
 */

#include "mini.h"
#include "hakios.i2c.h"

static int hex_val(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static uint32_t parse_hex(const char *s)
{
	uint32_t v = 0;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
	while (*s) {
		int h = hex_val(*s++);
		if (h < 0) break;
		v = (v << 4) | (uint32_t)h;
	}
	return v;
}

static uint32_t parse_dec(const char *s)
{
	uint32_t v = 0;
	while (*s >= '0' && *s <= '9') {
		v = v * 10 + (*s++ - '0');
	}
	return v;
}

static void usage(void)
{
	printf("usage: i2ctest <command> [args...]\n");
	printf("  scan                         scan I2C bus (0x08..0x77)\n");
	printf("  status                       dump GENI / GPIO / GCC status\n");
	printf("  reset                        pulse touchscreen HW reset pin\n");
	printf("  clk <0|1|2>                  set GENI clock select index\n");
	printf("  probe [addr]                 test known touch commands on addr (default 0x49)\n");
	printf("  read <addr> <len>            read len bytes from 7-bit addr\n");
	printf("  write <addr> <b0> [b1...]    write bytes to 7-bit addr\n");
	printf("  wrrd <addr> <w0..> <rlen>    write bytes then read rlen bytes\n");
}

int main(int argc, char **argv, char **envp)
{
	(void)envp;
	if (argc < 2) {
		usage();
		return 1;
	}

	int fd = open("/dev/hakios-i2c", O_RDWR, 0);
	if (fd < 0) {
		printf("i2ctest: failed to open /dev/hakios-i2c: %s\n", strerror(errno));
		return 1;
	}

	const char *cmd = argv[1];

	if (strcmp(cmd, "scan") == 0) {
		struct hakios_i2c_scan scan;
		memset(&scan, 0, sizeof(scan));
		printf("i2ctest: scanning I2C bus...\n");
		if (ioctl(fd, HAKIOS_I2C_IOC_SCAN, &scan) < 0) {
			printf("i2ctest: scan ioctl failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}

		printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
		for (int row = 0; row < 128; row += 16) {
			printf("%02x: ", row);
			for (int col = 0; col < 16; col++) {
				int a = row + col;
				if (a < 0x08 || a > 0x77) {
					printf("   ");
				} else if (scan.found[a]) {
					printf("%02x ", a);
				} else {
					printf("-- ");
				}
			}
			printf("\n");
		}
		printf("i2ctest: found %u device(s) on bus\n", scan.count);
	} else if (strcmp(cmd, "status") == 0) {
		struct hakios_i2c_status st;
		memset(&st, 0, sizeof(st));
		if (ioctl(fd, HAKIOS_I2C_IOC_STATUS, &st) < 0) {
			printf("i2ctest: status ioctl failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		printf("i2ctest: === QUPv3 GENI I2C status ===\n");
		printf("  GENI_STATUS:     0x%08x\n", st.geni_status);
		printf("  GENI_IOS:        0x%08x\n", st.geni_ios);
		printf("  M_IRQ_STATUS:    0x%08x\n", st.m_irq_status);
		printf("  CLK_SEL:         %u\n", st.clk_sel);
		printf("  SCL_COUNTERS:    0x%08x\n", st.scl_counters);
		printf("  SER_M_CLK_CFG:   0x%08x\n", st.ser_m_clk_cfg);
		printf("  GPIO 23 (VDDIO): %s\n", st.pin_vddio ? "HIGH (ON)" : "LOW (OFF)");
		printf("  GPIO 31 (INT):   %s\n", st.pin_int ? "HIGH" : "LOW");
		printf("  GPIO 32 (RESET): %s\n", st.pin_reset ? "HIGH (RUN)" : "LOW (RESET)");
		printf("  GPIO 33 (SDA):   %s\n", st.pin_sda ? "HIGH" : "LOW");
		printf("  GPIO 34 (SCL):   %s\n", st.pin_scl ? "HIGH" : "LOW");
		printf("  GCC M_AHB CBCR:  0x%08x\n", st.gcc_m_ahb);
		printf("  GCC S_AHB CBCR:  0x%08x\n", st.gcc_s_ahb);
		printf("  GCC SE6 CBCR:    0x%08x\n", st.gcc_se6);
	} else if (strcmp(cmd, "reset") == 0) {
		printf("i2ctest: pulsing touchscreen hardware reset line...\n");
		if (ioctl(fd, HAKIOS_I2C_IOC_RESET, NULL) < 0) {
			printf("i2ctest: reset ioctl failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		printf("i2ctest: reset complete\n");
	} else if (strcmp(cmd, "clk") == 0) {
		if (argc < 3) {
			printf("i2ctest: expected clk index (0, 1, or 2)\n");
			close(fd);
			return 1;
		}
		uint32_t clk_idx = parse_dec(argv[2]);
		if (ioctl(fd, HAKIOS_I2C_IOC_SET_CLK, &clk_idx) < 0) {
			printf("i2ctest: set_clk ioctl failed: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		printf("i2ctest: clock index set to %u\n", clk_idx);
	} else if (strcmp(cmd, "read") == 0) {
		if (argc < 4) {
			printf("usage: i2ctest read <addr> <len>\n");
			close(fd);
			return 1;
		}
		uint32_t addr = parse_hex(argv[2]);
		uint32_t len = parse_dec(argv[3]);
		if (len > HAKIOS_I2C_MAX_DATA) len = HAKIOS_I2C_MAX_DATA;

		struct hakios_i2c_msg msg;
		memset(&msg, 0, sizeof(msg));
		msg.addr = (uint16_t)addr;
		msg.flags = 1; /* read */
		msg.read_len = (uint16_t)len;

		if (ioctl(fd, HAKIOS_I2C_IOC_XFER, &msg) < 0) {
			printf("i2ctest: read failed (addr 0x%02x): %s\n", addr, strerror(errno));
			close(fd);
			return 1;
		}

		printf("i2ctest: read %u bytes from 0x%02x:\n  ", msg.read_len, addr);
		for (uint16_t i = 0; i < msg.read_len; i++) {
			printf("%02x ", msg.read_buf[i]);
		}
		printf("\n");
	} else if (strcmp(cmd, "write") == 0) {
		if (argc < 4) {
			printf("usage: i2ctest write <addr> <b0> [b1...]\n");
			close(fd);
			return 1;
		}
		uint32_t addr = parse_hex(argv[2]);
		struct hakios_i2c_msg msg;
		memset(&msg, 0, sizeof(msg));
		msg.addr = (uint16_t)addr;
		msg.flags = 0; /* write */

		for (int i = 3; i < argc && msg.write_len < HAKIOS_I2C_MAX_DATA; i++) {
			msg.write_buf[msg.write_len++] = (uint8_t)parse_hex(argv[i]);
		}

		if (ioctl(fd, HAKIOS_I2C_IOC_XFER, &msg) < 0) {
			printf("i2ctest: write failed (addr 0x%02x): %s\n", addr, strerror(errno));
			close(fd);
			return 1;
		}
		printf("i2ctest: wrote %u bytes to 0x%02x: OK\n", msg.write_len, addr);
	} else if (strcmp(cmd, "wrrd") == 0) {
		if (argc < 5) {
			printf("usage: i2ctest wrrd <addr> <w0..> <rlen>\n");
			close(fd);
			return 1;
		}
		uint32_t addr = parse_hex(argv[2]);
		uint32_t rlen = parse_dec(argv[argc - 1]);
		if (rlen > HAKIOS_I2C_MAX_DATA) rlen = HAKIOS_I2C_MAX_DATA;

		struct hakios_i2c_msg msg;
		memset(&msg, 0, sizeof(msg));
		msg.addr = (uint16_t)addr;
		msg.flags = 2; /* write + read with repeated start */
		msg.read_len = (uint16_t)rlen;

		for (int i = 3; i < argc - 1 && msg.write_len < HAKIOS_I2C_MAX_DATA; i++) {
			msg.write_buf[msg.write_len++] = (uint8_t)parse_hex(argv[i]);
		}

		if (ioctl(fd, HAKIOS_I2C_IOC_XFER, &msg) < 0) {
			printf("i2ctest: wrrd failed (addr 0x%02x): %s\n", addr, strerror(errno));
			close(fd);
			return 1;
		}

		printf("i2ctest: wrrd 0x%02x wrote %u, read %u bytes:\n  ", addr, msg.write_len, msg.read_len);
		for (uint16_t i = 0; i < msg.read_len; i++) {
			printf("%02x ", msg.read_buf[i]);
		}
		printf("\n");
	} else if (strcmp(cmd, "probe") == 0) {
		uint32_t addr = 0x49;
		if (argc >= 3) addr = parse_hex(argv[2]);

		printf("i2ctest: probing touch controller at 0x%02x...\n", addr);

		/* 1. Try reading 8-byte System Info (command 0x80) */
		struct hakios_i2c_msg msg;
		memset(&msg, 0, sizeof(msg));
		msg.addr = (uint16_t)addr;
		msg.flags = 2; /* write + read */
		msg.write_len = 1;
		msg.write_buf[0] = 0x80; /* STMFTS_READ_INFO */
		msg.read_len = 8;

		if (ioctl(fd, HAKIOS_I2C_IOC_XFER, &msg) == 0) {
			printf("  [0x80 Read Info SUCCESS]: ");
			for (int i = 0; i < msg.read_len; i++) printf("%02x ", msg.read_buf[i]);
			printf("\n");
			uint16_t chip_id = ((uint16_t)msg.read_buf[6] << 8) | msg.read_buf[7];
			uint16_t fw_ver = ((uint16_t)msg.read_buf[2] << 8) | msg.read_buf[3];
			printf("  -> Chip ID: 0x%04x, FW VER: 0x%04x, Chip VER: 0x%02x, CFG ID: 0x%02x\n",
			    chip_id, fw_ver, msg.read_buf[0], msg.read_buf[4]);
		} else {
			printf("  [0x80 Read Info FAILED]: %s\n", strerror(errno));
		}

		/* 2. Try reading FIFO event (command 0x86 or 0x85) */
		memset(&msg, 0, sizeof(msg));
		msg.addr = (uint16_t)addr;
		msg.flags = 2;
		msg.write_len = 1;
		msg.write_buf[0] = 0x86; /* STMFTS_READ_ALL_EVENT */
		msg.read_len = 8;
		if (ioctl(fd, HAKIOS_I2C_IOC_XFER, &msg) == 0) {
			printf("  [0x86 Read FIFO SUCCESS]: ");
			for (int i = 0; i < msg.read_len; i++) printf("%02x ", msg.read_buf[i]);
			printf("\n");
			if (msg.read_buf[0] == 0x03) {
				printf("  -> Controller Ready Event (0x03) detected!\n");
			}
		} else {
			printf("  [0x86 Read FIFO FAILED]: %s\n", strerror(errno));
		}

		/* 3. Try HW register read (command 0xFA 0x20 0x00 0x00 0x00) */
		memset(&msg, 0, sizeof(msg));
		msg.addr = (uint16_t)addr;
		msg.flags = 2;
		msg.write_len = 5;
		msg.write_buf[0] = 0xFA;
		msg.write_buf[1] = 0x20;
		msg.write_buf[2] = 0x00;
		msg.write_buf[3] = 0x00;
		msg.write_buf[4] = 0x00;
		msg.read_len = 4;
		if (ioctl(fd, HAKIOS_I2C_IOC_XFER, &msg) == 0) {
			printf("  [0xFA HW Reg SUCCESS]: ");
			for (int i = 0; i < msg.read_len; i++) printf("%02x ", msg.read_buf[i]);
			printf("\n");
		} else {
			printf("  [0xFA HW Reg FAILED]: %s\n", strerror(errno));
		}
	} else {
		printf("i2ctest: unknown command '%s'\n", cmd);
		usage();
		close(fd);
		return 1;
	}

	close(fd);
	return 0;
}
