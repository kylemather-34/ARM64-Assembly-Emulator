#include <fstream>
#include <iostream>
#include <string>

#include "parser.hpp"

int main(int argc, char** argv) {
    using namespace arm64;

    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <input.asm>\n";
        return 1;
    }

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "error: could not open file: " << argv[1] << "\n";
        return 1;
    }

    Parser parser;
    std::string line;
    std::size_t lineNo = 1;

    while (std::getline(in, line)) {
        try {
            auto decoded = parser.parse_line(line);
            if (decoded) {
                print_decoded(lineNo, *decoded);
            }
        } catch (const std::exception& ex) {
            std::cerr << "Parse error on line " << lineNo << ": " << ex.what() << "\n";
            return 2; // stop on first execution-blocking parse error
        }
        ++lineNo;
    }

    return 0;
}