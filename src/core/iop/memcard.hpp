#ifndef MEMCARD_HPP
#define MEMCARD_HPP
#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <queue>

// Memcard constants and other information largely retrieved from Ross Ridge's
// writeup: https://www.ps2savetools.com/ps2memcardformat.html

// A note about the port/slot relationship:
// Ports directly map to the physical ports on the base console.
// Slots map to the ports. Without multitaps, port 0 is also slot 0, port 1 is
// also slot 1. With multitaps, port 0 on the console now has slot 0, PLUS 2, 3,
// and 4. Port 1 then has slot 1, PLUS 5, 6, and 7.

// A note about 0xff v. 0x00 for unused space:
// OEM memcards were NAND flash. One thing that may go against intuition is that
// the default state of a bit is 1, not 0. Furthermore the physical cards were
// only able to flip a bit from 1 to 0, not from 0 to 1, so in order to do the
// latter, an entire "erase block" would have to be reset to 1 bits.

// 528 bytes per page of an OEM memcard, including ECC bytes.
const uint16_t bytes_per_page = 528;

// Bytes of a 8 MB OEM memcard. 528 bytes/page, 16,384 pages = 8,388,608 bytes
const uint32_t oem_memcard_bytes = bytes_per_page * 16384;

// 28 character sequence found at the start of a memcard superblock to indicate
// it is formatted.
const std::string format_string = "Sony PS2 Memory Card Format ";

// Number of pages per erase block on an OEM memcard.
const uint8_t erase_block_pages = 16;

// Address of an 8 byte checksum. Documentation is less than helpful at
// explaining what this is or why it exists (claiming this area is unused).
// Perhaps it is a CRC?
const uint32_t checksum_address = 528;

class Memcard
{
	private:
		uint16_t FIFO_size; // Total number of reply bytes
		uint16_t data_size; // Reply bytes of just the data being sent/received
		uint32_t get_erase_block_start(uint32_t address);

		bool formatted;
		
		std::string file_name;
		std::array<char, oem_memcard_bytes> raw_data;
    public:
        Memcard();
		
		enum MODE {
			INIT,
			PAGE,
			PAGE_ERASE,
			PAGE_WRITE,
			PAGE_READ,
			GET_TERMINATOR,
			AUTHENTICATION,
			PS1
		};

		MODE mode = MODE::INIT;

		uint8_t port;
		uint8_t slot;
		uint8_t command;
		uint8_t terminator; // Container for terminator byte
		uint32_t page_index; // Container for target page of a page op
		uint32_t page_address; // Container for target address of a page op

		bool good_page;
		bool page_selected;
		bool is_formatted();
		bool address_in_bounds(uint32_t address, uint32_t length);
		bool new_memcard_file(std::string file_name);
		bool open(std::string file_name);
		bool read(std::vector<uint8_t> destination, uint32_t address, uint32_t length);
		bool write(std::vector<uint8_t> source, uint32_t address);
		bool erase_block(uint32_t address);
		bool is_erase_block_erased(uint32_t page_number);

		void op_start(std::queue<uint8_t>& FIFO, uint8_t value);
		void op_page(std::queue<uint8_t>& FIFO, uint8_t value);
		void op_page_erase(std::queue<uint8_t>& FIFO, uint8_t value);
		void op_page_write(std::queue<uint8_t>& FIFO, uint8_t value);
		void op_page_read(std::queue<uint8_t>& FIFO, uint8_t value);
		void op_get_terminator(std::queue<uint8_t>& FIFO, uint8_t value);
		void op_authentication(std::queue<uint8_t>& FIFO, uint8_t value);
		void op_ps1(std::queue<uint8_t>& FIFO, uint8_t value);
		void push_terminators(std::queue<uint8_t>& FIFO);

		// Used for the card info command. Memcard::terminator must be assigned
		// to mystery_byte manually, as this changes throughout operation.
		struct card_info {
			uint8_t		flags = 0x2b;										// flags?
			uint16_t	page_bytes_no_ecc = bytes_per_page - 16;			// Page size in bytes, EXCLUDING ECC.
			uint16_t	erase_block_pages = erase_block_pages;				// Erase block size in pages
			uint32_t	memcard_pages = oem_memcard_bytes / bytes_per_page;	// Memcard size in pages
			uint8_t		memcard_xor = 18;									// XOR? PCSX2 thinks it's of the superblock? Definitely a magic number.
			uint8_t		mystery_byte;										// PCSX2 claims it is unused/overwritten, but uses terminator byte.
		};
};

#endif // MEMCARD_HPP
