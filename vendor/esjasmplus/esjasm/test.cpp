#include "esjasm.hpp"
#include <string>

#ifdef WIN32
int main(int argc, char* argv[]) {
#else
int main(int argc, char **argv) {
#endif
	std::string i{"\tfoo"};
	std::string o{""};
	std::string e{""};
	assemble(i, o, e);
	return 0;
}