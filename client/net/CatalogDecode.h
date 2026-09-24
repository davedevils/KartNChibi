// turns a catalog frame into one structured line so servers can be diffed easily
#pragma once

#include <string>
#include "net/Packet.h"

namespace KnC::Client {

// returns a structured line for a known catalog opcode or empty otherwise reads from a copy
std::string decodeCatalog(uint16_t opcode, const ::knc::Packet& pkt);

}
