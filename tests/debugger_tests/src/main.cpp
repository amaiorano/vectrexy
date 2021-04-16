#include "core/FileSystem.h"
#include "debugger/RstParser.h"
#include "subprocess/subprocess.h"
#include <fstream>

#undef FAIL
#include "gtest/gtest.h"

namespace {
    void StringToFile(const std::string& s, fs::path path) {
        std::ofstream fout(path);
        fout << s;
    }

    bool Exec(std::vector<const char*> command, bool showOutput) {
        command.push_back(0); // Add terminator
        struct subprocess_s subprocess;
        int result = subprocess_create(command.data(), subprocess_option_combined_stdout_stderr,
                                       &subprocess);
        if (result != 0)
            return {};

        FILE* p_stdout = subprocess_stdout(&subprocess);
        if (showOutput) {
            char outputText[1024];
            while (fgets(outputText, sizeof(outputText), p_stdout)) {
                std::cout << "stdout/stderr> " << outputText;
            }
        }

        int process_return;
        result = subprocess_join(&subprocess, &process_return);
        if (result != 0)
            return false;
        if (process_return != 0) {
            std::cerr << "Process returned unexpected value: " << process_return << std::endl;
            return false;
        }

        result = subprocess_destroy(&subprocess);
        if (result != 0)
            return false;

        return true;
    }

    fs::path CompileSource(const char* source, bool showOutput) {
        // Find template dir in current path and up to root. This works as long as we're running out
        // of a subdirectory of the project folder.
        auto findBuildTemplateDir = []() -> fs::path {
            auto templatePath = fs::path{"tests/debugger_tests/build-template"};
            while (true) {
                if (fs::exists(templatePath)) {
                    return (fs::current_path() / templatePath).make_preferred();
                }
                fs::current_path("..");
            }
            return {};
        };

        const auto sourceBuildTemplateDir = findBuildTemplateDir();
        const auto tempDir = fs::temp_directory_path() / "vectrexy-unit-tests";
        const auto buildDir = tempDir / "build-template";

        fs::create_directories(buildDir);

        fs::copy(sourceBuildTemplateDir, buildDir,
                 fs::copy_options::update_existing | fs::copy_options::recursive);

        StringToFile(source, buildDir / "src/main.cpp");

        Exec({"docker", "run", "-v",
              FormattedString("%s:/root/build-template", buildDir.string().c_str()),
              "amaiorano/gcc6809", "make", "DEBUG=1", "-C", "build-template"},
             showOutput);

        auto rstFile = buildDir / "main.rst";
        if (!fs::exists(rstFile)) {
            std::cerr << "Compiled listing file not found: " << rstFile << std::endl;
            return {};
        }

        return rstFile;
    }

    std::shared_ptr<Variable> FindVariableByName(const std::shared_ptr<const Function>& function,
                                                 const char* name) {

        return Traverse(
            function->scope, [&](const std::shared_ptr<Scope>& node) -> std::shared_ptr<Variable> {
                auto iter = std::find_if(node->variables.begin(), node->variables.end(),
                                         [&](auto variable) { return variable->name == name; });
                if (iter != node->variables.end()) {
                    return *iter;
                }
                return {};
            });
    }

} // namespace

TEST(StabsParser, PrimitiveTypes) {

    const char* source = R"(
int main() {
    // Primitive types
    float f = 1.0f;
    double d = 2.0;
    long double ld = 3.0;
    char c = 127;
    signed char sc = 127;
    unsigned char uc = 255;
    int i = 127;
    unsigned int ui = 255;
    long unsigned int lui = 65535u;
    long long unsigned int llui = 0xFFFF;
    long long int lli = 0xFFFF;
    short int si = 127;
    short unsigned int sui = 255;

    // Alias types
    signed s = 127;
    unsigned long ul = 255;
    long long unsigned llu = 0xFFFF;
    unsigned short us = 255;

    return 0;
}
)";
    auto rstFile = CompileSource(source, false);
    ASSERT_TRUE(!rstFile.empty());

    auto symbols = std::make_unique<DebugSymbols>();
    auto parser = std::make_unique<RstParser>(*symbols);
    ASSERT_TRUE(parser->Parse(rstFile));

    auto* mainSymbol = symbols->GetSymbolByName("main()");
    ASSERT_TRUE(mainSymbol);
    auto mainFunction = symbols->GetFunctionByAddress(mainSymbol->address);
    ASSERT_TRUE(mainFunction);

    // Should have a scope since there are local variables
    EXPECT_TRUE(mainFunction->scope);

    struct TestParams {
        const char* name;
        const char* typeName;
        bool isSigned;
        size_t byteSize;
        PrimitiveType::Format format;
    };

    TestParams testParams[] = {
        {"f", "float", true, 4, PrimitiveType::Format::Float},
        {"d", "double", true, 4, PrimitiveType::Format::Float},
        {"ld", "long double", true, 4, PrimitiveType::Format::Float},
        {"c", "char", true, 1, PrimitiveType::Format::Char},
        {"sc", "signed char", true, 1, PrimitiveType::Format::Char},
        {"uc", "unsigned char", false, 1, PrimitiveType::Format::Char},
        {"i", "int", true, 1, PrimitiveType::Format::Int},
        {"ui", "unsigned int", false, 1, PrimitiveType::Format::Int},
        {"lui", "long unsigned int", false, 2, PrimitiveType::Format::Int},
        {"llui", "long long unsigned int", false, 4, PrimitiveType::Format::Int},
        {"lli", "long long int", true, 4, PrimitiveType::Format::Int},
        {"si", "short int", true, 1, PrimitiveType::Format::Int},
        {"sui", "short unsigned int", false, 1, PrimitiveType::Format::Int},
        // Alias types
        {"s", "int", true, 1, PrimitiveType::Format::Int},
        {"ul", "long unsigned int", false, 2, PrimitiveType::Format::Int},
        {"llu", "long long unsigned int", false, 4, PrimitiveType::Format::Int},
        {"us", "short unsigned int", false, 1, PrimitiveType::Format::Int},
        // TODO: add "void" and test with a void*
    };

    for (auto& p : testParams) {
        SCOPED_TRACE(p.name);

        auto var = FindVariableByName(mainFunction, p.name);
        ASSERT_TRUE(var);
        EXPECT_EQ(var->name, p.name);
        EXPECT_EQ(var->type->name, p.typeName);

        auto primType = std::dynamic_pointer_cast<PrimitiveType>(var->type);
        ASSERT_TRUE(primType);
        EXPECT_EQ(primType->isSigned, p.isSigned);
        EXPECT_EQ(primType->byteSize, p.byteSize);
        EXPECT_EQ(primType->format, p.format);
    }
}

TEST(StabsParser, IndirectTypes) {

    const char* source = R"(
int main() {
    int a;
    int* p1 = &a;
    int* p2 = p1;
    int& r1 = a;
    int& r2 = r1;
    int** pp = &p1;
    int*** ppp = &pp;
    int***& rppp = ppp;

    const int* cp1 = &a;

    // TODO: pointer to class/struct
    // TODO: pointer to function

    return 0;
}
)";
    auto rstFile = CompileSource(source, false);
    ASSERT_TRUE(!rstFile.empty());

    auto symbols = std::make_unique<DebugSymbols>();
    auto parser = std::make_unique<RstParser>(*symbols);
    ASSERT_TRUE(parser->Parse(rstFile));

    auto* mainSymbol = symbols->GetSymbolByName("main()");
    ASSERT_TRUE(mainSymbol);
    auto mainFunction = symbols->GetFunctionByAddress(mainSymbol->address);
    ASSERT_TRUE(mainFunction);

    // Should have a scope since there are local variables
    EXPECT_TRUE(mainFunction->scope);

    struct TestParams {
        const char* name;
        const char* typeName;
        const std::type_info& pointeeTypeInfo;
        const char* pointeeName;
    };

    TestParams testParams[] = {
        "p1",   "int*",    typeid(PrimitiveType), "int",
        "p2",   "int*",    typeid(PrimitiveType), "int",
        "r1",   "int*",    typeid(PrimitiveType), "int",
        "r2",   "int*",    typeid(PrimitiveType), "int",
        "pp",   "int**",   typeid(IndirectType),  "int*",
        "ppp",  "int***",  typeid(IndirectType),  "int**",
        "rppp", "int****", typeid(IndirectType),  "int***",
    };

    for (auto& p : testParams) {
        SCOPED_TRACE(p.name);
        auto var = FindVariableByName(mainFunction, p.name);
        ASSERT_TRUE(var);
        EXPECT_EQ(var->name, p.name);
        EXPECT_EQ(var->type->name, p.typeName);

        auto indirectType = std::dynamic_pointer_cast<IndirectType>(var->type);
        ASSERT_TRUE(indirectType);

        EXPECT_EQ(typeid(*indirectType->type), p.pointeeTypeInfo);
        EXPECT_EQ(indirectType->type->name, p.pointeeName);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
