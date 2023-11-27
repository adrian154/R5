#ifndef __BUS_H
#define __BUS_H

#include <stdint.h>

void store8(uint64_t addr, uint8_t value);
void store16(uint64_t addr, uint16_t value);
void store32(uint64_t addr, uint32_t value);
void store64(uint64_t addr, uint32_t value);

uint8_t load8(uint64_t addr);
uint16_t load16(uint16_t addr);
uint32_t load32(uint32_t addr);
uint32_t load64(uint64_t addr);

#endif