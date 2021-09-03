#include <iostream>
#include <fstream>

#include "xpum_config.h"
#include "PciIds.h"

PciIds::PciIds()
{
}

PciIds::~PciIds()
{
}

PciIds &PciIds::instance()
{
	static PciIds instance;
	std::unique_lock<std::mutex> lock(instance.mutex);
	if (!instance.bInitialized)
	{
		instance.bInitialized = instance.init();
	}
	return instance;
}

enum id_entry_src
{
	SRC_UNKNOWN,
	SRC_CACHE,
	SRC_NET,
	SRC_HWDB,
	SRC_LOCAL,
};

int pci_id_insert(int cat, int id1, int id2, int id3, int id4, char *text, enum id_entry_src src)
{
	//printf("insert");
	return 0;
}

enum id_entry_type
{
	ID_UNKNOWN,
	ID_VENDOR,
	ID_DEVICE,
	ID_SUBSYSTEM,
	ID_GEN_SUBSYSTEM,
	ID_CLASS,
	ID_SUBCLASS,
	ID_PROGIF
};

static int id_hex(char *p, int cnt)
{
	int x = 0;
	while (cnt--)
	{
		x <<= 4;
		if (*p >= '0' && *p <= '9')
			x += (*p - '0');
		else if (*p >= 'a' && *p <= 'f')
			x += (*p - 'a' + 10);
		else if (*p >= 'A' && *p <= 'F')
			x += (*p - 'A' + 10);
		else
			return -1;
		p++;
	}
	return x;
}

static inline int id_white_p(int c)
{
	return (c == ' ') || (c == '\t');
}

static const char *id_parse_list(FILE *f, int *lino)
{
	char line[1024];
	char *p;
	int id1 = 0, id2 = 0, id3 = 0, id4 = 0;
	int cat = -1;
	int nest;
	static const char parse_error[] = "Parse error";

	*lino = 0;
	while (fgets(line, sizeof(line), f))
	{
		(*lino)++;
		p = line;
		while (*p && *p != '\n' && *p != '\r')
			p++;
		if (!*p && !feof(f))
			return "Line too long";
		*p = 0;
		if (p > line && (p[-1] == ' ' || p[-1] == '\t'))
			*--p = 0;

		p = line;
		while (id_white_p(*p))
			p++;
		if (!*p || *p == '#')
			continue;

		p = line;
		while (*p == '\t')
			p++;
		nest = p - line;

		if (!nest) // Top-level entries
		{
			if (p[0] == 'C' && p[1] == ' ') ///Class block
			{
				if ((id1 = id_hex(p + 2, 2)) < 0 || !id_white_p(p[4]))
					return parse_error;
				cat = ID_CLASS;
				p += 5;
			}
			else if (p[0] == 'S' && p[1] == ' ')
			{ // Generic subsystem block
				if ((id1 = id_hex(p + 2, 4)) < 0 || p[6])
					return parse_error;
				//      if (!pci_id_lookup(a, 0, ID_VENDOR, id1, 0, 0, 0))
				//	return "Vendor does not exist";
				cat = ID_GEN_SUBSYSTEM;
				continue;
			}
			else if (p[0] >= 'A' && p[0] <= 'Z' && p[1] == ' ')
			{ // Unrecognized block (RFU)
				cat = ID_UNKNOWN;
				continue;
			}
			else // Vendor ID
			{
				///int num = std::stoi("0xFFFFF",NULL,16);
				if ((id1 = id_hex(p, 4)) < 0 || !id_white_p(p[4]))
					return parse_error;
				cat = ID_VENDOR;
				p += 5;
			}
			id2 = id3 = id4 = 0;
		}
		else if (cat == ID_UNKNOWN) ///Nested entries in RFU blocks are skipped
			continue;
		else if (nest == 1) // Nesting level 1
			switch (cat)
			{
			case ID_VENDOR:
			case ID_DEVICE:
			case ID_SUBSYSTEM:
				if ((id2 = id_hex(p, 4)) < 0 || !id_white_p(p[4]))
					return parse_error;
				p += 5;
				cat = ID_DEVICE;
				id3 = id4 = 0;
				break;
			case ID_GEN_SUBSYSTEM:
				if ((id2 = id_hex(p, 4)) < 0 || !id_white_p(p[4]))
					return parse_error;
				p += 5;
				id3 = id4 = 0;
				break;
			case ID_CLASS:
			case ID_SUBCLASS:
			case ID_PROGIF:
				if ((id2 = id_hex(p, 2)) < 0 || !id_white_p(p[2]))
					return parse_error;
				p += 3;
				cat = ID_SUBCLASS;
				id3 = id4 = 0;
				break;
			default:
				return parse_error;
			}
		else if (nest == 2) // Nesting level 2 /
			switch (cat)
			{
			case ID_DEVICE:
			case ID_SUBSYSTEM:
				if ((id3 = id_hex(p, 4)) < 0 || !id_white_p(p[4]) || (id4 = id_hex(p + 5, 4)) < 0 || !id_white_p(p[9]))
					return parse_error;
				p += 10;
				cat = ID_SUBSYSTEM;
				break;
			case ID_CLASS:
			case ID_SUBCLASS:
			case ID_PROGIF:
				if ((id3 = id_hex(p, 2)) < 0 || !id_white_p(p[2]))
					return parse_error;
				p += 3;
				cat = ID_PROGIF;
				id4 = 0;
				break;
			default:
				return parse_error;
			}
		else ///Nesting level 3 or more
			return parse_error;
		while (id_white_p(*p))
			p++;
		if (!*p)
			return parse_error;
		if (pci_id_insert(cat, id1, id2, id3, id4, p, SRC_LOCAL))
			return "Duplicate entry";
	}
	return NULL;
}

bool PciIds::init()
{
	std::string fileName = std::string(PCI_IDS_DIR) + "/" + std::string(PCI_IDS_FILE);
	std::ifstream infile;
	std::string temp;

	infile.open(fileName.data());

	if (!infile.is_open())
	{
		return false;
	}

	while (std::getline(infile, temp))
	{
		if(temp.length() > 4)
			std::cout << temp.substr(2, 2).c_str() << std::endl;
	}

	infile.close();

	FILE *f = fopen(fileName.data(), "r");
	int lino;
	const char *err = id_parse_list(f, &lino);

	if (err)
	{
		;
	}
	return true;
}