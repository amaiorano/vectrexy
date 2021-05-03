#include "debugger/RstParser.h"
#include "core/Base.h"
#include "core/ConsoleOutput.h"
#include "core/StdUtil.h"
#include "core/StringUtil.h"
#include "debugger/RstMatchers.h"

#include <cmath>
#include <limits>

namespace {

    uint16_t HexToU16(const std::string& s) {
        return checked_static_cast<uint16_t>(std::stoi(s, {}, 16));
    }

    uint16_t DecToU16(const std::string& s) {
        return checked_static_cast<uint16_t>(std::stoi(s, {}, 10));
    }

    template <typename T = void>
    size_t StringToSizeT(const std::string& s) {
        if constexpr (sizeof(size_t) == sizeof(unsigned long long)) {
            return std::stoull(s);
        } else if constexpr (sizeof(size_t) == sizeof(unsigned int)) {
            return std::stoul(s);
        } else {
            static_assert(dependent_false<T>::value, "size_t case missing");
            FAIL();
            return 0;
        }
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

bool RstParser::Parse(const fs::path& rstFilePath) {
    Printf("Parsing rst file: %s\n", rstFilePath.string().c_str());

    std::ifstream fin(rstFilePath);
    if (!fin)
        return false;

    m_rstFile = RstFile{fin};

    std::string line;
    bool reparseCurrLine = false;
    while (reparseCurrLine || m_rstFile.ReadLine(line)) {
        reparseCurrLine = false;

        auto parseLabelLine = [&](const std::string& line) -> bool {
            if (auto label = LabelMatch(line)) {
                m_labelToAddress[label.Label()] = HexToU16(label.Address());
                return true;
            }
            return false;
        };

        std_util::visit_overloads(
            m_state,
            [&](ParseDirectives&) {
                if (parseLabelLine(line)) {

                } else if (auto stabs = StabStringMatch(line)) {
                    HandleStabStringMatch(stabs);

                } else if (auto stabd = StabDotMatch(line)) {
                    HandleStabDotMatch(stabd);

                } else if (auto stabn = StabNumberMatch(line)) {
                    HandleStabNumberMatch(stabn);
                }

                // If we're defining a function, and the next line is not a stabs line, we're done
                // defining this function. Wrap it up and add to debug symbols. Note that this
                // assumes that the stabs info for function definitions is always defined as a block
                // of stab lines followed by one non-stab line.
                if (m_currFunction) {
                    std::string nextLine;
                    if (m_rstFile.PeekNextLine(nextLine) && !StabMatch(nextLine)) {
                        EndFunctionDefinition();
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
                    m_debugSymbols->AddSourceLocation(address, {m_currSourceFile, st.lineNum});

                }
                // Stop as soon as we hit any stab directive
                else if (StabMatch(line)) {
                    reparseCurrLine = true;
                    m_state = ParseDirectives{};
                }
            });
    }

    ASSERT_MSG(!m_currFunction,
               "Reached end of file before completing current function definition");

    m_debugSymbols->ResolveTypes([this](std::string id) -> std::shared_ptr<Type> {
        ASSERT(m_typeIdToType.count(id) != 0);
        return m_typeIdToType[id];
    });

    return true;
}

std::shared_ptr<Type> RstParser::FindType(const std::string& typeRefId,
                                          std::optional<std::string> varName) {
    auto iter = m_typeIdToType.find(typeRefId);

    // If type isn't found, create an UnresolvedType in its stead. These will be resolved later.
    if (iter == m_typeIdToType.end()) {
        auto type = std::make_unique<UnresolvedType>();
        type->id = typeRefId;
        return type;
    }

    return iter->second;
}

void RstParser::AddType(const std::string& typeDefId, std::shared_ptr<Type> type) {
    auto iter = m_typeIdToType.find(typeDefId);

    // If it's found, it must an unresolved type. We will replace it either with another
    // unresolved type, or with the actual type.
    if (iter != m_typeIdToType.end()) {
        ASSERT(std::dynamic_pointer_cast<UnresolvedType>(iter->second));
    }

    m_typeIdToType[typeDefId] = type;
    m_debugSymbols->AddType(std::move(type));
}

std::shared_ptr<Type> RstParser::AddPrimitiveType(const std::string& typeDefId,
                                                  const std::string& typeName, bool isSigned,
                                                  size_t byteSize) {
    auto t = std::make_shared<PrimitiveType>(typeName, isSigned, byteSize);
    AddType(typeDefId, t);
    return t;
}

std::shared_ptr<EnumType>
RstParser::AddEnumType(const std::string& typeDefId, const std::string& typeName, bool isSigned,
                       size_t byteSize, std::unordered_map<int64_t, std::string> valueToId) {
    auto t = std::make_shared<EnumType>(typeName, isSigned, byteSize, std::move(valueToId));
    AddType(typeDefId, t);
    return t;
}

std::shared_ptr<ArrayType> RstParser::AddArrayType(const std::string& typeDefId,
                                                   std::shared_ptr<Type> type, size_t numElems) {
    auto t = std::make_shared<ArrayType>(std::move(type), numElems);
    AddType(typeDefId, t);
    return t;
}

std::shared_ptr<StructType> RstParser::AddStructType(const std::string& typeDefId,
                                                     const std::string& typeName, size_t byteSize,
                                                     std::vector<StructType::Member> members) {
    auto t = std::make_shared<StructType>(typeName, byteSize, std::move(members));
    AddType(typeDefId, t);
    return t;
}

std::shared_ptr<IndirectType> RstParser::AddIndirectType(const std::string& typeDefId,
                                                         std::shared_ptr<Type> type) {
    auto t = std::make_shared<IndirectType>(std::move(type));
    AddType(typeDefId, t);
    return t;
}

void RstParser::AddVariable(std::string name, std::shared_ptr<Type> type, uint16_t stackOffset) {
    auto v = std::make_shared<Variable>();
    v->name = std::move(name);
    v->type = std::move(type);
    v->location = Variable::StackOffset{stackOffset};

    m_currVariables.push_back(std::move(v));
}

void RstParser::HandleStabStringMatch(StabStringMatch& stabs) {
    const int type = std::stoi(stabs.Type());

    // Source file name
    if (type == N_SOL) {
        m_currSourceFile = stabs.String();
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

        auto iter = m_labelToAddress.find(label);
        if (iter == m_labelToAddress.end()) {
            Printf("Warning! label not found: %s\n", label.c_str());
            return;
        }
        const uint16_t funcAddress = iter->second;
        m_debugSymbols->AddSymbol(Symbol{funcName, funcAddress});

        BeginFunctionDefinition(funcName, funcAddress);
    }

    // Local variable or type definition
    else if (type == N_LSYM) {
        const std::string lsymString = stabs.String();
        const std::string lsymValue = stabs.Value();

        // Struct type definitions
        if (auto lsymStruct = LSymStructMatch(lsymString)) {
            // Collect info for each member
            const auto values = lsymStruct.Values();

            std::vector<StructType::Member> members;
            members.reserve(values.Count());

            for (size_t i = 0; i < values.Count(); ++i) {

                StructType::Member member;

                // Parse member variables as we do regular variables. We expect
                // these to be either regular variable declarations, or pointer type
                // defintion and declaration.
                const auto memberLSym = values.LSym(i);

                if (auto lsymMemberVar = LSymMatch(memberLSym)) {
                    ASSERT(!lsymMemberVar.IsTypeDef()); // Is a variable declaration

                    auto memberType =
                        FindType(lsymMemberVar.VarTypeRefId(), lsymMemberVar.VarName());
                    if (!memberType)
                        return;

                    member.name = lsymMemberVar.VarName();
                    member.type = std::move(memberType);

                } else if (auto lsymMemberPointer = LSymPointerMatch(memberLSym)) {
                    // Find the type that this is a pointer to and create a new type
                    // for it
                    auto pointerType =
                        FindType(lsymMemberPointer.TypeRefId(), lsymMemberPointer.VarName());
                    if (!pointerType) {
                        // Skip this member
                        continue;
                    }

                    auto indirectType = AddIndirectType(lsymMemberPointer.TypeDefId(), pointerType);

                    member.name = lsymMemberPointer.VarName();
                    member.type = std::move(indirectType);

                } else {
                    FAIL_MSG("Unexpected match for member");
                }

                member.offsetBits = StringToSizeT(values.OffsetBits(i));
                member.sizeBits = StringToSizeT(values.SizeBits(i));

                members.push_back(member);
            }

            AddStructType(lsymStruct.TypeDefId(), lsymStruct.TypeName(),
                          StringToSizeT(lsymStruct.SizeBytes()), std::move(members));
        }
        // Type definition or variable declaration
        else if (auto lsym = LSymMatch(lsymString)) {
            // Type definition
            if (lsym.IsTypeDef()) {
                const auto typeName = lsym.TypeName();
                const auto typeDefId = lsym.TypeDefId();

                if (!lsym.HasRange()) {
                    // If it's an alias for an existing type, just skip it. We will
                    // always display the first type, not the alias type name. E.g.
                    // 'signed' is an alias for 'int', 'unsigned long' is an alias
                    // for 'unsigned long int'.
                    if (m_typeIdToType.find(typeDefId) != m_typeIdToType.end())
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
                        AddPrimitiveType(typeDefId, typeName, true, 1);
                    } else if (typeName == "void") {
                        AddPrimitiveType(typeDefId, typeName, false, 0);
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

                        auto numBits = static_cast<size_t>(::log2(upperBound - lowerBound + 1));

                        // HACK: sometimes the range will be something like [0,127],
                        // instead of [-128,127]. For these cases, let's assume it's
                        // signed, and increase number of bits until it's a valid
                        // 8-bit multiple.
                        if (numBits % 8 != 0) {
                            isSigned = true;
                            while (numBits % 8 != 0)
                                ++numBits;
                        }

                        AddPrimitiveType(typeDefId, typeName, isSigned, numBits / 8);
                    } else {
                        // Special case where lowerBound specifies the number of
                        // bytes
                        AddPrimitiveType(typeDefId, typeName, true,
                                         static_cast<size_t>(lowerBound));
                    }
                }
            }
            // Variable declaration
            else {
                // Note that the "value" part (the last number) of the stabs for
                // L_SYM local variables is the stack offset in bytes.
                // Ex:
                // 101 ;	.stabs	"c:7",128,0,0,1
                const auto stackOffset = lsymValue;

                auto varType = FindType(lsym.VarTypeRefId(), lsym.VarName());
                if (!varType)
                    return;

                AddVariable(lsym.VarName(), varType, DecToU16(stackOffset));
            }
        }
        // Pointer type definition AND variable declaration
        else if (auto lsymPointer = LSymPointerMatch(lsymString)) {
            // Find the type that this is a pointer to and create a new type for it
            auto pointerType = FindType(lsymPointer.TypeRefId(), lsymPointer.VarName());
            if (!pointerType)
                return;

            auto indirectType = AddIndirectType(lsymPointer.TypeDefId(), pointerType);

            const auto stackOffset = lsymValue;
            AddVariable(lsymPointer.VarName(), indirectType, DecToU16(stackOffset));
        }
        // Enum type definitions
        else if (auto lsymEnum = LSymEnumMatch(lsymString)) {
            auto minValue = std::numeric_limits<int64_t>::max();
            auto maxValue = std::numeric_limits<int64_t>::min();
            std::unordered_map<int64_t, std::string> valueToId;

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

                int64_t v = std::stoll(kvp[1]);
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
            const auto numBits = static_cast<size_t>(::log2(maxValue - minValue + 1));
            const size_t byteSize = (numBits + 7) / 8;

            AddEnumType(lsymEnum.TypeDefId(), lsymEnum.TypeName(), isSigned, byteSize,
                        std::move(valueToId));

        }

        else if (auto lsymArray = LSymArrayMatch(lsymString)) {

            // This will be set to the last dimension we parse, which will be the type of the array
            // variable.
            std::shared_ptr<ArrayType> arrayType;

            // Iterate through dimensions in reverse order to define each dimension as an array type
            const auto& dims = lsymArray.Dims();
            for (auto iter = dims.rbegin(); iter != dims.rend(); ++iter) {
                auto& dim = *iter;

                // For parsing, prefix a dummy var name and colon to be able to match.
                // TODO: do this better.
                auto arrayLSym = "dummy:" + dim.typeRefSym;

                // TODO: After parsing the first dimension, we don't need to match/find the next
                // type, as it should already be 'arrayType' from the last iteration.
                std::shared_ptr<Type> arrayElemType{};

                if (auto arrayLSymVar = LSymMatch(arrayLSym)) {
                    ASSERT(!arrayLSymVar.IsTypeDef()); // Is a variable declaration

                    arrayElemType = FindType(arrayLSymVar.VarTypeRefId(), arrayLSymVar.VarName());
                    if (!arrayElemType)
                        return;

                } else if (auto lsymPointer2 = LSymPointerMatch(arrayLSym)) {
                    // Find the type that this is a pointer to and create a new type
                    // for it
                    auto pointerType = FindType(lsymPointer2.TypeRefId(), lsymPointer2.VarName());
                    if (!pointerType) {
                        return;
                    }

                    auto indirectType = AddIndirectType(lsymPointer2.TypeDefId(), pointerType);
                    arrayElemType = indirectType;

                } else {
                    FAIL_MSG("Unexpected match for member");
                    return;
                }

                ASSERT(!arrayType || arrayType == arrayElemType);

                arrayType =
                    AddArrayType(dim.typeDefId, arrayElemType, StringToSizeT(dim.maxIndex) + 1);
            }

            // Finally, add the array variable
            const auto stackOffset = lsymValue;
            AddVariable(lsymArray.VarName(), arrayType, DecToU16(stackOffset));
        }
    }
}

void RstParser::HandleStabDotMatch(struct StabDotMatch& stabd) {
    const int type = std::stoi(stabd.Type());

    if (type == N_SLINE) {
        uint32_t lineNum = stoi(stabd.Desc());
        // If the compiler injects code (e.g. no return in main, compiler injects
        // "return 0;" instructions), the stab data says this code is at line "0" in
        // the file, because technically it's not in the file. We ignore these,
        // since they don't correspond to actual source locations.
        if (lineNum != 0) {
            m_state = ParseLineInstructions{lineNum};
        }
    }
}

void RstParser::HandleStabNumberMatch(struct StabNumberMatch& stabn) {
    const int type = std::stoi(stabn.Type());
    const std::string value = stabn.Value();

    if (type == N_LBRAC) {
        // Create a scope and transfer all variable declarations we've collected so
        // far
        auto scope = std::make_shared<Scope>();
        scope->variables = std::move(m_currVariables);

        const std::string label = value;
        auto iter = m_labelToAddress.find(label);
        if (iter == m_labelToAddress.end()) {
            FAIL_MSG("Warning! label not found: %s\n", label.c_str());
        }
        const uint16_t scopeStartAddress = iter->second;
        scope->range.first = scopeStartAddress;

        // Make current
        ASSERT(m_currScope); // Function always has a root scope
        m_currScope->AddChild(scope);
        m_currScope = scope;

    } else if (type == N_RBRAC) {
        // Set the end address for the closing scope
        const std::string label = value;
        auto iter = m_labelToAddress.find(label);
        if (iter == m_labelToAddress.end()) {
            FAIL_MSG("Warning! label not found: %s\n", label.c_str());
        }
        const uint16_t scopeEndAddress = iter->second;
        m_currScope->range.second = scopeEndAddress;

        // Set current scope to parent
        ASSERT(m_currScope);
        m_currScope = m_currScope->Parent();
        ASSERT(m_currScope); // Function always has a root scope
    }
}

void RstParser::BeginFunctionDefinition(const std::string& funcName, uint16_t funcAddress) {
    ASSERT(!m_currFunction);
    ASSERT(!m_currScope);

    m_currFunction = std::make_shared<Function>(funcName, funcAddress);

    // Always create a root scope, in case this function doesn't define one, for e.g.:
    // void foo() {
    //     { int a; }
    //     { int b; }
    // }
    // Because the root scope of 'foo' doesn't contain any variable declarations, the debug info
    // doesn't bother to add scope info. To simplify our parsing, we always create one.
    m_currFunction->scope = std::make_shared<Scope>();
    m_currScope = m_currFunction->scope;
}

void RstParser::EndFunctionDefinition() {
    ASSERT(m_currFunction);
    ASSERT(m_currScope);

    // Remove the dummy root scope if it's not necessary. It's only
    // necessary for functions that have no variables declared in the root
    // scope of the function.
    if (m_currFunction->scope->children.size() == 1) {
        m_currFunction->scope = m_currFunction->scope->children[0];
    }

    m_debugSymbols->AddFunction(std::move(m_currFunction));

    m_currFunction = {};
    m_currScope = {};
}
