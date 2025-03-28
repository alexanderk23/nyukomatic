#pragma once

#include <iostream>
#include <rang.hpp>

namespace nm::logger {

inline std::ostream& _log(const char* t, const rang::fgB fg) {
    return std::cerr << rang::style::bold << fg << t << ": " << rang::style::reset;
}

}

#define LOG_F nm::logger::_log("fatal", rang::fgB::red)
#define LOG_E nm::logger::_log("error", rang::fgB::red)
#define LOG_W nm::logger::_log("warn", rang::fgB::yellow)
#define LOG_I nm::logger::_log("info", rang::fgB::gray)
