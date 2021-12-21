#pragma once

#include <stdint.h>
//#include "compiler_specific.h"
namespace xpum {

typedef struct {
	uint8_t bus;
	uint8_t device;
	uint8_t function;
} pci_address_t;
}
