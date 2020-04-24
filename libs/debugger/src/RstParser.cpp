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

    // Match a label line
    // Captures: 1:address, 2:label
    //   086C                     354 Lscope3:
    const std::regex labelRe(
        R"([[:space:]]*([0-9|A-F][0-9|A-F][0-9|A-F][0-9|A-F])[[:space:]]+.*[[:space:]]+(.*):$)");

    // Match any stab directive
    const std::regex stabRe(R"(.*\.stab.*)");

    // Match stabs (string) directive
    // Captures: 1:string, 2:type, 3:other, 4:desc, 5:value
    //    204 ;	.stabs	"src/vectrexy.h",132,0,0,Ltext2
    const std::regex stabsRe(
        R"(.*\.stabs[[:space:]]*\"(.*)\",[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*))");

    // Match stabd (dot) directive
    // Captures: 1:type, 2:other, 3:desc
    //    206;.stabd	68, 0, 61
    const std::regex stabdRe(R"(.*\.stabd[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*))");

    // Match an instruction line
    // Capture: 1:address
    //   072B AE E4         [ 5]  126 	ldx	,s	; tmp33, dest
    const std::regex instructionRe(
        R"([[:space:]]*([0-9|A-F][0-9|A-F][0-9|A-F][0-9|A-F])[[:space:]]*.*\[..\].*)");

    auto hexToU16 = [](const std::string& s) {
        return checked_static_cast<uint16_t>(std::stoi(s, {}, 16));
    };

    auto decToU16 = [](const std::string& s) {
        return checked_static_cast<uint16_t>(std::stoi(s, {}, 10));
    };

    constexpr auto N_FUN = 36;   // 0x24 Function name
    constexpr auto N_SLINE = 68; // 0x44 Line number in text segment
    constexpr auto N_LSYM = 128; // 0x80 Local variable or type definition
    constexpr auto N_SOL = 132;  // 0x84 Name of include file

    std::unordered_map<std::string, uint16_t> labelToAddress;
    std::unordered_map<std::string, std::shared_ptr<Type>> typeIdToType;

    std::unique_ptr<Function> currFunction;
    std::unique_ptr<Scope> currScope;
    std::vector<Variable> currVariables;

    std::string line;
    bool reparseCurrLine = false;
    while (reparseCurrLine || std::getline(fin, line)) {
        reparseCurrLine = false;

        std_util::visit_overloads(
            state,
            [&](ParseDirectives& st) {
                // Label line
                std::smatch match;
                if (std::regex_match(line, match, labelRe)) {
                    const uint16_t address = hexToU16(match[1]);
                    const std::string label = match[2];
                    labelToAddress[label] = address;
                }

                // Stab string (stabs) directive
                else if (std::regex_match(line, match, stabsRe)) {
                    const int type = std::stoi(match[2]);

                    // Source file name
                    if (type == N_SOL) {
                        st.sourceFile = match[1].str();
                    }

                    // Function name
                    else if (type == N_FUN) {
                        // TODO: N_FUN is either a function name or text segment variable (e.g.
                        // const variable). We know by looking at '#' in "name:#type". For now,
                        // ignore it.
                        auto fixupFuncName = [](std::string name) {
                            // Remove ":#type"
                            size_t index = name.rfind(":");
                            if (index != -1)
                                name = name.substr(0, index);
                            // If no parens (i.e. "main"), add them
                            if (name.back() != ')') {
                                name += "()";
                            }
                            return name;
                        };

                        const std::string funcName = fixupFuncName(match[1].str()); // Function name
                        const std::string label = match[5].str();                   // Label name

                        auto iter = labelToAddress.find(label);
                        if (iter == labelToAddress.end()) {
                            Printf("Warning! label not found: %s\n", label.c_str());
                            return;
                        }
                        const uint16_t funcAddress = iter->second;
                        m_debugSymbols->AddSymbol(Symbol{funcName, funcAddress});
                    }

                    // Local variable or type definition
                    else if (type == N_LSYM) {
                        const std::string lsymString = match[1];
                        const std::string lsymValue = match[5];

                        // Type definitions:
                        // "int:t7"
                        // "char:t13=r13;0;255;"

                        // Local variables:
                        // "a:7"

                        // 1: type/variable name
                        // 2: 't' for type, or nothing if variable declaration
                        // 3: type def/ref #
                        // 4: type range, or nothing (full match)
                        //  5: type-def # that this is a range of (can be self-referential)
                        //  6: lower-bound of range (if > upper-bound, is size in bytes)
                        //  7: upper-bound of range
                        const std::regex lsymRe(
                            R"((.*):([t]*)([0-9]+)(=[rR]([0-9]+);([0-9]+);([0-9]+);|))");

                        std::smatch lsymMatch;
                        if (std::regex_match(lsymString, lsymMatch, lsymRe)) {
                            const bool isType = !lsymMatch[2].str().empty();

                            // Type definition
                            if (isType) {
                                ASSERT(lsymMatch[2].str() == "t");

                                const auto typeName = lsymMatch[1].str();
                                const auto typeDefId = lsymMatch[3].str();
                                // TODO: look at range info

                                // TODO: handle other types
                                if (typeName != "int")
                                    return;

                                PrimitiveType pt;
                                pt.name = typeName;
                                pt.isSigned = true;
                                pt.byteSize = 1;

                                auto t = std::make_shared<Type>(pt);
                                typeIdToType[typeDefId] = t;

                                m_debugSymbols->AddType(t);
                            }
                            // Variable declaration
                            else {
                                const auto varName = lsymMatch[1].str();
                                const auto typeRefId = lsymMatch[3].str();

                                // Note that the "value" part (the last number) of the stabs for
                                // L_SYM local variables is the stack offset in bytes.
                                // Ex:
                                //
                                // 101 ;	.stabs	"c:7",128,0,0,1
                                const auto stackOffset = lsymValue;

                                auto iter = typeIdToType.find(typeRefId);
                                if (iter != typeIdToType.end()) {
                                    Variable v;
                                    v.name = varName;
                                    v.m_type = iter->second;
                                    v.stackOffset = decToU16(stackOffset);

                                    currVariables.push_back(std::move(v));

                                } else {
                                    Printf("Warning! Type not found for variable '%s' (type-ref "
                                           "id: '%s')\n",
                                           varName.c_str(), typeRefId.c_str());
                                    return;
                                }
                            }
                        }
                    }
                }

                // Stab dot (stabd) directive
                else if (std::regex_match(line, match, stabdRe)) {
                    if (std::stoi(match[1]) == N_SLINE) {
                        uint32_t lineNum = stoi(match[3]);
                        state = ParseLineInstructions{st.sourceFile, lineNum};
                    }
                }
            },

            [&](ParseLineInstructions& st) {
                std::smatch match;
                // Collect source locations while we match instruction lines
                if (std::regex_match(line, match, instructionRe)) {
                    uint16_t address = hexToU16(match[1]);
                    m_debugSymbols->AddSourceLocation(address, {st.sourceFile, st.lineNum});

                }
                // Stop as soon as we hit any stab directive
                else if (std::regex_match(line, match, stabRe)) {
                    reparseCurrLine = true;
                    state = ParseDirectives{st.sourceFile};
                }
            });
    }

    return true;
}
