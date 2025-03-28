#include "compiler.h"

#include <boost/regex.hpp>
#include <exception>
#include <iostream>

#include "esjasm/esjasm.hpp"
#include "logger.h"

namespace nm::compiler {

bool compile(input_t &source, output_t &output, errors_t &errors) {
    static const std::string prologue{"\tdevice zxspectrum128\n\torg $6000\n"};
    static const boost::regex re(R"##(\((\d+)\): ((?:error|warning): ([^\n]+)))##");

    output.clear();
    errors.clear();

    int res;
    std::string src = prologue + source;
    std::string err;

    try {
        res = assemble(src, output, err);
    } catch (std::exception e) {
        LOG_E << "compiler crashed: " << e.what() << std::endl;
        return false;
    }

    if (!err.empty()) {
        auto err_begin = boost::sregex_iterator(err.begin(), err.end(), re);
        auto err_end = boost::sregex_iterator();
        for (boost::sregex_iterator i = err_begin; i != err_end; ++i) {
            boost::smatch match = *i;
            errors.insert({std::stoi(match.str(1)) - 2, match.str(2)});
        }
    }

    return res == 0;
}

} // namespace nm::compiler
