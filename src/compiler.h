#pragma once

#include <map>
#include <string>

namespace nm::compiler {

typedef std::map<int, std::string> errors_t;
typedef std::string input_t;
typedef std::string output_t;

bool compile(input_t &source, output_t &output, errors_t &errors);

} // namespace nm::compiler
