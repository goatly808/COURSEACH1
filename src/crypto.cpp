#include "crypto.hpp"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

std::vector<unsigned char> sha256(const std::vector<unsigned char>& data) {
    std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);
    SHA256(data.data(), data.size(), hash.data());
    return hash;
}

std::string hex_encode(const std::vector<unsigned char>& data) {
    std::ostringstream oss;
    for (auto byte : data) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return oss.str();
}
