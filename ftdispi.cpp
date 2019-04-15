#include "ftdispi.h"
#include <cstring>
#include <iostream>
#include <chrono>

ftdispi::ftdispi()
{
    m_ftdi = ftdi_new();
    if (m_ftdi == nullptr)
    {
	throw std::runtime_error("Unable to create FTDI driver instance.");
    }

    ftdi_version_info version = ftdi_get_library_version();    

    std::stringstream version_ss;
    version_ss << version.snapshot_str;
    m_version = version_ss.str();
}

ftdispi::~ftdispi()
{
    if (m_ftdi != nullptr)
    {
	if (m_ftdic_open)
	{
	    if (m_ftdic_latency_set)
	    {
		ftdi_set_latency_timer(m_ftdi, m_ftdi_latency);
	    }
	    ftdi_usb_close(m_ftdi);
	}
	ftdi_deinit(m_ftdi);
	ftdi_free(m_ftdi);
	m_ftdi = nullptr;
    }
}

std::string ftdispi::getVersion()
{
    return m_version;
}

uint32_t ftdispi::getDivisor()
{
    return m_divisor;
}

double ftdispi::getClock()
{
    return m_mpsse_clk;
}

void ftdispi::open(enum ftdi_interface ifnum, const char *devstr)
{
    int result = 0;

    ftdi_set_interface(m_ftdi, ifnum);

    if (devstr != nullptr)
    {
	if (ftdi_usb_open_string(m_ftdi, devstr))
	{
	    throw std::runtime_error(Formatter() << "Cannot find FTDI USB device (device string: " << devstr << ").");
	}
    }
    else
    {
	if (ftdi_usb_open(m_ftdi, 0x0403, 0x6010))
	{
	    throw std::runtime_error("Cannot find FTDI FT2232H USB device (vedor_id 0x0403, device_id 0x6010).");
	}
    }

    if (m_ftdi->type != TYPE_2232H && m_ftdi->type != TYPE_4232H)
    {
	throw std::runtime_error(Formatter() << "FTDI chip type " << m_ftdi->type << " is not high-speed.");
    }

    m_ftdic_open = true;

    if (ftdi_usb_reset(m_ftdi))
    {
	throw std::runtime_error(Formatter() << "Failed to reset FTDI USB device.");
    }

    usleep(100000);

    result = ftdi_set_bitmode(m_ftdi, 0, BITMODE_RESET);
    if (result < 0)
    {
	throw std::runtime_error(Formatter() << "Failed set BITMODE_MPSSE on FTDI USB device. Error: " << 
	    result << ", " << ftdi_get_error_string(m_ftdi));
    }    

    result = ftdi_set_bitmode(m_ftdi, 0xff, BITMODE_MPSSE);
    if (result < 0)
    {
	throw std::runtime_error(Formatter() << "Failed set BITMODE_MPSSE on FTDI USB device. Error: " << 
	    result << ", " << ftdi_get_error_string(m_ftdi));
    }


/* 
   The following code is commented because at the time of writing Ubuntu has still distributed headers with the 
   wrong implementation to flush the buffers. Refer to libftdi source for more details. Uncomment when needed.
*/

/*
    result = ftdi_tciflush(m_ftdi);
    if (result < 0)
    {
	throw std::runtime_error(Formatter() << "Failed to purge read buffers on FTDI USB device. Error: " << 
	    result << ", " << ftdi_get_error_string(m_ftdi));
    }

    result = ftdi_tcoflush(m_ftdi);
    if (result < 0)
    {
	throw std::runtime_error(Formatter() << "Failed to purge write buffers on FTDI USB device. Error: " << 
	    result << ", " << ftdi_get_error_string(m_ftdi));
    }
*/

    result = ftdi_get_latency_timer(m_ftdi, &m_ftdi_latency);
    if (result < 0)
    {
	throw std::runtime_error(Formatter() << "Failed to get latency timer. Error: " << 
	    result << ", " << ftdi_get_error_string(m_ftdi));
    }

    result = ftdi_set_latency_timer(m_ftdi, 16);
    if (result < 0)
    {
	throw std::runtime_error(Formatter() << "Failed to set latency timer. Error: " << 
	    result << ", " << ftdi_get_error_string(m_ftdi));
    }

    m_ftdic_latency_set = true;

    /* The following two lines seems to be very sensitive. Needs more investigation. */
    set_read_chunksize(8 * 1024);
    set_write_chunksize(8 * 1024);

    /*
     * The 'H' chips can run with an internal clock of either 12 MHz or 60 MHz,
     * but the non-H chips can only run at 12 MHz. We enable the divide-by-5
     * prescaler on the former to run on the same speed.
     */
    uint8_t clock_5x = 1;
    /* In addition to the prescaler mentioned above there is also another
     * configurable one on all versions of the chips. Its divisor div can be
     * set by a 16 bit value x according to the following formula:
     * div = (1 + x) * 2 <-> x = div / 2 - 1
     * Hence the expressible divisors are all even numbers between 2 and
     * 2^17 (=131072) resulting in SCK frequencies of 6 MHz down to about
     * 92 Hz for 12 MHz inputs.
     */
    
    uint8_t divider;    
    if (clock_5x)
    {
	divider = DIS_DIV_5; // Disable divide-by-5. DIS_DIV_5 in newer libftdi
	m_mpsse_clk = 60.0;
    }
    else
    {
    	divider = EN_DIV_5; // Enable clock divide by 5
    	m_mpsse_clk = 12.0;
    }

    uint8_t ftdi_init[] = {
	// Enable clock divide by 5
	divider,
	// Set clock
	TCK_DIVISOR, (uint8_t)((m_divisor / 2 - 1) & 0xff), (uint8_t)(((m_divisor / 2 - 1) >> 8) & 0xff),
	// Disconnect TDI/DO to TDO/DI for loopback
	LOOPBACK_END,
	// Set data bits - deselect CS
	SET_BITS_LOW, m_cs_bits, m_pindir,
	// Chip select test - use this for measurment
	CHIP_SELECT,
	CHIP_DESELECT
    };

    write(ftdi_init, sizeof(ftdi_init), "Device init");

    usleep(100000);
}

void ftdispi::flash_read_id(std::list<uint8_t> &id)
{
    uint8_t ftdi_data_in[] = {
        CHIP_SELECT,
	DATA_OUT_IN(4),
	0x9F,
	0,
	0,
	0,
	CHIP_DESELECT
    };

    write(ftdi_data_in, sizeof(ftdi_data_in), "Flash Read Id (w)");

    uint8_t ftdi_data_out[4] = { };

    read(ftdi_data_out, sizeof(ftdi_data_out), "Flash Read Id (r)");

    for (int i = 1; i < (int)sizeof(ftdi_data_out); i++)
    {
    	id.push_back(ftdi_data_out[i]);
    }
}

void ftdispi::flash_power_up()
{
    uint8_t ftdi_data[] = {
    	CHIP_SELECT,
	DATA_OUT(1),
	0xAB,
	CHIP_DESELECT
    };

    write(ftdi_data, sizeof(ftdi_data), "Flash power up");
}

void ftdispi::flash_power_down()
{
    uint8_t ftdi_data[] = {
    	CHIP_SELECT,
	DATA_OUT(1),
	0xB9,
	CHIP_DESELECT		
    };

    write(ftdi_data, sizeof(ftdi_data), "Flash power down");
}

void ftdispi::flash_write_enable()
{
    uint8_t ftdi_data[] = {
	CHIP_SELECT,
	DATA_OUT(1),
	0x06,
	CHIP_DESELECT
    };

    write(ftdi_data, sizeof(ftdi_data), "Flash write enable");
}

void ftdispi::flash_bulk_erase()
{
    uint8_t ftdi_data[] = {
    	CHIP_SELECT,
	DATA_OUT(1),
	0xC7,
	CHIP_DESELECT
    };

    write(ftdi_data, sizeof(ftdi_data), "Bulk erase");
}

void ftdispi::flash_64kB_sector_erase(int addr)
{
    uint8_t ftdi_data[] = {
	CHIP_SELECT,
	DATA_OUT(4),
	0xD8,
	(uint8_t)(addr >> 16),
	(uint8_t)(addr >> 8),
	(uint8_t)addr,
	CHIP_DESELECT		
    };

    write(ftdi_data, sizeof(ftdi_data), "Erase 64kB sector");
}

void ftdispi::flash_wait(int timeout, int duration)
{
    uint8_t ftdi_data_in[] = {
        CHIP_SELECT,
        DATA_OUT_IN(2),
        0x05,
        0,
        CHIP_DESELECT,
    };

    uint8_t ftdi_data_out[2] = { };

    auto begin = std::chrono::high_resolution_clock::now();

    while (1)
    {
	write(ftdi_data_in, sizeof(ftdi_data_in), "Wait");
    	
	read(ftdi_data_out, sizeof(ftdi_data_out), "Wait");

	if ((ftdi_data_out[1] & 0x01) == 0)
	    break;

	usleep(timeout * 1000);
	
	auto end = std::chrono::high_resolution_clock::now();
	auto dur = end - begin;
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
	if (ms > duration)
	{
	    throw std::runtime_error(Formatter() << "Waiting too long for flash memory to be ready.");
	}
    }
}

void ftdispi::flash_prog(int addr, uint8_t *page, int n)
{
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

    std::memcpy(&ftdi_data[0], &ftdi_data_begin[0], sizeof(ftdi_data_begin));
    std::memcpy(&ftdi_data[sizeof(ftdi_data_begin)], page, n);
    std::memcpy(&ftdi_data[sizeof(ftdi_data_begin) + n], &ftdi_data_end[0], sizeof(ftdi_data_end));

    write(ftdi_data, sizeof(ftdi_data), "Flash prog");
}

void ftdispi::sendBulk(std::vector<uint8_t> &data)
{
    write(data.data(), data.size(), "Send bulk data");
}

void ftdispi::prepare_flash_prog(std::vector<uint8_t> &data, int addr, uint8_t *page, int n, uint32_t pageProgramTime)
{
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

    std::memcpy(&ftdi_data[0], &ftdi_data_begin[0], sizeof(ftdi_data_begin));
    std::memcpy(&ftdi_data[sizeof(ftdi_data_begin)], page, n);
    std::memcpy(&ftdi_data[sizeof(ftdi_data_begin) + n], &ftdi_data_end[0], sizeof(ftdi_data_end));

    data.insert(data.end(), ftdi_data, ftdi_data + sizeof(ftdi_data));

    int pageProgramWaitUs = pageProgramTime;
    
    double spiBusClockHz = (getClock() / getDivisor()) * 1000000;
    double spiByteDurationUs = ((1 / spiBusClockHz) * 1000000) * 8;
    
    // Calculate number of max milliseconds to wait after programming page
    int waitMaxCount = pageProgramWaitUs / ((spiByteDurationUs < 1) ? 1 : spiByteDurationUs);

    uint8_t ftdi_data_wait[] = {
	WAIT_8_BITS(waitMaxCount)
    };

    data.insert(data.end(), ftdi_data_wait, ftdi_data_wait + sizeof(ftdi_data_wait));    
}

void ftdispi::flash_read(int addr, uint8_t *data, int n)
{
    uint8_t ftdi_data_begin[] = {
	CHIP_SELECT,
	DATA_OUT_IN(4 + n),
	0x03,
	(uint8_t)(addr >> 16),
	(uint8_t)(addr >> 8),
	(uint8_t)addr,
    };

    uint8_t ftdi_data_end[] = {
	CHIP_DESELECT
    };

    uint8_t ftdi_data[sizeof(ftdi_data_begin) + n + sizeof(ftdi_data_end)];

    std::memcpy(&ftdi_data[0], &ftdi_data_begin[0], sizeof(ftdi_data_begin));
    std::memset(&ftdi_data[sizeof(ftdi_data_begin)], 0xFF, n);
    std::memcpy(&ftdi_data[sizeof(ftdi_data_begin) + n], &ftdi_data_end[0], sizeof(ftdi_data_end));

    write(ftdi_data, sizeof(ftdi_data), "Flash read (w)");

    std::memset(&ftdi_data[0], 0, sizeof(ftdi_data));

    read(ftdi_data, n + 4, "Flash read (r)");
    std::memcpy(data, &ftdi_data[4], n);
}
