/*
* ARM64 Assembly Parser
*
* This program reads an input text file containing ARM64 Assembly instructions
* and parses it to extract:
*   1. The instruction mnemonic and
*   2. The operands associated with the instruction (registers, memory references, labels, etc.)
*
* - Strips comments, labels and blank lines before parsing.
* - Splits operands even when inside memory brackets.
* - Classifies operands into types (Register, Immediate, Memory, Label).
* - Uses instruction-specific handlers for stricter parsing (ex. ADD, LDR, STR).
* - Extensible design, additional instruction handlers can be added.
*/

#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>
#include <stdexcept>


#include "parser.hpp"


namespace arm64 {


/********************* Utilities *********************/
std::string ltrim(std::string s){ s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch){return !std::isspace(ch);})); return s; }
std::string rtrim(std::string s){ s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch){return !std::isspace(ch);} ).base(), s.end()); return s; }
std::string trim(std::string s){ return ltrim(rtrim(std::move(s))); }
std::string upper(std::string s){ for(char &c: s) c = std::toupper((unsigned char)c); return s; }


// Split by commas, but keep bracketed memory operands [ ... , ... ] together.
std::vector<std::string> split_operands(const std::string &s) {
    std::vector<std::string> out;
    std::string cur;
    int bracket = 0;

    for (char c : s) {
        if (c == '[') {
            ++bracket;
            cur.push_back(c);
            continue;
        }
        if (c == ']') {
            if (bracket > 0) --bracket;
            cur.push_back(c);
            continue;
        }
        if (c == ',' && bracket == 0) {
            // end of one operand
            // trim leading/trailing spaces
            auto l = cur.find_first_not_of(" \t\r\n");
            auto r = cur.find_last_not_of(" \t\r\n");
            if (l != std::string::npos) out.emplace_back(cur.substr(l, r - l + 1));
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }

    // push the last token
    if (!cur.empty()) {
        auto l = cur.find_first_not_of(" \t\r\n");
        auto r = cur.find_last_not_of(" \t\r\n");
        if (l != std::string::npos) out.emplace_back(cur.substr(l, r - l + 1));
    }
    return out;
}


static bool is_hex(const std::string &s){
if(s.rfind("0x",0)!=0 && s.rfind("0X",0)!=0) return false; if(s.size()<=2) return false;
for(size_t i=2;i<s.size();++i){ if(!std::isxdigit((unsigned char)s[i])) return false; }
return true;
}


static long long parse_int(const std::string &tok){
std::string t = tok;
if(!t.empty() && t[0]=='#') t = t.substr(1);
if(is_hex(t)) return std::stoll(t, nullptr, 16);
return std::stoll(t, nullptr, 0);
}


// Core operand tokenizer
Operand parse_operand(std::string tok){
tok = trim(tok);
Operand op; op.text = tok; op.type = OperandType::Unknown;


// Memory [BASE] or [BASE, OFFSET]
if(!tok.empty() && tok.front()=='[' && tok.back()==']'){
std::string inner = tok.substr(1, tok.size()-2);
auto parts = split_operands(inner); // re-use comma-aware split for inside too
op.type = OperandType::Memory;
if(!parts.empty()) op.memBase = upper(parts[0]);
if(parts.size()>=2){
std::string off = trim(parts[1]);
if(!off.empty() && (off[0]=='#' || off[0]=='+' || off[0]=='-' || std::isdigit((unsigned char)off[0]) || (off.rfind("0x",0)==0 || off.rfind("0X",0)==0))){
try{ op.memOffset = parse_int(off); op.memHasOffset=true; } catch(...){}
}
}
return op;
}


std::string up = upper(tok);
} // namespace arm64