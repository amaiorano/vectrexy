#pragma once

#include "core/FileSystem.h"
#include "debugger/DebugSymbols.h"
#include "debugger/RstFile.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class DebugSymbols;

class RstParser {
public:
    RstParser(DebugSymbols& debugSymbols);
    bool Parse(const fs::path& rstFilePath);

private:
    std::shared_ptr<Type> FindType(const std::string& typeRefId,
                                   std::optional<std::string> varName = {});
    void AddType(const std::string& typeDefId, std::shared_ptr<Type> type);

    std::shared_ptr<Type> AddPrimitiveType(const std::string& typeDefId,
                                           const std::string& typeName, bool isSigned,
                                           size_t byteSize);
    std::shared_ptr<EnumType> AddEnumType(const std::string& typeDefId, const std::string& typeName,
                                          bool isSigned, size_t byteSize,
                                          std::unordered_map<int64_t, std::string> valueToId);
    std::shared_ptr<ArrayType> AddArrayType(const std::string& typeDefId,
                                            std::shared_ptr<Type> arrayType, size_t numElems);
    std::shared_ptr<StructType> AddStructType(const std::string& typeDefId,
                                              const std::string& typeName, size_t byteSize,
                                              std::vector<StructType::Member> members);
    std::shared_ptr<IndirectType> AddIndirectType(const std::string& typeDefId,
                                                  std::shared_ptr<Type> type);
    void AddVariable(std::string name, std::shared_ptr<Type> type, uint16_t stackOffset);

    void HandleStabStringMatch(struct StabStringMatch& stabs);
    void HandleStabDotMatch(struct StabDotMatch& stabd);
    void HandleStabNumberMatch(struct StabNumberMatch& stabn);

    void BeginFunctionDefinition(const std::string& funcName, uint16_t funcAddress);
    void EndFunctionDefinition();

    DebugSymbols* m_debugSymbols{};
    RstFile m_rstFile;

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
