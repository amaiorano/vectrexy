#pragma once

#include <regex>
#include <string>

struct MatchBase {
    MatchBase(const char* re, std::string s)
        : m_s(std::move(s))
        , m_re(re) {
        m_matched = std::regex_match(m_s, m_match, m_re);
    }

    operator bool() const { return m_matched; }

protected:
    std::string m_s;
    std::regex m_re;
    std::smatch m_match;
    bool m_matched{};
};

struct MultiMatchBase {
    MultiMatchBase(const char* re, std::string s)
        : m_s(std::move(s))
        , m_re(re) {

        auto words_begin = std::sregex_iterator(m_s.begin(), m_s.end(), m_re);
        auto words_end = std::sregex_iterator();

        m_matches.reserve(std::distance(words_begin, words_end));
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            m_matches.push_back(*i);
        }
    }

    operator bool() const { return !m_matches.empty(); }

protected:
    std::string m_s;
    std::regex m_re;
    std::vector<std::smatch> m_matches;
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
        : MatchBase(R"([[:space:]]*([0-9|A-F][0-9|A-F][0-9|A-F][0-9|A-F])[[:space:]]*.*\[..\].*)",
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
        : MatchBase(R"((.*):([t]*)([0-9]+)(=[rR]([0-9]+);((?:-|)[0-9]+);((?:-|)[0-9]+);|))", s) {}

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
        : MatchBase(R"((.*):([0-9]+)=\*([0-9]+)(=.*:)*)", s) {}

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
// 3: values (comma-separated key:value pairs)
struct LSymEnumMatch : MatchBase {
    LSymEnumMatch(const std::string& s)
        : MatchBase(R"((.*):t([0-9]+)=e(.*),;)", s) {}

    std::string TypeName() const { return m_match[1].str(); }
    std::string TypeDefId() const { return m_match[2].str(); }
    std::string Values() const { return m_match[3].str(); }
};

// Match stabs type string for N_LSYM: array variable declarations
// int i[1];        i:25=ar26=r26;0;-1;;0;0;7
// char c[2];       c:27=ar26;0;1;13
// bool b[3];       b:28=ar26;0;2;22
// int* pi[4];      pi:29=ar26;0;3;30=*7
//
// 1: variable name
// 2: type def #
// 3: index upper-bound
// 4: lsym type (not the entire lsym, e.g. "7" instead of "i:7" or "30=*7" instead of "pi:30=*7"
struct LSymArrayMatch : MatchBase {
    LSymArrayMatch(const std::string& s)
        : MatchBase(R"((.*):([0-9]+)=ar[0-9]+(?:=r.*?;.*?;.*?;|);.*?;(.*?);(.*))", s) {}

    std::string VarName() const { return m_match[1].str(); }
    std::string TypeDefId() const { return m_match[2].str(); }
    std::string IndexUpperBound() const { return m_match[3].str(); }
    std::string LSymType() const { return m_match[4].str(); }
};

// Match stabs type string for N_LSYM: struct/class type definitions
// "Foo:T26=s4a:7,0,8;b:7,8,8;c:7,16,8;d:7,24,6;e:7,30,2;;
//
// 1: type name
// 2: type def #
// 3: total byte size of struct
// 4: values (semicolon-separated key:value pairs)
struct LSymStructMatch : MatchBase {
    LSymStructMatch(const std::string& s)
        : MatchBase(R"((.*):T([0-9]+)=s([0-9]+)(.*);)", s) {}

    // Splits out the array of values
    // 1: lsym string
    // 2: offset in bits
    // 3: size in bits
    // "a:7,0,8;b:7,8,8;c:7,16,8;d:7,24,6;e:7,30,2;p:28=*7,88,16;"
    struct ValueMatch : MultiMatchBase {
        ValueMatch(const std::string& s)
            : MultiMatchBase(R"((.*?),(.*?),(.*?);)", s) {}

        size_t Count() const { return m_matches.size(); }

        // "a:7"
        // "p:28=*7"
        std::string LSym(size_t i) const { return m_matches[i][1].str(); }

        std::string OffsetBits(size_t i) const { return m_matches[i][2].str(); }
        std::string SizeBits(size_t i) const { return m_matches[i][3].str(); }
    };

    std::string TypeName() const { return m_match[1].str(); }
    std::string TypeDefId() const { return m_match[2].str(); }
    std::string SizeBytes() const { return m_match[3].str(); }
    ValueMatch Values() const { return ValueMatch{m_match[4].str()}; }
};
