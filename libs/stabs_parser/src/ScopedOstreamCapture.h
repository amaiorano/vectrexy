#pragma once

#include <iostream>
#include <sstream>
#include <string>

// RAII object that while alive, captures std::cout and std::cerr output, allowing it to be returned
// as a string.
class ScopedOstreamCapture {
public:
    ScopedOstreamCapture() {
        m_oldCout = std::cout.rdbuf(m_buffer.rdbuf());
        m_oldCerr = std::cerr.rdbuf(m_buffer.rdbuf());
    }

    ~ScopedOstreamCapture() {
        std::cout.rdbuf(m_oldCout);
        std::cerr.rdbuf(m_oldCerr);
    }

    std::string str() const { return m_buffer.str(); }

private:
    std::stringstream m_buffer;
    std::streambuf* m_oldCout;
    std::streambuf* m_oldCerr;
};
