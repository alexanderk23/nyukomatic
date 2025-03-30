#include "options.h"

#include <fstream>
#include <iostream>

#include <cxxopts.hpp>
#include <json.h>
#include <rang.hpp>

#include "logger.h"

namespace nm::options {

cxxopts::ParseResult args;
jt::Json json;

std::string url{"ws://drone.alkama.com:9000/nyukomatic/test"};
std::string configFileName{""};
bool isSender = true;
int fps = 50;
bool vsync = true;
bool fpsLock = true;
bool fullBorder = false;

bool loadConfigFile(const std::string &filename) {
    std::ifstream configFile(filename);
    if (!configFile.is_open())
        return false;

    std::stringstream cfg;
    cfg << configFile.rdbuf();
    configFile.close();

    jt::Json::Status status;
    std::tie(status, json) = jt::Json::parse(cfg.str());

    return (status == jt::Json::success && json.isObject());
}

void parseOptions(int argc, char **argv) {
    cxxopts::Options opts(argv[0], "");

    // clang-format off
    opts.add_options()
        ("c,config", "Configuration file name", cxxopts::value<std::string>())
        ("s,server", "Server URI", cxxopts::value<std::string>()->default_value(url))
        ("g,grabber", "Run in grabber mode")
        ("h,help", "Print usage");
    // clang-format on

    try {
        args = opts.parse(argc, argv);
        if (args.count("help")) {
            opts.show_positional_help();
            std::cerr << opts.help() << std::endl;
            exit(0);
        }
    } catch (const cxxopts::exceptions::parsing &e) {
        LOG_F << "failed to parse args: " << e.what() << std::endl;
        std::cerr << opts.help() << std::endl;
        exit(-1);
    }

    // use custom config
    if (args.count("config")) {
        configFileName = args["config"].as<std::string>();
        if (!loadConfigFile(configFileName)) {
            LOG_F << "failed to load config file " << configFileName << std::endl;
            exit(-2);
        }
        LOG_I << "using config file " << configFileName << std::endl;
    }

    if (args.count("server")) {
        url = args["server"].as<std::string>();
    } else if (json["server"].isString()) {
        url = json["server"].getString();
    }

    if (args.count("grabber")) {
        isSender = !args["grabber"].as<bool>();
    } else if (json["grabber"].isBool()) {
        isSender = !json["grabber"].getBool();
    }

    if (json["fps"].isNumber()) {
        fps = json["fps"].getNumber();
    }
}

} // namespace nm::options
