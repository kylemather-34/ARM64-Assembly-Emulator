#include <fstream>
#include <iostream>
#include <string>
#include "parser.hpp"


/*
* Simple CLI front-end that reads a text file of ARM64 assembly and prints
* parsed instructions and operands in a human-friendly format.
*/
int main(int argc, char** argv){
using namespace arm64;


if(argc < 2){
std::cerr << "Usage: " << argv[0] << " <input_asm.txt>";
std::cerr << "Example line: ADD X1, X2, X3";
return 1;
}


std::ifstream fin(argv[1]);
if(!fin){ std::cerr << "Failed to open input file: " << argv[1] << ""; return 2; }

Parser parser;
std::string line; std::size_t lineNo = 0;
while(std::getline(fin, line)){
++lineNo;
auto decoded = parser.parse_line(line);
if(!decoded.has_value()) continue; // skip blank/label lines
arm64::print_decoded(lineNo, *decoded);
}
return 0;
}