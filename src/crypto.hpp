#pragma once
#include <vector>
#include <string>

std::vector<unsigned char> sha256(const std::vector<unsigned char>& data);
std::string hex_encode(const std::vector<unsigned char>& data);