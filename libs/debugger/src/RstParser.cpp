#include "Debugger/RstParser.h"
#include "Debugger/DebugSymbols.h"
#include "core/Base.h"
#include "core/ConsoleOutput.h"
#include "core/StdUtil.h"
#include "core/StringUtil.h"

#include <fstream>
#include <limits>
#include <regex>
#include <string>
#include <variant>

namespace {
    struct MatchBase {
        MatchBase(const char* re, const std::string& s)
            : m_re(re) {
            m_matched = std::regex_match(s, m_match, m_re);
        }

        operator bool() const { return m_matched; }

    protected:
        std::regex m_re;
        std::smatch m_match;
        bool m_matched{};
    };

    // Match a label line
    // Captures: 1:address, 2:label
    //   086C                     354 Lscope3:
    struct LabelMatch : MatchBase {
        LabelMatch(const std::string& s)
            : MatchBase(
                  R"([[:space:]]*([0-9|A-F][0-9|A-F][0-9|A-F][0-9|A-F])[[:space:]]+.*[[:space:]]+(.*):$)",
                  s) {}
        std::string Address() const { return m_match[1].str(); }
        std::string Label() const { return m_match[2].str(); }
    };

    // Match any stab directive
    struct StabMatch : MatchBase {
        StabMatch(const std::string& s)
            : MatchBase(R"(.*\.stab.*)", s) {}
    };

    // Match stabs (string) directive
    // Captures: 1:string, 2:type, 3:other, 4:desc, 5:value
    //    204 ;	.stabs	"src/vectrexy.h",132,0,0,Ltext2
    struct StabStringMatch : MatchBase {
        StabStringMatch(const std::string& s)
            : MatchBase(
                  R"(.*\.stabs[[:space:]]*\"(.*)\",[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*))",
                  s) {}
        std::string String() const { return m_match[1].str(); }
        std::string Type() const { return m_match[2].str(); }
        std::string Other() const { return m_match[3].str(); }
        std::string Desc() const { return m_match[4].str(); }
        std::string Value() const { return m_match[5].str(); }
    };

    // Match stabd (dot) directive
    // Captures: 1:type, 2:other, 3:desc
    //    206;.stabd	68, 0, 61
    struct StabDotMatch : MatchBase {
        StabDotMatch(const std::string& s)
            : MatchBase(R"(.*\.stabd[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*))", s) {}
        std::string Type() const { return m_match[1].str(); }
        std::string Other() const { return m_match[2].str(); }
        std::string Desc() const { return m_match[3].str(); }
    };

    // Match stabn (number) directive
    // Captures: 1:type, 2:other, 3:desc, 4:value
    //    869;.stabn	192, 0, 0, LBB8
    struct StabNumberMatch : MatchBase {
        StabNumberMatch(const std::string& s)
            : MatchBase(
                  R"(.*\.stabn[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*),[[:space:]]*(.*))",
                  s) {}
        std::string Type() const { return m_match[1].str(); }
        std::string Other() const { return m_match[2].str(); }
        std::string Desc() const { return m_match[3].str(); }
        std::string Value() const { return m_match[4].str(); }
    };

    // Match an instruction line
    // Capture: 1:address
    //   072B AE E4         [ 5]  126 	ldx	,s	; tmp33, dest
    struct InstructionMatch : MatchBase {
        InstructionMatch(const std::string& s)
            : MatchBase(
                  R"([[:space:]]*([0-9|A-F][0-9|A-F][0-9|A-F][0-9|A-F])[[:space:]]*.*\[..\].*)",
                  s) {}
        std::string Address() const { return m_match[1].str(); }
    };

    // Match stabs type string for N_LSYM: type definitions or variable declarations
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
    struct LSymMatch : MatchBase {
        LSymMatch(const std::string& s)
            : MatchBase(R"((.*):([t]*)([0-9]+)(=[rR]([0-9]+);((?:-|)[0-9]+);((?:-|)[0-9]+);|))",
                        s) {}

        // If false, this is a variable declaration
        bool IsTypeDef() const { return m_match[2].str() == "t"; }

        // Type definition interface
        std::string TypeName() const { return m_match[1].str(); }
        std::string TypeDefId() const { return m_match[3].str(); }
        bool HasRange() const { return !m_match[4].str().empty(); }
        std::string RangeTypeDefNum() const { return m_match[5].str(); }
        std::string RangeLowerBound() const { return m_match[6].str(); }
        std::string RangeUpperBound() const { return m_match[7].str(); }

        // Variable declaration interface
        std::string VarName() const { return m_match[1].str(); }
        std::string VarTypeRefId() const { return m_match[3].str(); }
    };

    // Match stabs type string for N_LSYM: pointer type definitions and variable declarations
    // Pointers are both type definitions AND variable declarations:
    // "p:25=*7"
    //
    // 1: variable name
    // 2: type def #
    // 3: type ref # that this is a pointer to
    //
    // Note: stabs outputs exactly the same data for references. We cannot
    // easily distinguish them.
    struct LSymPointerMatch : MatchBase {
        LSymPointerMatch(const std::string& s)
            : MatchBase(R"((.*):([0-9]+)=\*([0-9]+))", s) {}

        std::string VarName() const { return m_match[1].str(); }
        std::string TypeDefId() const { return m_match[2].str(); }
        std::string TypeRefId() const { return m_match[3].str(); }
    };

    // Match stabs type string for N_LSYM: enum type definitions
    // "bool:t22=eFalse:0,True:1,;"
    // "WeekDay:t25=eMonday:0,Tuesday:1,Wednesday:2,EndOfDays:2,Foo:-5000,;"
    //
    // 1: type (enum) name
    // 2: type def #
    // 3. values (comma-separated key:value pairs)
    struct LSymEnumMatch : MatchBase {
        LSymEnumMatch(const std::string& s)
            : MatchBase(R"((.*):t([0-9]+)=e(.*),;)", s) {}

        std::string TypeName() const { return m_match[1].str(); }
        std::string TypeDefId() const { return m_match[2].str(); }
        std::string Values() const { return m_match[3].str(); }
    };

    uint16_t HexToU16(const std::string& s) {
        return checked_static_cast<uint16_t>(std::stoi(s, {}, 16));
    }

    uint16_t DecToU16(const std::string& s) {
        return checked_static_cast<uint16_t>(std::stoi(s, {}, 10));
    }

    constexpr auto N_FUN = 36;    // 0x24 Function name
    constexpr auto N_SLINE = 68;  // 0x44 Line number in text segment
    constexpr auto N_LSYM = 128;  // 0x80 Local variable or type definition
    constexpr auto N_SOL = 132;   // 0x84 Name of include file
    constexpr auto N_LBRAC = 192; // 0xC0 Left brace (open scope)
    constexpr auto N_RBRAC = 224; // 0xE0 Right brace (close scope)

} // namespace

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

    std::unordered_map<std::string, uint16_t> labelToAddress;
    std::unordered_map<std::string, std::shared_ptr<Type>> typeIdToType;

    std::shared_ptr<Function> currFunction;
    std::shared_ptr<Scope> currScope{};
    std::vector<Variable> currVariables;

    auto GetLine = [](std::ifstream& fin, std::string& line) {
        if (!std::getline(fin, line))
            return false;

        // stab string lines that are too long are extended over multiple consecutive lines by
        // adding a '\\' token at the end of the 'string' portion, for example:
        //   59;.stabs	"WeekDay:t25=eMonday:0,Tuesday:1,Wednesday:2,EndOfDays:2,\\", 128, 0, 0, 0
        //   60;.stabs	"Foo:-5000,;", 128, 0, 0, 0
        // To simplify parsing, we join all these lines into a single one.
        if (auto stabs = StabStringMatch(line)) {
            const char* LineContinuationToken = R"(\\)";
            auto s = stabs.String();

            while (s.size() >= 2 && s.substr(s.size() - 2) == LineContinuationToken) {
                std::string nextLine;
                if (!std::getline(fin, nextLine))
                    return false;
                auto nextStabs = StabStringMatch(nextLine);
                ASSERT(nextStabs);
                s = nextStabs.String();

                // Replace the "\\" with the string contents of the next line
                auto index = line.rfind(LineContinuationToken);
                ASSERT(index != std::string::npos);
                line.erase(index, 2);
                line.insert(index, s);
            }
        }

        return true;
    }; // namespace

    std::string line;
    bool reparseCurrLine = false;
    while (reparseCurrLine || GetLine(fin, line)) {
        reparseCurrLine = false;

        auto parseLabelLine = [&](const std::string& line) -> bool {
            if (auto label = LabelMatch(line)) {
                labelToAddress[label.Label()] = HexToU16(label.Address());
                return true;
            }
            return false;
        };

        std_util::visit_overloads(
            state,
            [&](ParseDirectives& st) {
                // Label line
                if (parseLabelLine(line)) {
                }

                // Stab string (stabs) directive
                else if (auto stabs = StabStringMatch(line)) {
                    const int type = std::stoi(stabs.Type());

                    // Source file name
                    if (type == N_SOL) {
                        st.sourceFile = stabs.String();
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

                        const std::string funcName = fixupFuncName(stabs.String()); // Function name
                        const std::string label = stabs.Value();                    // Label name

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

                        auto addEnumType = [&](const std::string& typeDefId,
                                               const std::string& typeName, bool isSigned,
                                               size_t byteSize,
                                               std::unordered_map<ssize_t, std::string> valueToId) {
                            auto t = std::make_shared<EnumType>();
                            t->name = typeName;
                            t->isSigned = isSigned;
                            t->byteSize = byteSize;
                            t->valueToId = std::move(valueToId);

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
                            v.location = Variable::StackOffset{stackOffset};

                            currVariables.push_back(std::move(v));
                        };

                        const std::string lsymString = stabs.String();
                        const std::string lsymValue = stabs.Value();

                        if (auto lsym = LSymMatch(lsymString)) {
                            // Type definition
                            if (lsym.IsTypeDef()) {
                                const auto typeName = lsym.TypeName();
                                const auto typeDefId = lsym.TypeDefId();
                                const bool hasRange = lsym.HasRange();

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
                                    const auto lowerBound = std::stoll(lsym.RangeLowerBound());
                                    const auto upperBound = std::stoll(lsym.RangeUpperBound());

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
                                const auto varName = lsym.VarName();
                                const auto typeRefId = lsym.VarTypeRefId();

                                // Note that the "value" part (the last number) of the stabs for
                                // L_SYM local variables is the stack offset in bytes.
                                // Ex:
                                // 101 ;	.stabs	"c:7",128,0,0,1
                                const auto stackOffset = lsymValue;

                                auto iter = typeIdToType.find(typeRefId);
                                if (iter == typeIdToType.end()) {
                                    Printf("Warning! Type not found for variable '%s' (type-ref "
                                           "id: '%s')\n",
                                           varName.c_str(), typeRefId.c_str());
                                    return;
                                } else {
                                    addVariable(varName, iter->second, DecToU16(stackOffset));
                                }
                            }
                        }
                        // Pointer type definition AND variable declaration
                        else if (auto lsymPointer = LSymPointerMatch(lsymString)) {
                            const auto varName = lsymPointer.VarName();
                            const auto typeDefId = lsymPointer.TypeDefId();
                            const auto typeRefId = lsymPointer.TypeRefId();

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
                            addVariable(varName, indirectType, DecToU16(stackOffset));
                        }
                        // Enum type definitions
                        else if (auto lsymEnum = LSymEnumMatch(lsymString)) {
                            auto minValue = std::numeric_limits<ssize_t>::max();
                            auto maxValue = std::numeric_limits<ssize_t>::min();
                            std::unordered_map<ssize_t, std::string> valueToId;

                            auto values = StringUtil::Split(lsymEnum.Values(), ",");
                            for (auto& value : values) {
                                auto kvp = StringUtil::Split(value, ":");
                                auto id = kvp[0];

                                // For "bool", gcc emits an enum type with "True" and "False" as
                                // identifiers. We want these to be displayed as "true" and "false"
                                // instead.
                                if (lsymEnum.TypeName() == "bool") {
                                    id = StringUtil::ToLower(id);
                                }

                                ssize_t v = std::stoll(kvp[1]);
                                minValue = std::min(minValue, v);
                                maxValue = std::max(maxValue, v);

                                // Only insert the first identifier mapped to the same value in the
                                // enum. This way we only display the first one in the debugger,
                                // like Visual Studio does.
                                valueToId.try_emplace(v, id);
                            }

                            const bool isSigned = minValue < 0;

                            // For byte size, we are assuming the compiler will pick the smallest
                            // possible byte size that can represent this enum. This assumption may
                            // not always be true, but it seems to be for the gcc fork that targets
                            // 6809.
                            const auto numBits =
                                static_cast<size_t>(::log2(maxValue - minValue + 1));
                            const size_t byteSize = (numBits + 7) / 8;

                            addEnumType(lsymEnum.TypeDefId(), lsymEnum.TypeName(), isSigned,
                                        byteSize, std::move(valueToId));
                        }
                    }
                }

                // Stab dot (stabd) directive
                else if (auto stabd = StabDotMatch(line)) {
                    const int type = std::stoi(stabd.Type());

                    if (type == N_SLINE) {
                        uint32_t lineNum = stoi(stabd.Desc());
                        // If the compiler injects code (e.g. no return in main, compiler injects
                        // "return 0;" instructions), the stab data says this code is at line "0" in
                        // the file, because technically it's not in the file. We ignore these,
                        // since they don't correspond to actual source locations.
                        if (lineNum != 0) {
                            state = ParseLineInstructions{st.sourceFile, lineNum};
                        }
                    }
                }

                // Stab number (stabn) directive
                else if (auto stabn = StabNumberMatch(line)) {
                    const int type = std::stoi(stabn.Type());
                    const std::string value = stabn.Value();

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
                // Label line
                if (parseLabelLine(line)) {
                }

                // Collect source locations while we match instruction lines
                else if (auto instruction = InstructionMatch(line)) {
                    uint16_t address = HexToU16(instruction.Address());
                    m_debugSymbols->AddSourceLocation(address, {st.sourceFile, st.lineNum});

                }
                // Stop as soon as we hit any stab directive
                else if (StabMatch(line)) {
                    reparseCurrLine = true;
                    state = ParseDirectives{st.sourceFile};
                }
            });
    }

    return true;
}
