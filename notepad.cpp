/*
unsigned char ftdi_latency;

void check_rx()
{
	while (1)
	{
		uint8_t data;
		int rc = ftdi_read_data(&ftdic, &data, 1);
		if (rc <= 0)
		    break;
		fprintf(stderr, "unexpected rx byte: %02X\n", data);
	}
}

void error()
{
	check_rx();
	fprintf(stderr, "ABORT.\n");
	if (ftdic_open)
	{
		if (ftdic_latency_set)
		{
			ftdi_set_latency_timer(&ftdic, ftdi_latency);
		}
		ftdi_usb_close(&ftdic);
	}
	ftdi_deinit(&ftdic);
	exit(1);
}

uint8_t recv_byte()
{
	uint8_t data;
	while (1)
	{
		int rc = ftdi_read_data(&ftdic, &data, 1);
		if (rc < 0)
		{
			fprintf(stderr, "Read error.\n");
			error();
		}
		if (rc == 1)
			break;
		usleep(100);
	}
	return data;
}

void send_byte(uint8_t data)
{
	int rc = ftdi_write_data(&ftdic, &data, 1);
	if (rc != 1)
	{
		fprintf(stderr, "Write error (single byte, rc=%d, expected %d).\n", rc, 1);
		error();
	}
}

void send_spi(uint8_t *data, int n)
{
	if (n < 1)
		return;

	send_byte(0x11);
	send_byte(n-1);
	send_byte((n-1) >> 8);

	int rc = ftdi_write_data(&ftdic, data, n);
	if (rc != n)
	{
		fprintf(stderr, "Write error (chunk, rc=%d, expected %d).\n", rc, n);
		error();
	}
}

void xfer_spi(uint8_t *data, int n)
{
	if (n < 1)
		return;

	send_byte(0x31);
	send_byte(n-1);
	send_byte((n-1) >> 8);

	int rc = ftdi_write_data(&ftdic, data, n);
	if (rc != n)
	{
		fprintf(stderr, "Write error (chunk, rc=%d, expected %d).\n", rc, n);
		error();
	}

	for (int i = 0; i < n; i++)
	{
		data[i] = recv_byte();
	}
}

void chip_select()
{
	send_byte(SET_BITS_LOW);
	send_byte(0 & ~cs_bits); // assertive
	send_byte(pindir);
}

void chip_deselect()
{
	send_byte(SET_BITS_LOW);
	send_byte(cs_bits);
	send_byte(pindir);
}*/
/*
void flash_prog(int addr, uint8_t *data, int n)
{
	if (verbose)
	{
		fprintf(stderr, "Prog 0x%06X +0x%03X..\n", addr, n);
	}

	uint8_t ftdi_data_begin[] = {
		CHIP_SELECT,
		DATA_OUT(1),
		0x06,
		CHIP_DESELECT,
		CHIP_SELECT,
		DATA_OUT(4 + n),
		0x02,
		(uint8_t)(addr >> 16),
		(uint8_t)(addr >> 8),
		(uint8_t)addr,
	};
	uint8_t ftdi_data_end[] = {
		CHIP_DESELECT
	};
	
	uint8_t ftdi_data[sizeof(ftdi_data_begin) + n + sizeof(ftdi_data_end)];

	memcpy(&ftdi_data[0], &ftdi_data_begin[0], sizeof(ftdi_data_begin));
	memcpy(&ftdi_data[sizeof(ftdi_data_begin)], data, n);
	memcpy(&ftdi_data[sizeof(ftdi_data_begin) + n], &ftdi_data_end[0], sizeof(ftdi_data_end));

	int retval = ftdi_write_data(&ftdic, ftdi_data, sizeof(ftdi_data));
        if (retval != (int)sizeof(ftdi_data))
	{
		if (retval > 0)
		{
            		fprintf(stderr, "Flash prog failed. Written %d, but expected %d.\n",
				retval, (int)sizeof(ftdi_data));
		}
		else if (retval == -666)
		{
			fprintf(stderr, "Flash prog failed. USB device not connected.\n");
		}
		else
		{
			fprintf(stderr, "Flash prog failed. Error: %d, %s\n",
				retval, ftdi_get_error_string(&ftdic));
		}
		error();
        }

	if (verbose)
	{
		for (int i = 0; i < n; i++)
		{
			fprintf(stderr, "%02x%c", data[i], i == n-1 || i % 32 == 31 ? '\n' : ' ');
		}
	}
}
*/
void flash_read(int addr, uint8_t *data, int n)
{
	if (verbose)
		fprintf(stderr, "read 0x%06X +0x%03X..\n", addr, n);

	uint8_t command[4] = { 0x03, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };
	chip_select();
	send_spi(command, 4);
	memset(data, 0, n);
	xfer_spi(data, n);
	chip_deselect();

	if (verbose)
	{
		for (int i = 0; i < n; i++)
		{
			fprintf(stderr, "%02x%c", data[i], i == n-1 || i % 32 == 31 ? '\n' : ' ');
		}
	}
}

/*********************************************/

/*
	ftdi_init(&ftdic);
	ftdi_set_interface(&ftdic, ifnum);

	if (devstr != NULL)
	{
		if (ftdi_usb_open_string(&ftdic, devstr))
		{
			fprintf(stderr, "Can't find FTDI USB device (device string %s).\n", devstr);
			error();
		}
	}
	else
	{
		if (ftdi_usb_open(&ftdic, 0x0403, 0x6010)) // Dual RS232 without eeprom
		{
			fprintf(stderr, "Can't find FTDI FT2232H USB device (vedor_id 0x0403, device_id 0x6010).\n");
			error();
		}
	}

	if (ftdic.type != TYPE_2232H && ftdic.type != TYPE_4232H && ftdic.type != TYPE_232H)
	{
		fprintf(stderr,"FTDI chip type %d is not high-speed.\n", ftdic.type);
		error();
	}

	ftdic_open = true;

	if (ftdi_usb_reset(&ftdic))
	{
		fprintf(stderr, "Failed to reset FTDI USB device.\n");
		error();
	}

	usleep(100000);

	err = ftdi_set_bitmode(&ftdic, 0, BITMODE_RESET);
	if (err < 0)
	{
		fprintf(stderr, "Failed set BITMODE_MPSSE on FTDI USB device. Error: %d, %s\n",
			err, ftdi_get_error_string(&ftdic));
		error();
	}    

	err = ftdi_set_bitmode(&ftdic, 0xff, BITMODE_MPSSE);
	if (err < 0)
	{
		fprintf(stderr, "Failed set BITMODE_MPSSE on FTDI USB device. Error: %d, %s\n",
			err, ftdi_get_error_string(&ftdic));
		error();
	}

	err = ftdi_usb_purge_buffers(&ftdic);
	if (err < 0)
	{
		fprintf(stderr, "Failed to purge buffers on FTDI USB device. Error: %d, %s\n",
			err, ftdi_get_error_string(&ftdic));
		error();
	}

	err = ftdi_get_latency_timer(&ftdic, &ftdi_latency);
	if (err < 0)
	{
		fprintf(stderr, "Failed to get latency timer. Error: %d, %s\n",
			err, ftdi_get_error_string(&ftdic));
		error();
	}

	// 1 is the fastest polling, it means 1 kHz polling
	err = ftdi_set_latency_timer(&ftdic, 16);
	if (err < 0)
	{
		fprintf(stderr, "Failed to set latency timer. Error: %d, %s.\n", 
			err, ftdi_get_error_string(&ftdic));
		error();
	}

	ftdic_latency_set = true;

	err = ftdi_write_data_set_chunksize(&ftdic, 256 + 13 + 10);
	if (err < 0)
	{
		fprintf(stderr, "Unable to set chunk size. Error: %d, %s.\n",
			err, ftdi_get_error_string(&ftdic));
		error();
	}

	if (clock_5x)
	{
		if (verbose)
			fprintf(stderr, "Disable divide-by-5 front stage.\n");
		send_byte(DIS_DIV_5); // Disable divide-by-5. DIS_DIV_5 in newer libftdi
		mpsse_clk = 60.0;
	}
	else
	{
		// enable clock divide by 5
		send_byte(EN_DIV_5);
		mpsse_clk = 12.0;
	}

	uint8_t ftdi_init[] = {
		// Set clock
		TCK_DIVISOR, (uint8_t)((divisor / 2 - 1) & 0xff), (uint8_t)(((divisor / 2 - 1) >> 8) & 0xff),
		// Disconnect TDI/DO to TDO/DI for loopback
		LOOPBACK_END,
		// Set data bits
		SET_BITS_LOW, cs_bits, pindir
	};

	int retval = ftdi_write_data(&ftdic, ftdi_init, sizeof(ftdi_init));
        if (retval != sizeof(ftdi_init))
	{
		if (retval > 0)	    
		{
            		fprintf(stderr, "Device init failed. Written %d, but expected %d.\n",
				retval, (int)sizeof(ftdi_init));
		}
		else if (retval == -666)
		{
			fprintf(stderr, "Device init failed. USB device not connected.\n");
		}
		else
		{
			fprintf(stderr, "Device init failed. Error: %d, %s\n",
				retval, ftdi_get_error_string(&ftdic));
		}
		error();
        }
		
	fprintf(stderr, "MPSSE clock: %f MHz, divisor: %u, SPI clock: %f MHz\n",
		 mpsse_clk, divisor, (double)(mpsse_clk / divisor));

	usleep(100000);

	chip_deselect();

*/

/*	if (test_mode)
	{
		fprintf(stderr, "Reset..\n");

		usleep(250000);

		flash_power_up();

		flash_read_id();

		flash_power_down();

		usleep(250000);
	}
	else
	{
		// ---------------------------------------------------------
		// Reset
		// ---------------------------------------------------------

		fprintf(stderr, "Reset..\n");

		usleep(250000);

		flash_power_up();

		flash_read_id();

		// ---------------------------------------------------------
		// Program
		// ---------------------------------------------------------
*/
/*		if (!read_mode && !check_mode)
		{
			FILE *f = (strcmp(filename, "-") == 0) ? stdin :
				fopen(filename, "rb");
			if (f == NULL)
			{
				fprintf(stderr, "Error: Can't open '%s' for reading: %s\n", filename, strerror(errno));
				error();
			}

			if (!dont_erase)
			{
				if (bulk_erase)
				{
					flash_write_enable();
					flash_bulk_erase();
					flash_wait(1 * 1000 * 1000, true);
				}
				else
				{
					struct stat st_buf;
					if (stat(filename, &st_buf))
					{
						fprintf(stderr, "Error: Can't stat '%s': %s\n", filename, strerror(errno));
						error();
					}

					fprintf(stderr, "file size: %d\n", (int)st_buf.st_size);

					int begin_addr = rw_offset & ~0xffff;
					int end_addr = (rw_offset + (int)st_buf.st_size + 0xffff) & ~0xffff;

					for (int addr = begin_addr; addr < end_addr; addr += 0x10000)
					{
						flash_write_enable();
						flash_64kB_sector_erase(addr);
						flash_wait(1 * 1000 * 1000, true);
					}
				}
			}

			fprintf(stderr, "Programming...");

			int page_counter = 0;

			for (int rc, addr = 0; true; addr += rc)
			{
				uint8_t buffer[256];
				int page_size = 256 - (rw_offset + addr) % 256;

				rc = fread(buffer, 1, page_size, f);
				if (rc <= 0) break;
				
				flash_prog(rw_offset + addr, buffer, rc);
				flash_wait(50, false);

				page_counter++;
				if (page_counter >= 255)
				{
					fprintf(stderr, ".");
					page_counter = 0;
				}
			}
			
			fprintf(stderr, "\n");    

			if (f != stdin)
				fclose(f);
		}*/


		// ---------------------------------------------------------
		// Read/Verify
		// ---------------------------------------------------------

		if (read_mode)
		{
			fprintf(stderr, "Verifying..\n");

			FILE *f = (strcmp(filename, "-") == 0) ? stdout :
				fopen(filename, "wb");
			if (f == NULL)
			{
				fprintf(stderr, "Error: Can't open '%s' for writing: %s\n", filename, strerror(errno));
				error();
			}

			fprintf(stderr, "reading..\n");
			for (int addr = 0; addr < read_size; addr += 256)
			{
				uint8_t buffer[256];
				flash_read(rw_offset + addr, buffer, 256);
				fwrite(buffer, 256, 1, f);
			}

			if (f != stdout)
				fclose(f);
		}
		else
		{
			FILE *f = (strcmp(filename, "-") == 0) ? stdin :
				fopen(filename, "rb");
			if (f == NULL) {
				fprintf(stderr, "Error: Can't open '%s' for reading: %s\n", filename, strerror(errno));
				error();
			}

			fprintf(stderr, "reading..\n");
			for (int addr = 0; true; addr += 256)
			{
				uint8_t buffer_flash[256], buffer_file[256];
				int rc = fread(buffer_file, 1, 256, f);
				if (rc <= 0) break;
				flash_read(rw_offset + addr, buffer_flash, rc);
				if (memcmp(buffer_file, buffer_flash, rc))
				{
					fprintf(stderr, "Found difference between flash and file!\n");
					error();
				}
			}

			fprintf(stderr, "VERIFY OK\n");

			if (f != stdin)
				fclose(f);
		}

		// ---------------------------------------------------------
		// Reset
		// ---------------------------------------------------------

		flash_power_down();
		usleep(250000);
	}

	// ---------------------------------------------------------
	// Exit
	// ---------------------------------------------------------
/*
	fprintf(stderr, "Done.\n");
	ftdi_set_latency_timer(&ftdic, ftdi_latency);
	ftdi_disable_bitbang(&ftdic);
	ftdi_usb_close(&ftdic);
	ftdi_deinit(&ftdic);

*/
