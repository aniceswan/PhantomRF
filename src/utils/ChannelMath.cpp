/**
 * @file ChannelMath.cpp
 * @brief Pure channel ↔ frequency conversion helpers — TU
 *
 * All functions in `ChannelMath.h` are `constexpr`, so this translation
 * unit exists only to keep the build happy and to give the linker
 * somewhere to anchor the symbols. There is no runtime state.
 *
 * @author PhantomRF Project
 * @date 2026
 */
#include "utils/ChannelMath.h"

// This translation unit is intentionally empty. The functions in
// ChannelMath.h are all `constexpr` and resolved at compile time.
// A non-empty .cpp is included so the file appears in build outputs
// and so future non-constexpr helpers have a place to live.

namespace phm::util {

// Reserved for future non-constexpr helpers (e.g. caching tables).
namespace {

}  // namespace
}  // namespace phm::util
