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

    // Match stabn (number) directive
    // Captures: 1:type, 2:other, 3:desc, 4:value
    //    869;.stabn	192, 0, 0, LBB8
    const std::regex stabnRe(
        R"(.*\.stabn[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*))");

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

    constexpr auto N_FUN = 36;    // 0x24 Function name
    constexpr auto N_SLINE = 68;  // 0x44 Line number in text segment
    constexpr auto N_LSYM = 128;  // 0x80 Local variable or type definition
    constexpr auto N_SOL = 132;   // 0x84 Name of include file
    constexpr auto N_LBRAC = 192; // 0xC0 Left brace (open scope)
    constexpr auto N_RBRAC = 224; // 0xE0 Right brace (close scope)

    std::unordered_map<std::string, uint16_t> labelToAddress;
    std::unordered_map<std::string, std::shared_ptr<Type>> typeIdToType;

    std::shared_ptr<Function> currFunction;
    std::shared_ptr<Scope> currScope{};
    std::vector<Variable> currVariables;

    std::string line;
    bool reparseCurrLine = false;
    while (reparseCurrLine || std::getline(fin, line)) {
        reparseCurrLine = false;

        auto parseLabelLine = [&](const std::string& line) -> bool {
            std::smatch match;
            if (std::regex_match(line, match, labelRe)) {
                const uint16_t address = hexToU16(match[1]);
                const std::string label = match[2];
                labelToAddress[label] = address;
                return true;
            }
            return false;
        };

        std_util::visit_overloads(
            state,
            [&](ParseDirectives& st) {
                std::smatch match;

                // Label line
                if (parseLabelLine(line)) {
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

                        // Note that currFunction may still point to the last one we parsed at this
                        // point. This happens if the function has no scopes (no local variables).
                        ASSERT(!currScope);
                        currFunction = std::make_shared<Function>(funcName, funcAddress);
                    }

                    // Local variable or type definition
                    else if (type == N_LSYM) {
                        auto addPrimitiveType = [&](const std::string& typeDefId,
                                                    const std::string& typeName, bool isSigned,
                                                    size_t byteSize) -> std::shared_ptr<Type> {
                            auto t = std::make_shared<PrimitiveType>();
                            t->name = typeName;
                            t->isSigned = isSigned;
                            t->byteSize = byteSize;

                            // Figure out the format type
                            if (typeName.find("float") != std::string::npos ||
                                typeName.find("double") != std::string::npos) {
                                t->format = PrimitiveType::Format::Float;
                            } else if (typeName.find("char") != std::string::npos &&
                                       byteSize == 1) {
                                t->format = PrimitiveType::Format::Char;
                            } else {
                                t->format = PrimitiveType::Format::Int;
                            }

                            typeIdToType[typeDefId] = t;
                            m_debugSymbols->AddType(t);
                            return t;
                        };

                        auto addIndirectType =
                            [&](const std::string& typeDefId,
                                std::shared_ptr<Type> type) -> std::shared_ptr<Type> {
                            auto t = std::make_shared<IndirectType>();
                            t->name = type->name + "*";
                            t->type = std::move(type);

                            typeIdToType[typeDefId] = t;
                            m_debugSymbols->AddType(t);
                            return t;
                        };

                        auto addVariable = [&](std::string name, std::shared_ptr<Type> type,
                                               uint16_t stackOffset) {
                            Variable v;
                            v.name = std::move(name);
                            v.type = std::move(type);
                            v.stackOffset = stackOffset;

                            currVariables.push_back(std::move(v));
                        };

                        const std::string lsymString = match[1];
                        const std::string lsymValue = match[5];

                        // Type definitions:
                        // "int:t7"
                        // "char:t13=r13;0;255;"
                        //
                        // Local variables:
                        // "a:7"
                        //
                        // 1: type/variable name
                        // 2: 't' for type, or nothing if variable declaration
                        // 3: type def/ref #
                        // 4: type range, or nothing (full match)
                        //  5: type-def # that this is a range of (can be self-referential)
                        //  6: lower-bound of range (if > upper-bound, is size in bytes)
                        //  7: upper-bound of range
                        const std::regex lsymRe(
                            R"((.*):([t]*)([0-9]+)(=[rR]([0-9]+);((?:-|)[0-9]+);((?:-|)[0-9]+);|))");

                        // Pointers are both type definitions AND variable declarations:
                        // "p:25=*7"
                        //
                        // 1: variable name
                        // 2: type def #
                        // 3: type ref # that this is a pointer to
                        //
                        // Note: stabs outputs exactly the same data for references. We cannot
                        // easily distinguish them.
                        const std::regex lsymPointerRe(R"((.*):([0-9]+)=\*([0-9]+))");

                        std::smatch lsymMatch;
                        if (std::regex_match(lsymString, lsymMatch, lsymRe)) {
                            const bool isType = !lsymMatch[2].str().empty();

                            // Type definition
                            if (isType) {
                                ASSERT(lsymMatch[2].str() == "t");

                                const auto typeName = lsymMatch[1].str();
                                const auto typeDefId = lsymMatch[3].str();

                                const bool hasRange = !lsymMatch[4].str().empty();

                                if (!hasRange) {
                                    // If it's an alias for an existing type, just skip it. We will
                                    // always display the first type, not the alias type name. E.g.
                                    // 'signed' is an alias for 'int', 'unsigned long' is an alias
                                    // for 'unsigned long int'.
                                    if (typeIdToType.find(typeDefId) != typeIdToType.end())
                                        return;

                                    // TODO: handle 'bool' which is an enum type (handle enum types,
                                    // basically)

                                    // Since no range is given, we are expected to just know the
                                    // type. Match on the types we've seen so far in stabs that
                                    // don't specify a range.
                                    if (typeName == "int") {
                                        // TODO: byte size if 1 or 2, depending on if -mint8 was
                                        // passed or not. Need to figure that out, or have the user
                                        // tell us.
                                        addPrimitiveType(typeDefId, typeName, true, 1);
                                    } else if (typeName == "void") {
                                        addPrimitiveType(typeDefId, typeName, false, 0);
                                    }
                                } else {
                                    // Ignore the type-def # that this is a range of because it's
                                    // not actually helpful. For example, 'float' is apparently a
                                    // range of type 'int'. Let's just use it to determine the byte
                                    // size and sign.
                                    const auto lowerBound = std::stoll(lsymMatch[6]);
                                    const auto upperBound = std::stoll(lsymMatch[7]);

                                    if (lowerBound <= upperBound) {
                                        bool isSigned = lowerBound < 0;

                                        auto numBits = static_cast<size_t>(
                                            ::log2(upperBound - lowerBound + 1));

                                        // HACK: sometimes the range will be something like [0,127],
                                        // instead of [-128,127]. For these cases, let's assume it's
                                        // signed, and increase number of bits until it's a valid
                                        // 8-bit multiple.
                                        if (numBits % 8 != 0) {
                                            isSigned = true;
                                            while (numBits % 8 != 0)
                                                ++numBits;
                                        }

                                        addPrimitiveType(typeDefId, typeName, isSigned,
                                                         numBits / 8);
                                    } else {
                                        // Special case where lowerBound specifies the number of
                                        // bytes
                                        addPrimitiveType(typeDefId, typeName, true,
                                                         static_cast<size_t>(lowerBound));
                                    }
                                }
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
                                if (iter == typeIdToType.end()) {
                                    Printf("Warning! Type not found for variable '%s' (type-ref "
                                           "id: '%s')\n",
                                           varName.c_str(), typeRefId.c_str());
                                    return;
                                } else {
                                    addVariable(varName, iter->second, decToU16(stackOffset));
                                }
                            }
                        }
                        // Pointer type definition AND variable declaration
                        else if (std::regex_match(lsymString, lsymMatch, lsymPointerRe)) {
                            const auto varName = lsymMatch[1].str();
                            const auto typeDefId = lsymMatch[2].str();
                            const auto typeRefId = lsymMatch[3].str();

                            // Find the type that this is a pointer to and create a new type for it
                            auto iter = typeIdToType.find(typeRefId);
                            if (iter == typeIdToType.end()) {
                                Printf("Warning! Type not found for variable '%s' (type-ref id: "
                                       "'%s')\n",
                                       varName.c_str(), typeRefId.c_str());
                                return;
                            }

                            const auto stackOffset = lsymValue;

                            auto indirectType = addIndirectType(typeDefId, iter->second);
                            addVariable(varName, indirectType, decToU16(stackOffset));
                        }
                    }
                }

                // Stab dot (stabd) directive
                else if (std::regex_match(line, match, stabdRe)) {
                    const int type = std::stoi(match[1]);

                    if (type == N_SLINE) {
                        uint32_t lineNum = stoi(match[3]);
                        state = ParseLineInstructions{st.sourceFile, lineNum};
                    }
                }

                // Stab number (stabn) directive
                else if (std::regex_match(line, match, stabnRe)) {
                    const int type = std::stoi(match[1]);
                    const std::string value = match[4];

                    if (type == N_LBRAC) {
                        // Create a scope and transfer all variable declarations we've collected so
                        // far
                        auto scope = std::make_shared<Scope>();
                        scope->variables = std::move(currVariables);

                        const std::string label = value;
                        auto iter = labelToAddress.find(label);
                        if (iter == labelToAddress.end()) {
                            FAIL_MSG("Warning! label not found: %s\n", label.c_str());
                        }
                        const uint16_t scopeStartAddress = iter->second;
                        scope->range.first = scopeStartAddress;

                        // Make current
                        if (currScope) {
                            currScope->AddChild(scope);
                        }
                        currScope = scope;

                        // Add the first scope to the function
                        if (!currFunction->scope) {
                            currFunction->scope = scope;
                        }

                    } else if (type == N_RBRAC) {
                        // Set the end address for the closing scope
                        const std::string label = value;
                        auto iter = labelToAddress.find(label);
                        if (iter == labelToAddress.end()) {
                            FAIL_MSG("Warning! label not found: %s\n", label.c_str());
                        }
                        const uint16_t scopeEndAddress = iter->second;
                        currScope->range.second = scopeEndAddress;

                        // Set current scope to parent
                        ASSERT(currScope);
                        currScope = currScope->Parent();

                        // On the last closing scope, we're done defining the current function
                        if (!currScope) {
                            m_debugSymbols->AddFunction(std::move(currFunction));
                        }
                    }
                }
            },

            [&](ParseLineInstructions& st) {
                std::smatch match;

                // Label line
                if (parseLabelLine(line)) {
                }

                // Collect source locations while we match instruction lines
                else if (std::regex_match(line, match, instructionRe)) {
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
