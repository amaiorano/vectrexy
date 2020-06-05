#pragma once

#include "core/FileSystem.h"
#include "debugger/DebugSymbols.h"
#include <fstream>
#include <memory>
#include <unordered_map>
#include <vector>

class DebugSymbols;

class RstParser {
public:
    RstParser(DebugSymbols& debugSymbols);
    bool Parse(const fs::path& rstFile);

private:
    bool GetLine(std::ifstream& fin, std::string& line);

    std::shared_ptr<Type> FindType(const std::string& typeRefId,
                                   std::optional<std::string> varName = {});

    std::shared_ptr<Type> AddPrimitiveType(const std::string& typeDefId,
                                           const std::string& typeName, bool isSigned,
                                           size_t byteSize);
    std::shared_ptr<EnumType> AddEnumType(const std::string& typeDefId, const std::string& typeName,
                                          bool isSigned, size_t byteSize,
                                          std::unordered_map<ssize_t, std::string> valueToId);
    std::shared_ptr<StructType> AddStructType(const std::string& typeDefId,
                                              const std::string& typeName,
                                              std::vector<StructType::Member> members);
    std::shared_ptr<IndirectType> AddIndirectType(const std::string& typeDefId,
                                                  std::shared_ptr<Type> type);
    void AddVariable(std::string name, std::shared_ptr<Type> type, uint16_t stackOffset);

    void HandleStabStringMatch(struct StabStringMatch& stabs);
    void HandleStabDotMatch(struct StabDotMatch& stabd);
    void HandleStabNumberMatch(struct StabNumberMatch& stabn);

    DebugSymbols* m_debugSymbols{};

    struct ParseDirectives {};
    struct ParseLineInstructions {
        uint32_t lineNum{};
    };

    std::variant<ParseDirectives, ParseLineInstructions> m_state{ParseDirectives{}};

    std::unordered_map<std::string, uint16_t> m_labelToAddress;
    std::unordered_map<std::string, std::shared_ptr<Type>> m_typeIdToType;

    std::string m_currSourceFile;
    std::shared_ptr<Function> m_currFunction;
    std::shared_ptr<Scope> m_currScope{};
    std::vector<std::shared_ptr<Variable>> m_currVariables;
};
