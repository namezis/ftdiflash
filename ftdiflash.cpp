/*
 *  ftdiflash
 *
 *  Modified version of iceprog -- simple programming tool for FTDI-based Lattice iCE programmers
 *
 *  Copyright (C) 2015 Clifford Wolf <clifford@clifford.at>
 *	 Modified 2017 Dean Miller <dean@adafruit.com>
 *       Modified 2019 Tomasz Zeman <tomasz dot zeman at gmail dot com>
 *  
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <ftdi.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ftdispi.h"

#include <limits>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <exception>

void help(const char *progname)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "ftdiflash -- simple programming tool for programming SPI flash with an FTDI\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [options] <filename>\n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "    -d <device-string>\n");
	fprintf(stderr, "        use the specified USB device: (on Linux use lsusb)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "            d:<devicenode>                (e.g. d:002/005)\n");
	fprintf(stderr, "            i:<vendor>:<product>          (e.g. i:0x0403:0x6010)\n");
	fprintf(stderr, "            i:<vendor>:<product>:<index>  (e.g. i:0x0403:0x6010:0)\n");
	fprintf(stderr, "            s:<vendor>:<product>:<serial-string>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -I [ABCD]\n");
	fprintf(stderr, "        connect to the specified interface on the FTDI chip\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -r\n");
	fprintf(stderr, "        read first 256 kB from flash and write to file\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -R <size_in_bytes>\n");
	fprintf(stderr, "        read the specified number of bytes from flash\n");
	fprintf(stderr, "        (append 'k' to the argument for size in kilobytes, or\n");
	fprintf(stderr, "        'M' for size in megabytes)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -o <offset_in_bytes>\n");
	fprintf(stderr, "        start address for read/write (instead of zero)\n");
	fprintf(stderr, "        (append 'k' to the argument for size in kilobytes, or\n");
	fprintf(stderr, "        'M' for size in megabytes)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -c\n");
	fprintf(stderr, "        do not write flash, only verify (check)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -b\n");
	fprintf(stderr, "        bulk erase entire flash before writing\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -n\n");
	fprintf(stderr, "        do not erase flash before writing\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -t\n");
	fprintf(stderr, "        just read the flash ID sequence\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -v\n");
	fprintf(stderr, "        verbose output\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Without -b or -n, ftdiflash will erase aligned chunks of 64kB in write mode.\n");
	fprintf(stderr, "This means that some data after the written data (or even before when -o is\n");
	fprintf(stderr, "used) may be erased as well.\n");
	fprintf(stderr, "\n");
	exit(1);
}

FlashConfig memory[] = 
{
    // Manufacturer   Manufacturer  Memory       Memory      Memory              Page Program  64k block Erase
    // Name           Id            Ids          Name        Size                Time          Time
    { "Winbond",      0xEF,         0x40, 0x18, "W25Q128JV", 16 * 1024 * 1024,   800,          2000 }    
};

int main(int argc, char **argv)
{
	int read_size = 256 * 1024;
	int rw_offset = 0;
	bool verbose = false;
	bool read_mode = false;
	bool check_mode = false;
	bool bulk_erase = false;
	bool dont_erase = false;
	bool prog_sram = false;
	bool test_mode = false;
	const char *inputFilename = NULL;
	const char *devstr = NULL;
	enum ftdi_interface ifnum = INTERFACE_A;

	int opt;
	char *endptr;
	while ((opt = getopt(argc, argv, "d:I:rR:o:cbnStv")) != -1)
	{
		switch (opt)
		{
		case 'd':
			devstr = optarg;
			break;
		case 'I':
			if (!strcmp(optarg, "A")) ifnum = INTERFACE_A;
			else if (!strcmp(optarg, "B")) ifnum = INTERFACE_B;
			else if (!strcmp(optarg, "C")) ifnum = INTERFACE_C;
			else if (!strcmp(optarg, "D")) ifnum = INTERFACE_D;
			else help(argv[0]);
			break;
		case 'r':
			read_mode = true;
			break;
		case 'R':
			read_mode = true;
			read_size = strtol(optarg, &endptr, 0);
			if (!strcmp(endptr, "k")) read_size *= 1024;
			if (!strcmp(endptr, "M")) read_size *= 1024 * 1024;
			break;
		case 'o':
			rw_offset = strtol(optarg, &endptr, 0);
			if (!strcmp(endptr, "k")) rw_offset *= 1024;
			if (!strcmp(endptr, "M")) rw_offset *= 1024 * 1024;
			break;
		case 'c':
			check_mode = true;
			break;
		case 'b':
			bulk_erase = true;
			break;
		case 'n':
			dont_erase = true;
			break;
		case 't':
			test_mode = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			help(argv[0]);
		}
	}

	if (read_mode + check_mode + prog_sram + test_mode > 1)
	    help(argv[0]);

	if (bulk_erase && dont_erase)
	    help(argv[0]);

	if (optind+1 != argc && !test_mode)
	{
	    if (bulk_erase && optind == argc)
	    	inputFilename = "/dev/null";
	    else
	    	help(argv[0]);
	}
	else
	{
	    inputFilename = argv[optind];
	}

	// ---------------------------------------------------------
	// Initialize USB connection to FT2232H
	// ---------------------------------------------------------

	char* fileBuffer = nullptr;

	int result = 0;
	try
	{
	    ftdispi spi;
	    std::cout << "FTDI driver version: " << spi.getVersion() << std::endl << std::flush;

	    spi.open(ifnum, devstr);
	    std::cout << "MPSSE clock: " << 
		spi.getClock() << " MHz, divisor: " <<
		spi.getDivisor() << ", SPI clock: " <<
		(double)(spi.getClock() / spi.getDivisor()) << " MHz\n";

	    usleep(250000);

	    spi.flash_power_up();

	    std::cout << "Reading flash ID... ";
	    std::list<uint8_t> id;
	    spi.flash_read_id(id);
	    std::cout <<  "Flash ID: ";
	    for (std::list<uint8_t>::iterator it = id.begin(); it != id.end(); it++)
	    {
	        std::cout << "0x" << std::setfill('0') << std::setw(2) << std::hex << (int)(*it) << " ";
	    }
	    std::cout << std::dec << std::endl << std::flush;

	    uint32_t pageProgramTime = 0;
	    uint32_t blockEraseTime64k = 0;

	    if (id.size() != 3)
		throw std::runtime_error("Unable to identify flash memory. (Wrong memory response)");

	    uint8_t flashId[3];
	    uint8_t iFlashId = 0;
	    for (std::list<uint8_t>::iterator it = id.begin(); it != id.end(); it++)
	    {
		flashId[iFlashId++] =  (uint8_t)(*it);
	    }

	    bool found = false;
	    for (auto item : memory)
	    {
		if (item.manufacturerId == flashId[0] &&
		    item.ID15_ID8 == flashId[1] &&
		    item.ID7_ID0 == flashId[2])
		{
		    found = true;
		    std::cout << "Found memory " << item.manfacturerName << ", " << item.memoryName << ", Size=" << item.size << " bytes." << std::endl << std::flush;
		    pageProgramTime = item.pageProgramTime;
		    blockEraseTime64k = item.blockEraseTime64k;
		}
	    }

	    if (found == false)
		throw std::runtime_error("Unknown flash memory.");

	    if (!test_mode)
	    {

		std::string filename(inputFilename);
		std::ifstream fileStream;

		// Open input file
		fileStream.open(filename.c_str(), std::ifstream::binary);

		if (fileStream.is_open() == false)
		{
		    throw std::runtime_error("Could not open file.");
		}

		struct stat stat_buf;
		int rc = stat(filename.c_str(), &stat_buf);
		int fileLength = (rc == 0) ? stat_buf.st_size : 0;

		std::cout << "File name: " << filename << std::endl;
		std::cout << "File size: " << fileLength << " bytes" << std::endl;
		std::cout << std::endl;

		// Allocate memory for input file
		fileBuffer = new char[fileLength];

		// Read input file
		fileStream.read(fileBuffer, fileLength);
		fileStream.close();

		// ---------------------------------------------------------
		// Program
		// ---------------------------------------------------------

		if (!read_mode && !check_mode)
		{		    
		    if (!dont_erase)
		    {
		    	if (bulk_erase)
		    	{
			    std::cout << "Chip erasing... " << std::flush;
			    spi.flash_write_enable();
			    spi.flash_bulk_erase();
			    spi.flash_wait(1000, 200000); // Probe every 1 sec for 200 seconds 
			    std::cout << "Done." << std::endl << std::flush;
			}
			else
			{			    
			    int begin_addr = rw_offset & ~0xffff;
			    int end_addr = (rw_offset + fileLength + 0xffff) & ~0xffff;

			    uint32_t prog_cent = 0; // percentage progress

			    std::cout << "Sector erasing... " << std::flush;
			    for (int addr = begin_addr; addr < end_addr; addr += 0x10000)
			    {
				spi.flash_write_enable();
				spi.flash_64kB_sector_erase(addr);
				spi.flash_wait(150, blockEraseTime64k); // Probe every 150 ms for 2000 ms 

				uint32_t new_cent = ((addr + 0x10000) * 100) / end_addr;
				new_cent = new_cent - (new_cent % 10);
				if (new_cent >= (prog_cent + 10))
				{			  
				    prog_cent = new_cent;
				    std::cout << prog_cent << "% " << std::flush;
				}
			    }
			    std::cout << "Done." << std::endl << std::flush;
			}
		    }

		    std::cout << "Checking chip status... " << std::flush;
		    spi.flash_wait(100, 1000);
		    std::cout << "Ready." << std::endl << std::flush;

		    std::cout << "Programming... " << std::flush;

		    uint32_t addr = 0;
		    uint32_t bytes_left = fileLength;
		    uint32_t buf_idx = 0;		    		    
		    uint8_t buffer[256];
		    
		    uint32_t prog_cent = 0; // percentage progress

		    while (bytes_left > 0)
		    {
			std::vector<uint8_t> bulkData;
			for (int ipages = 0; bulkData.size() < 64 * 1024 && bytes_left > 0; ipages++)
			{      
			    int page_size = 256 - (rw_offset + addr) % 256;

			    memcpy(buffer, &fileBuffer[buf_idx], page_size);
			    spi.prepare_flash_prog(bulkData, rw_offset + addr, buffer, page_size, pageProgramTime);

			    addr += page_size;
			    buf_idx += page_size;
			    bytes_left -= page_size;
			}

			spi.sendBulk(bulkData);
			spi.flash_wait(50, 100);

			uint32_t new_cent = ((fileLength - bytes_left) * 100) / fileLength;
			new_cent = new_cent - (new_cent % 10);
			if (new_cent >= (prog_cent + 10))
			{			  
			    prog_cent = new_cent;
			    std::cout << prog_cent << "% " << std::flush;
			}
		    }

		    std::cout << "Done." << std::endl << std::flush;
		}

    		// ---------------------------------------------------------
		// Read/Verify
		// ---------------------------------------------------------

		int bufferSize = 7 * 1024;

		uint32_t prog_cent = 0; // percentage progress

		if (read_mode)
		{
		    std::ofstream outputFile(inputFilename, std::ofstream::binary);
		    if (outputFile.is_open() == false)
		    {
			throw std::runtime_error("Could not open file to write flash data.");
		    }
        	    std::cout << "Reading flash... " << std::flush;
		    
		    uint8_t buffer[bufferSize];
		    for (int addr = 0; addr < read_size; addr += bufferSize)
		    {			
			uint32_t sizeToRead = bufferSize;
			if (addr + bufferSize < read_size)
			    sizeToRead = read_size - addr;

			if (verbose)
			{
			    std::cout << "Read 0x" << std::setfill('0') << std::setw(6) << std::hex << rw_offset + addr <<
				" +0x" << sizeToRead << "." << std::dec << std::endl;
			}

		    	spi.flash_read(rw_offset + addr, buffer, sizeToRead);
			outputFile.write((const char *)buffer, sizeToRead);
    		    }
		    outputFile.close();
		}
		else
		{
		    std::cout << "Verifying... " << std::flush;

		    uint8_t buffer_flash[bufferSize];
		    for (int addr = 0; addr < fileLength; addr += bufferSize)
		    {		
			uint32_t sizeToRead = bufferSize;
			if (addr + bufferSize > fileLength)
			    sizeToRead = fileLength - addr;

			if (verbose)
			{
			    std::cout << "Read 0x" << std::setfill('0') << std::setw(6) << std::hex << rw_offset + addr <<
				" +0x" << sizeToRead << "." << std::dec << std::endl;
			}

			spi.flash_read(rw_offset + addr, buffer_flash, sizeToRead);

			/*if (verbose)
			{
			    for (int i = 0; i < n; i++)
			    {
				fprintf(stderr, "%02x%c", data[i], i == n-1 || i % 32 == 31 ? '\n' : ' ');
			    }
			}*/

			if (memcmp(fileBuffer + addr, buffer_flash, sizeToRead) != 0)
			{
			    throw std::runtime_error(Formatter() << "Found difference between flash and file at address " << addr << "!");
			}

			uint32_t new_cent = ((addr + bufferSize) * 100) / fileLength;
			new_cent = new_cent - (new_cent % 10);
			if (new_cent >= (prog_cent + 10))
			{			  
			    prog_cent = new_cent;
			    std::cout << prog_cent << "% " << std::flush;
			}
		    }

		    std::cout <<  "VERIFY OK. " << std::endl;
		}
	    }
	    // ---------------------------------------------------------
	    // Reset
	    // ---------------------------------------------------------
    
	    spi.flash_power_down();

	    usleep(250000);

	    std::cout << "Done." << std::endl << std::flush;
	}
	catch (std::exception& e)
	{
	    std::cout << std::endl << "Exception: " << e.what() << std::endl;
        }
    
	return result;
}
