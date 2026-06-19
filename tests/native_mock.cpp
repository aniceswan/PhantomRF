/**
 * @file native_mock.cpp
 * @brief Definitions for the static members of the host test mocks.
 *
 * The mock header declares a couple of `static` members (Preferences'
 * backing store and mutex). The headers can't define them inline because
 * we want a single shared instance across translation units.
 */

#define PHM_NATIVE_TEST 1

#include "native_mock.h"

namespace phm::test {

std::map<std::string, std::string>& Preferences::_store = *new std::map<std::string, std::string>();
std::mutex& Preferences::_mtx = *new std::mutex();

}  // namespace phm::test
