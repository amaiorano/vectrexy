#include "Options.h"
#include "StringHelpers.h"
#include <fstream>
#include <iostream>
#include <string>

Options LoadOptionsFile(const char* file) {
    Options options;

    std::ifstream fin(file);
    if (!fin) {
        std::cerr << "No options file \"" << file << "\" found, using default values" << std::endl;
        return options;
    }

    std::string line;
    while (std::getline(fin, line)) {
        line = Trim(line);
        if (line[0] == ';') // Skip comments
            continue;

        auto tokens = Trim(Split(line, "="));
        if (tokens.size() < 2)
            continue;
        if (tokens[0] == "windowX")
            options.windowX = std::stoi(tokens[1]);
        else if (tokens[0] == "windowY")
            options.windowY = std::stoi(tokens[1]);
        else if (tokens[0] == "windowWidth")
            options.windowWidth = std::stoi(tokens[1]);
        else if (tokens[0] == "windowHeight")
            options.windowHeight = std::stoi(tokens[1]);
        else if (tokens[0] == "imguiDebugWindow")
            options.imguiDebugWindow = tokens[1] != "0";
        else if (tokens[0] == "imguiFontScale")
            options.imguiFontScale = std::stof(tokens[1]);
        else {
            std::cerr << "Unknown option: " << tokens[0] << std::endl;
        }
    }
    return options;
}
