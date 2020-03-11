#include "Debugger/RstParser.h"
#include "Debugger/DebugSymbols.h"
#include "core/Base.h"
#include "core/ConsoleOutput.h"
#include "core/StdUtil.h"

#include <fstream>
#include <regex>
#include <string>
#include <variant>

RstParser::RstParser(DebugSymbols& debugSymbols)
    : m_debugSymbols(&debugSymbols) {}

bool RstParser::Parse(const fs::path& rstFile) {
    Printf("Parsing rst file: %s\n", rstFile.string().c_str());

    struct ParseDirectives {
        std::string sourceFile{};
    };
    struct ParseLineInstructions {
        std::string sourceFile{};
        uint32_t lineNum{};
    };

    std::variant<ParseDirectives, ParseLineInstructions> state{ParseDirectives{}};

    std::ifstream fin(rstFile);
    if (!fin)
        return false;

    // Match the _main label
    //    0AD2                     807 _main:
    const std::regex mainRe(
        R"([[:space:]]*([0-9|A-F][0-9|A-F][0-9|A-F][0-9|A-F])[[:space:]]+.*_main:)");

    // Match any stab directive
    const std::regex stabRe(R"(.*\.stab.*)");

    // Match stabs (string) directive
    //    204 ;	.stabs	"src/vectrexy.h",132,0,0,Ltext2
    const std::regex stabsRe(
        R"(.*\.stabs[[:space:]]*\"(.*)\",[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*))");

    // Match stabd (dot) directive
    //    206;.stabd	68, 0, 61
    const std::regex stabdRe(R"(.*\.stabd[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*))");

    // Match an instruction line
    //   072B AE E4         [ 5]  126 	ldx	,s	; tmp33, dest
    const std::regex instructionRe(
        R"([[:space:]]*([0-9|A-F][0-9|A-F][0-9|A-F][0-9|A-F])[[:space:]]*.*\[..\].*)");

    auto hextoi = [](const std::string& s) {
        return checked_static_cast<uint16_t>(std::stoi(s, {}, 16));
    };

    constexpr auto N_SLINE = 0x44; // Line number in text segment
    constexpr auto N_SOL = 0x84;   // Name of include file

    std::string line;
    bool reparseCurrLine = false;
    while (reparseCurrLine || std::getline(fin, line)) {
        reparseCurrLine = false;

        std_util::visit_overloads(
            state,
            [&](ParseDirectives& st) {
                std::smatch match;
                if (std::regex_match(line, match, mainRe)) {
                    uint16_t address = hextoi(match[1]);
                    m_debugSymbols->AddSymbol(Symbol{"_main", address});

                } else if (std::regex_match(line, match, stabsRe)) {
                    if (std::stoi(match[2]) == N_SOL) {
                        st.sourceFile = match[1].str();
                    }

                } else if (std::regex_match(line, match, stabdRe)) {
                    if (std::stoi(match[1]) == N_SLINE) {
                        uint32_t lineNum = stoi(match[3]);
                        state = ParseLineInstructions{st.sourceFile, lineNum};
                    }
                }
            },
            [&](ParseLineInstructions& st) {
                std::smatch match;
                if (std::regex_match(line, match, instructionRe)) {
                    uint16_t address = hextoi(match[1]);
                    m_debugSymbols->AddSourceLocation(address, {st.sourceFile, st.lineNum});

                } else if (std::regex_match(line, match, stabRe)) {
                    reparseCurrLine = true;
                    state = ParseDirectives{st.sourceFile};
                }
            });
    }

    return true;
}
