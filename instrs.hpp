/* Translate 32-bit memory instructions to 64-bit memory instructions */

#pragma once

#include <LIEF/LIEF.hpp>

void transform_instructions(std::shared_ptr<LIEF::MachO::FatBinary> macho);
