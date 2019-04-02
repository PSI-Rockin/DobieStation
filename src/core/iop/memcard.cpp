#include "memcard.hpp"
#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <fstream>
#include <queue>
#include <algorithm>

Memcard::Memcard()
{
	
}

bool Memcard::is_formatted()
{
	return formatted;
}

bool Memcard::address_in_bounds(uint32_t address, uint32_t length)
{
	return address + length <= oem_memcard_bytes;
}

// Create a file on disk with all bits initialized to their default state, 1.
// The file is unusable in this state; it must be formatted by a game or BIOS.
bool Memcard::new_memcard_file(std::string file_name)
{
	std::ofstream file(file_name);

	if (!file.is_open())
	{
		printf("[MCD] (WARN) Attempted to open ofstream on empty file %s, but it either cannot be opened or could not be created! Aborting...\n", file_name);
		return false;
	}

	// Write 0xff to entire card
	std::fill_n(std::ostream_iterator<char>(file), oem_memcard_bytes, 0xff);
	file.close();

	return true;
}

// Open the memcard file_name, mounting its contents to this memcard.
bool Memcard::open(std::string file_name)
{
	std::ifstream file(file_name);

	if (!file.is_open())
	{
		printf("[MCD] (WARN) Attempted to open ifstream on memcard file %s, but it either cannot be opened or does not exist! Aborting...\n", file_name);
		return false;
	}	

	file.read(raw_data.data(), oem_memcard_bytes);

	if (file)
		printf("[MCD] (INFO) Successfully read all bytes into memory!\n");
	else
		printf("[MCD] (WARN) Failed to read all bytes into memory! Only read %d!\n", file.gcount());

	file.close();
	this->file_name = file_name;
	this->formatted = true;

	for (int i = 0; i < format_string.length(); i++) {
		if (raw_data.at(i) != format_string.at(i)) {
			this->formatted = false;
			break;
		}
	}

	// Check if backup block 2 (labelled 2 in documentation, but actually the
	// first on the card physically) is not erased. Page 16352 is the first page
	// of backup block 2.
	if (!is_erase_block_erased(16352 * bytes_per_page))
	{
		printf("[MCD] (WARN) Backup block 2 was NOT erased when this memcard was opened! The last write op on this memcard may have failed! The emulated PS2 should be restoring the old data soon, if it hasn't already...\n");
	}

	return true;
}

// Read length bytes from memcard starting at address into destination.
bool Memcard::read(std::vector<uint8_t> destination, uint32_t address, uint32_t length)
{
	if (!address_in_bounds(address, length))
	{
		printf("[MCD] (WARN) Attempted to read out-of-bounds memcard address %d! Aborting...\n", address);
		return false;
	}

	memcpy(destination.data(), &raw_data[address], length);
	return true;
}

// Write length bytes from source starting at address into memcard.
bool Memcard::write(std::vector<uint8_t> source, uint32_t address)
{
	if (!address_in_bounds(address, source.size()))
	{
		printf("[MCD] (WARN) Attempted to write out-of-bounds memcard address %d! Aborting...\n", address);
		return false;
	}

	// Check if a bitwise and of source and address bytes would give a result
	// different from the source byte. If so, that means something else
	// currently exists here, or the erase block containing address has not been
	// wiped yet. 
	for (int i = 0; i < source.size(); i++)
	{
		if (raw_data[address + i] & source[i] != source[i])
		{
			printf("[MCD] (WARN) Attempted to write to memcard address %d, but its erase block hasn't been erased yet! Aborting...\n", address + i);
			return false;
		}
	}

	// Info printf's for when an important sector is written
	if (address < 528) // Superblock
	{
		printf("[MCD] (INFO) Memcard write op on superblock (addr = %d, size = %d)\n", address, source.size());
	}
	else if (address < checksum_address + 8) // Checksum
	{
		printf("[MCD] (INFO) Memcard write op on checksum (addr = %d, size = %d)\n", address, source.size());
	}
	else if (address < 8448) // Unused space
	{
		printf("[MCD] (INFO) Memcard write op on unused card space (addr = %d, size = %d)\n", address, source.size());
	}
	else if (address < 9504) // Indirect FAT
	{
		printf("MCD] (INFO) Memcard write op on indirect FAT (addr = %d, size = %d)\n", address, source.size());
	}
	else if (address < 43296) // FAT
	{
		printf("MCD] (INFO) Memcard write op on FAT (addr = %d, size = %d)\n", address, source.size());
	}

	std::ofstream file(this->file_name);

	if (!file.is_open())
	{
		printf("[MCD] (WARN) Attempted to open ofstream on %s, but it cannot be opened! Aborting...\n", this->file_name);
		return false;
	}

	memcpy(&raw_data[address], source.data(), source.size());
	file.seekp(address);
	file.write((char*) source.data(), source.size());
	file.close();
}

// Get the start of the erase block that contains address.
uint32_t Memcard::get_erase_block_start(uint32_t address)
{
	return ((address / bytes_per_page) / erase_block_pages) * erase_block_pages * bytes_per_page;
}

// Erase the erase block that contains address
bool Memcard::erase_block(uint32_t address)
{
	std::vector<uint8_t> blank;
	blank.assign(erase_block_pages * bytes_per_page, 0xff);
	write(blank, get_erase_block_start(address));
	return true;
}

// Test if the erase block containing address has been erased.
bool Memcard::is_erase_block_erased(uint32_t address)
{
	uint32_t start_address = get_erase_block_start(address);

	for (int i = 0; i < erase_block_pages * bytes_per_page; i++)
	{
		if (raw_data[start_address + i] != 0xff)
		{
			return false;
		}
	}

	return true;
}

void Memcard::op_start(std::queue<uint8_t>& FIFO, uint8_t value)
{
	switch (mode)
	{
	case PAGE:				op_page(FIFO, value);			break;
	case PAGE_ERASE:		op_page_erase(FIFO, value);		break;
	case PAGE_WRITE:		op_page_write(FIFO, value);		break;
	case PAGE_READ:			op_page_read(FIFO, value);		break;
	case GET_TERMINATOR:	op_get_terminator(FIFO, value);	break;
	case AUTHENTICATION:	op_authentication(FIFO, value);	break;
	case PS1:				op_ps1(FIFO, value);			break;
	default:				printf("[MCD] (WARN) WTF\n");	break;
	}
}

void Memcard::op_page(std::queue<uint8_t>& FIFO, uint8_t value)
{
	uint8_t xor = 0;

	switch (value)
	{		// Value == ...
	case 2: // First byte of target address
		page_index = value;
		xor = value;
		break;
	case 3: // Second
		page_index = value << 8;
		xor ^= value;
		break;
	case 4: // Third
		page_index = value << 16;
		xor ^= value;
		break;
	case 5: // Fourth
		page_index = value << 24;
		xor ^= value;
		break;
	case 6: // XOR of bytes 1-4
		good_page = value == xor;
		break;
	// Nothing to do for 7?
	case 8:
		page_address = page_index * bytes_per_page;
		break;
	case 9:
		std::queue<uint8_t> empty;
		FIFO.swap(empty);
		switch (command)
		{
		case 0x21:
			break;
		case 0x22:
			break;
		case 0x23:
			break;
		}
	}
}

void Memcard::op_page_erase(std::queue<uint8_t>& FIFO, uint8_t value)
{
	std::queue<uint8_t> empty;

	// PCSX2 seems to be using case 0 and default to control back to back erase
	// calls. That is, each operation starts with 0x81 and is followed by other
	// bytes. Then, the next 0x81 signals the start of the next. Lets try a less
	// hacked up control here.
	switch (FIFO.size())
	{
	case 0:
		if (value != 0x81)
		{
			printf("[MCD] (INFO) Expected 0x81, got %d, flushing FIFO and waiting...\n", value);
			FIFO.swap(empty);
		}
		else
		{
			printf("[MCD] (INFO) Got 0x81, completing the rest of erase command...\n");
		}
			
		break;
	case 1:

		break;
	default:

		break;
	}
}

void Memcard::op_page_write(std::queue<uint8_t>& FIFO, uint8_t value)
{

}

void Memcard::op_page_read(std::queue<uint8_t>& FIFO, uint8_t value)
{

}

void Memcard::op_get_terminator(std::queue<uint8_t>& FIFO, uint8_t value)
{

}

void Memcard::op_authentication(std::queue<uint8_t>& FIFO, uint8_t value)
{

}

void Memcard::op_ps1(std::queue<uint8_t>& FIFO, uint8_t value)
{
	printf("[MCD] (INFO) PS1 memcard op attempted, but not implemented yet.\n");
}

// Push constant 0x2b, then whatever the memcard has as it's terminator. Seems
// to be used to finish a command sequence.
void Memcard::push_terminators(std::queue<uint8_t>& FIFO)
{
	FIFO.push(0x2b);
	FIFO.push(terminator);
}
