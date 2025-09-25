#include "Assembler.h"

namespace fe { namespace sysex {

std::optional<std::vector<uint8_t>> push(AssemblyState& st, const Chunk& c) {
  if (st.id == 0 || st.id != c.id || st.expectedTotal == 0) {
    // Initialize new assembly if empty or id changed
    st.id = c.id;
    st.expectedTotal = c.total;
    st.parts.clear();
    st.parts.resize(c.total);
  }
  if (c.seq >= st.parts.size()) {
    return std::nullopt; // out of range
  }
  st.parts[c.seq] = c.data;

  // Check completeness
  for (size_t i = 0; i < st.parts.size(); ++i) {
    if (st.parts[i].empty()) return std::nullopt;
  }
  // Concatenate
  std::vector<uint8_t> out;
  size_t totalSize = 0;
  for (auto& part : st.parts) totalSize += part.size();
  out.reserve(totalSize);
  for (auto& part : st.parts) {
    out.insert(out.end(), part.begin(), part.end());
  }
  // Reset to accept next set (optional)
  st.id = 0;
  st.expectedTotal = 0;
  st.parts.clear();
  return out;
}

}} // namespace fe::sysex

