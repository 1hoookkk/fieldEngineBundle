#include "ProteusLayerFilter.h"
#include <sstream>

namespace emu_sysex {

std::string LayerFilter14::debug() const {
  std::ostringstream o;
  o << "type=" << int(filterType)
    << " cut=" << int(cutoff)
    << " q=" << int(q)
    << " morphIdx=" << int(morphIndex)
    << " morphDepth=" << int(morphDepth)
    << " tilt=" << int(tilt)
    << " rsv=[";
  for (int i = 0; i < 8; ++i) {
    o << int(reserved[i]);
    if (i < 7) o << ",";
  }
  o << "]";
  return o.str();
}

} // namespace emu_sysex

