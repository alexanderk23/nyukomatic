#pragma once

#include <string>

namespace nm::options {

extern std::string url;
extern std::string configFileName;
extern bool isSender;
extern int fps;
extern bool vsync;
extern bool fpsLock;
extern bool fullBorder;

bool loadConfigFile(const std::string &filename);
void parseOptions(int argc, char **argv);

} // namespace nm::options
