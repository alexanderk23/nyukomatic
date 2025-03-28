#pragma once

#include <string>

extern "C" {
int assemble(std::string &input, std::string &output, std::string &errors);
}
