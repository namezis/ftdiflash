#ifndef FTDI_SPI_H
#define FTDI_SPI_H

#include <stdexcept>
#include <cstdint>
#include <string>
#include <vector>
#include <list>

#include <ftdi.h>
#include <unistd.h>

#include "utils.h"

#define DEFAULT_DIVISOR 18

#define DATA_OUT(n) 0x11, \
		    (uint8_t)(n-1), \
		    (uint8_t)((n-1) >> 8)

#define DATA_OUT_IN(n) 0x31, \
		    (uint8_t)(n-1), \
		    (uint8_t)((n-1) >> 8)

#define WAIT_8_BITS(n) 0x8F, \
		    (uint8_t)(n-1), \
		    (uint8_t)((n-1) >> 8)

#define CHIP_SELECT SET_BITS_LOW, \
		    0 & ~m_cs_bits, \
		    m_pindir

#define CHIP_DESELECT SET_BITS_LOW, \
		    m_cs_bits, \
		    m_pindir

class ftdispi {

private:
    struct ftdi_context *m_ftdi = nullptr;
    bool m_ftdic_open = false;

    // Driver version
    std::string m_version = "";

    bool m_ftdic_latency_set = false;
    uint8_t m_ftdi_latency = 16;

    // Clock 
    uint32_t m_divisor = DEFAULT_DIVISOR;
    double m_mpsse_clk = 0;

    /* The variables cs_bits and pindir store the values for the "set data bits low byte" MPSSE command that
     * sets the initial state and the direction of the I/O pins. The pin offsets are as follows:
     * SCK is bit 0.
     * DO  is bit 1.
     * DI  is bit 2.
     * CS  is bit 3.
     *
     * The default values (set below) are used for most devices:
     *  value: 0x08  CS=high, DI=low, DO=low, SK=low
     *    dir: 0x0b  CS=output, DI=input, DO=output, SK=output
     */
    uint8_t m_cs_bits = 0x08;
    uint8_t m_pindir = 0x0b;
    
public:
    ftdispi();

    virtual ~ftdispi();

    std::string getVersion();
    uint32_t getDivisor();
    double getClock();

    void open(enum ftdi_interface ifnum, const char *devstr);

    void flash_read_id(std::list<uint8_t> &id);
    void flash_power_up();
    void flash_power_down();
    void flash_write_enable();
    void flash_bulk_erase();
    void flash_64kB_sector_erase(int addr);

    void flash_wait(int timeout, int duration);
    void flash_prog(int addr, uint8_t *page, int n);

    void sendBulk(std::vector<uint8_t> &data);
    void prepare_flash_prog(std::vector<uint8_t> &data, int addr, uint8_t *page, int n, uint32_t pageProgramTime);

    void flash_read(int addr, uint8_t *data, int n);

public:
    inline void write(uint8_t *data, size_t size, std::string operation_name)
    {
	int result = ftdi_write_data(m_ftdi, data, size);
	if (result != (int)size)
	{
    	    if (result > 0)
    	    {
		throw std::runtime_error(Formatter() << operation_name << ". Written " <<
		    result << ", but expected " << (int)size << ".");
	    }
	    else if (result == -666)
	    {
		throw std::runtime_error(Formatter() << operation_name << ". USB device not connected.");
	    }
	    else
	    {
		throw std::runtime_error(Formatter() << operation_name << ". Error " <<
		    result << ", " << ftdi_get_error_string(m_ftdi));
	    }
	}
    }

    inline void read(uint8_t *data, size_t size, std::string operation_name)
    {
	uint8_t *p_data = data;
	int bytesToRead = size;
	while (bytesToRead > 0)
	{
    	    int result = ftdi_read_data(m_ftdi, p_data, bytesToRead);
	    if (result < 0)
	    {
		if (result == -666)
		{
		    throw std::runtime_error(Formatter() << operation_name << ". USB device not connected.");
		}
		else
		{
		    throw std::runtime_error(Formatter() << operation_name << ". Error " <<
			result << ", " << ftdi_get_error_string(m_ftdi));
		}
	    }
	    bytesToRead -= result;
	    p_data += result;
	    usleep(100);
    	}
    }

    inline void set_read_chunksize(uint32_t size)
    {
	int result = ftdi_read_data_set_chunksize(m_ftdi, size);
	if (result < 0)
	{
	    throw std::runtime_error(Formatter() << "Unable to set read chunk size. Error: " << 
		result << ", " << ftdi_get_error_string(m_ftdi));
	}
    }

    inline void set_write_chunksize(uint32_t size)
    {
	int result = ftdi_write_data_set_chunksize(m_ftdi, size);
	if (result < 0)
	{
	    throw std::runtime_error(Formatter() << "Unable to set write chunk size. Error: " << 
		result << ", " << ftdi_get_error_string(m_ftdi));
	}
    }
};

#endif // FTDI_SPI_H
