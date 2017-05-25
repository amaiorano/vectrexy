#pragma once
#include <cstdint>

enum class AddressingMode {
    Relative,
    Inherent,
    Immediate,
    Direct,
    Indexed,
    Extended,
    Illegal,
    Variant,
};

struct CpuOp {
    uint8_t opCode;
    const char* name;
    AddressingMode addrMode;
    int cycles;
    int size;
};

constexpr CpuOp CpuOpsPage0[] = {
    // clang-format off
    { 0x00, "NEG      ", AddressingMode::Direct   ,  6, 2 },
    { 0x01, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x02, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x03, "COM      ", AddressingMode::Direct   ,  6, 2 },
    { 0x04, "LSR      ", AddressingMode::Direct   ,  6, 2 },
    { 0x05, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x06, "ROR      ", AddressingMode::Direct   ,  6, 2 },
    { 0x07, "ASR      ", AddressingMode::Direct   ,  6, 2 },
    { 0x08, "LSL/ASL  ", AddressingMode::Direct   ,  6, 2 },
    { 0x09, "ROL      ", AddressingMode::Direct   ,  6, 2 },
    { 0x0A, "DEC      ", AddressingMode::Direct   ,  6, 2 },
    { 0x0B, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x0C, "INC      ", AddressingMode::Direct   ,  6, 2 },
    { 0x0D, "TST      ", AddressingMode::Direct   ,  6, 2 },
    { 0x0E, "JMP      ", AddressingMode::Direct   ,  3, 2 },
    { 0x0F, "CLR      ", AddressingMode::Direct   ,  6, 2 },
    { 0x10, "PAGE1+   ", AddressingMode::Variant  ,  1, 1 },
    { 0x11, "PAGE2+   ", AddressingMode::Variant  ,  1, 1 },
    { 0x12, "NOP      ", AddressingMode::Inherent ,  2, 1 },
    { 0x13, "SYNC     ", AddressingMode::Inherent ,  2, 1 },
    { 0x14, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x15, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x16, "LBRA     ", AddressingMode::Relative ,  5, 3 },
    { 0x17, "LBSR     ", AddressingMode::Relative ,  9, 3 },
    { 0x18, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x19, "DAA      ", AddressingMode::Inherent ,  2, 1 },
    { 0x1A, "ORCC     ", AddressingMode::Immediate,  3, 2 },
    { 0x1B, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x1C, "ANDCC    ", AddressingMode::Immediate,  3, 2 },
    { 0x1D, "SEX      ", AddressingMode::Inherent ,  2, 1 },
    { 0x1E, "EXG      ", AddressingMode::Inherent ,  8, 2 },
    { 0x1F, "TFR      ", AddressingMode::Inherent ,  7, 2 },
    { 0x20, "BRA      ", AddressingMode::Relative ,  3, 2 },
    { 0x21, "BRN      ", AddressingMode::Relative ,  3, 2 },
    { 0x22, "BHI      ", AddressingMode::Relative ,  3, 2 },
    { 0x23, "BLS      ", AddressingMode::Relative ,  3, 2 },
    { 0x24, "BHS/BCC  ", AddressingMode::Relative ,  3, 2 },
    { 0x25, "BLO/BCS  ", AddressingMode::Relative ,  3, 2 },
    { 0x26, "BNE      ", AddressingMode::Relative ,  3, 2 },
    { 0x27, "BEQ      ", AddressingMode::Relative ,  3, 2 },
    { 0x28, "BVC      ", AddressingMode::Relative ,  3, 2 },
    { 0x29, "BVS      ", AddressingMode::Relative ,  3, 2 },
    { 0x2A, "BPL      ", AddressingMode::Relative ,  3, 2 },
    { 0x2B, "BMI      ", AddressingMode::Relative ,  3, 2 },
    { 0x2C, "BGE      ", AddressingMode::Relative ,  3, 2 },
    { 0x2D, "BLT      ", AddressingMode::Relative ,  3, 2 },
    { 0x2E, "BGT      ", AddressingMode::Relative ,  3, 2 },
    { 0x2F, "BLE      ", AddressingMode::Relative ,  3, 2 },
    { 0x30, "LEAX     ", AddressingMode::Indexed  ,  4, 2 },
    { 0x31, "LEAY     ", AddressingMode::Indexed  ,  4, 2 },
    { 0x32, "LEAS     ", AddressingMode::Indexed  ,  4, 2 },
    { 0x33, "LEAU     ", AddressingMode::Indexed  ,  4, 2 },
    { 0x34, "PSHS     ", AddressingMode::Inherent ,  5, 2 },
    { 0x35, "PULS     ", AddressingMode::Inherent ,  5, 2 },
    { 0x36, "PSHU     ", AddressingMode::Inherent ,  5, 2 },
    { 0x37, "PULU     ", AddressingMode::Inherent ,  5, 2 },
    { 0x38, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x39, "RTS      ", AddressingMode::Inherent ,  5, 1 },
    { 0x3A, "ABX      ", AddressingMode::Inherent ,  3, 1 },
    { 0x3B, "RTI      ", AddressingMode::Inherent , -1, 1 },
    { 0x3C, "CWAI     ", AddressingMode::Inherent , 21, 2 },
    { 0x3D, "MUL      ", AddressingMode::Inherent , 11, 1 },
    { 0x3E, "RESET*   ", AddressingMode::Inherent , -1, 1 },
    { 0x3F, "SWI      ", AddressingMode::Inherent , 19, 1 },
    { 0x40, "NEGA     ", AddressingMode::Inherent ,  2, 1 },
    { 0x41, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x42, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x43, "COMA     ", AddressingMode::Inherent ,  2, 1 },
    { 0x44, "LSRA     ", AddressingMode::Inherent ,  2, 1 },
    { 0x45, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x46, "RORA     ", AddressingMode::Inherent ,  2, 1 },
    { 0x47, "ASRA     ", AddressingMode::Inherent ,  2, 1 },
    { 0x48, "LSLA/AS  ", AddressingMode::Inherent ,  2, 1 },
    { 0x49, "ROLA     ", AddressingMode::Inherent ,  2, 1 },
    { 0x4A, "DECA     ", AddressingMode::Inherent ,  2, 1 },
    { 0x4B, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x4C, "INCA     ", AddressingMode::Inherent ,  2, 1 },
    { 0x4D, "TSTA     ", AddressingMode::Inherent ,  2, 1 },
    { 0x4E, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x4F, "CLRA     ", AddressingMode::Inherent ,  2, 1 },
    { 0x50, "NEGB     ", AddressingMode::Inherent ,  2, 1 },
    { 0x51, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x52, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x53, "COMB     ", AddressingMode::Inherent ,  2, 1 },
    { 0x54, "LSRB     ", AddressingMode::Inherent ,  2, 1 },
    { 0x55, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x56, "RORB     ", AddressingMode::Inherent ,  2, 1 },
    { 0x57, "ASRB     ", AddressingMode::Inherent ,  2, 1 },
    { 0x58, "LSLB/AS  ", AddressingMode::Inherent ,  2, 1 },
    { 0x59, "ROLB     ", AddressingMode::Inherent ,  2, 1 },
    { 0x5A, "DECB     ", AddressingMode::Inherent ,  2, 1 },
    { 0x5B, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x5C, "INCB     ", AddressingMode::Inherent ,  2, 1 },
    { 0x5D, "TSTB     ", AddressingMode::Inherent ,  2, 1 },
    { 0x5E, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x5F, "CLRB     ", AddressingMode::Inherent ,  2, 1 },
    { 0x60, "NEG      ", AddressingMode::Indexed  ,  6, 2 },
    { 0x61, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x62, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x63, "COM      ", AddressingMode::Indexed  ,  6, 2 },
    { 0x64, "LSR      ", AddressingMode::Indexed  ,  6, 2 },
    { 0x65, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x66, "ROR      ", AddressingMode::Indexed  ,  6, 2 },
    { 0x67, "ASR      ", AddressingMode::Indexed  ,  6, 2 },
    { 0x68, "LSL/ASL  ", AddressingMode::Indexed  ,  6, 2 },
    { 0x69, "ROL      ", AddressingMode::Indexed  ,  6, 2 },
    { 0x6A, "DEC      ", AddressingMode::Indexed  ,  6, 2 },
    { 0x6B, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x6C, "INC      ", AddressingMode::Indexed  ,  6, 2 },
    { 0x6D, "TST      ", AddressingMode::Indexed  ,  6, 2 },
    { 0x6E, "JMP      ", AddressingMode::Indexed  ,  3, 2 },
    { 0x6F, "CLR      ", AddressingMode::Indexed  ,  6, 2 },
    { 0x70, "NEG      ", AddressingMode::Extended ,  7, 3 },
    { 0x71, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x72, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x73, "COM      ", AddressingMode::Extended ,  7, 3 },
    { 0x74, "LSR      ", AddressingMode::Extended ,  7, 3 },
    { 0x75, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x76, "ROR      ", AddressingMode::Extended ,  7, 3 },
    { 0x77, "ASR      ", AddressingMode::Extended ,  7, 3 },
    { 0x78, "LSL/ASL  ", AddressingMode::Extended ,  7, 3 },
    { 0x79, "ROL      ", AddressingMode::Extended ,  7, 3 },
    { 0x7A, "DEC      ", AddressingMode::Extended ,  7, 3 },
    { 0x7B, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x7C, "INC      ", AddressingMode::Extended ,  7, 3 },
    { 0x7D, "TST      ", AddressingMode::Extended ,  7, 3 },
    { 0x7E, "JMP      ", AddressingMode::Extended ,  3, 3 },
    { 0x7F, "CLR      ", AddressingMode::Extended ,  7, 3 },
    { 0x80, "SUBA     ", AddressingMode::Immediate,  2, 2 },
    { 0x81, "CMPA     ", AddressingMode::Immediate,  2, 2 },
    { 0x82, "SBCA     ", AddressingMode::Immediate,  2, 2 },
    { 0x83, "SUBD     ", AddressingMode::Immediate,  4, 3 },
    { 0x84, "ANDA     ", AddressingMode::Immediate,  2, 2 },
    { 0x85, "BITA     ", AddressingMode::Immediate,  2, 2 },
    { 0x86, "LDA      ", AddressingMode::Immediate,  2, 2 },
    { 0x87, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x88, "EORA     ", AddressingMode::Immediate,  2, 2 },
    { 0x89, "ADCA     ", AddressingMode::Immediate,  2, 2 },
    { 0x8A, "ORA      ", AddressingMode::Immediate,  2, 2 },
    { 0x8B, "ADDA     ", AddressingMode::Immediate,  2, 2 },
    { 0x8C, "CMPX     ", AddressingMode::Immediate,  4, 3 },
    { 0x8D, "BSR      ", AddressingMode::Relative ,  7, 2 },
    { 0x8E, "LDX      ", AddressingMode::Immediate,  3, 3 },
    { 0x8F, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0x90, "SUBA     ", AddressingMode::Direct   ,  4, 2 },
    { 0x91, "CMPA     ", AddressingMode::Direct   ,  4, 2 },
    { 0x92, "SBCA     ", AddressingMode::Direct   ,  4, 2 },
    { 0x93, "SUBD     ", AddressingMode::Direct   ,  6, 2 },
    { 0x94, "ANDA     ", AddressingMode::Direct   ,  4, 2 },
    { 0x95, "BITA     ", AddressingMode::Direct   ,  4, 2 },
    { 0x96, "LDA      ", AddressingMode::Direct   ,  4, 2 },
    { 0x97, "STA      ", AddressingMode::Direct   ,  4, 2 },
    { 0x98, "EORA     ", AddressingMode::Direct   ,  4, 2 },
    { 0x99, "ADCA     ", AddressingMode::Direct   ,  4, 2 },
    { 0x9A, "ORA      ", AddressingMode::Direct   ,  4, 2 },
    { 0x9B, "ADDA     ", AddressingMode::Direct   ,  4, 2 },
    { 0x9C, "CMPX     ", AddressingMode::Direct   ,  6, 2 },
    { 0x9D, "JSR      ", AddressingMode::Direct   ,  7, 2 },
    { 0x9E, "LDX      ", AddressingMode::Direct   ,  5, 2 },
    { 0x9F, "STX      ", AddressingMode::Direct   ,  5, 2 },
    { 0xA0, "SUBA     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xA1, "CMPA     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xA2, "SBCA     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xA3, "SUBD     ", AddressingMode::Indexed  ,  6, 2 },
    { 0xA4, "ANDA     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xA5, "BITA     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xA6, "LDA      ", AddressingMode::Indexed  ,  4, 2 },
    { 0xA7, "STA      ", AddressingMode::Indexed  ,  4, 2 },
    { 0xA8, "EORA     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xA9, "ADCA     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xAA, "ORA      ", AddressingMode::Indexed  ,  4, 2 },
    { 0xAB, "ADDA     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xAC, "CMPX     ", AddressingMode::Indexed  ,  6, 2 },
    { 0xAD, "JSR      ", AddressingMode::Indexed  ,  7, 2 },
    { 0xAE, "LDX      ", AddressingMode::Indexed  ,  5, 2 },
    { 0xAF, "STX      ", AddressingMode::Indexed  ,  5, 2 },
    { 0xB0, "SUBA     ", AddressingMode::Extended ,  5, 3 },
    { 0xB1, "CMPA     ", AddressingMode::Extended ,  5, 3 },
    { 0xB2, "SBCA     ", AddressingMode::Extended ,  5, 3 },
    { 0xB3, "SUBD     ", AddressingMode::Extended ,  7, 3 },
    { 0xB4, "ANDA     ", AddressingMode::Extended ,  5, 3 },
    { 0xB5, "BITA     ", AddressingMode::Extended ,  5, 3 },
    { 0xB6, "LDA      ", AddressingMode::Extended ,  5, 3 },
    { 0xB7, "STA      ", AddressingMode::Extended ,  5, 3 },
    { 0xB8, "EORA     ", AddressingMode::Extended ,  5, 3 },
    { 0xB9, "ADCA     ", AddressingMode::Extended ,  5, 3 },
    { 0xBA, "ORA      ", AddressingMode::Extended ,  5, 3 },
    { 0xBB, "ADDA     ", AddressingMode::Extended ,  5, 3 },
    { 0xBC, "CMPX     ", AddressingMode::Extended ,  7, 3 },
    { 0xBD, "JSR      ", AddressingMode::Extended ,  8, 3 },
    { 0xBE, "LDX      ", AddressingMode::Extended ,  6, 3 },
    { 0xBF, "STX      ", AddressingMode::Extended ,  6, 3 },
    { 0xC0, "SUBB     ", AddressingMode::Immediate,  2, 2 },
    { 0xC1, "CMPB     ", AddressingMode::Immediate,  2, 2 },
    { 0xC2, "SBCB     ", AddressingMode::Immediate,  2, 2 },
    { 0xC3, "ADDD     ", AddressingMode::Immediate,  4, 3 },
    { 0xC4, "ANDB     ", AddressingMode::Immediate,  2, 2 },
    { 0xC5, "BITB     ", AddressingMode::Immediate,  2, 2 },
    { 0xC6, "LDB      ", AddressingMode::Immediate,  2, 2 },
    { 0xC7, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0xC8, "EORB     ", AddressingMode::Immediate,  2, 2 },
    { 0xC9, "ADCB     ", AddressingMode::Immediate,  2, 2 },
    { 0xCA, "ORB      ", AddressingMode::Immediate,  2, 2 },
    { 0xCB, "ADDB     ", AddressingMode::Immediate,  2, 2 },
    { 0xCC, "LDD      ", AddressingMode::Immediate,  3, 3 },
    { 0xCD, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0xCE, "LDU      ", AddressingMode::Immediate,  3, 3 },
    { 0xCF, "Illegal  ", AddressingMode::Illegal  ,  1, 1 },
    { 0xD0, "SUBB     ", AddressingMode::Direct   ,  4, 2 },
    { 0xD1, "CMPB     ", AddressingMode::Direct   ,  4, 2 },
    { 0xD2, "SBCB     ", AddressingMode::Direct   ,  4, 2 },
    { 0xD3, "ADDD     ", AddressingMode::Direct   ,  6, 2 },
    { 0xD4, "ANDB     ", AddressingMode::Direct   ,  4, 2 },
    { 0xD5, "BITB     ", AddressingMode::Direct   ,  4, 2 },
    { 0xD6, "LDB      ", AddressingMode::Direct   ,  4, 2 },
    { 0xD7, "STB      ", AddressingMode::Direct   ,  4, 2 },
    { 0xD8, "EORB     ", AddressingMode::Direct   ,  4, 2 },
    { 0xD9, "ADCB     ", AddressingMode::Direct   ,  4, 2 },
    { 0xDA, "ORB      ", AddressingMode::Direct   ,  4, 2 },
    { 0xDB, "ADDB     ", AddressingMode::Direct   ,  4, 2 },
    { 0xDC, "LDD      ", AddressingMode::Direct   ,  5, 2 },
    { 0xDD, "STD      ", AddressingMode::Direct   ,  5, 2 },
    { 0xDE, "LDU      ", AddressingMode::Direct   ,  5, 2 },
    { 0xDF, "STU      ", AddressingMode::Direct   ,  5, 2 },
    { 0xE0, "SUBB     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xE1, "CMPB     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xE2, "SBCB     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xE3, "ADDD     ", AddressingMode::Indexed  ,  6, 2 },
    { 0xE4, "ANDB     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xE5, "BITB     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xE6, "LDB      ", AddressingMode::Indexed  ,  4, 2 },
    { 0xE7, "STB      ", AddressingMode::Indexed  ,  4, 2 },
    { 0xE8, "EORB     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xE9, "ADCB     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xEA, "ORB      ", AddressingMode::Indexed  ,  4, 2 },
    { 0xEB, "ADDB     ", AddressingMode::Indexed  ,  4, 2 },
    { 0xEC, "LDD      ", AddressingMode::Indexed  ,  5, 2 },
    { 0xED, "STD      ", AddressingMode::Indexed  ,  5, 2 },
    { 0xEE, "LDU      ", AddressingMode::Indexed  ,  5, 2 },
    { 0xEF, "STU      ", AddressingMode::Indexed  ,  5, 2 },
    { 0xF0, "SUBB     ", AddressingMode::Extended ,  5, 3 },
    { 0xF1, "CMPB     ", AddressingMode::Extended ,  5, 3 },
    { 0xF2, "SBCB     ", AddressingMode::Extended ,  5, 3 },
    { 0xF3, "ADDD     ", AddressingMode::Extended ,  7, 3 },
    { 0xF4, "ANDB     ", AddressingMode::Extended ,  5, 3 },
    { 0xF5, "BITB     ", AddressingMode::Extended ,  5, 3 },
    { 0xF6, "LDB      ", AddressingMode::Extended ,  5, 3 },
    { 0xF7, "STB      ", AddressingMode::Extended ,  5, 3 },
    { 0xF8, "EORB     ", AddressingMode::Extended ,  5, 3 },
    { 0xF9, "ADCB     ", AddressingMode::Extended ,  5, 3 },
    { 0xFA, "ORB      ", AddressingMode::Extended ,  5, 3 },
    { 0xFB, "ADDB     ", AddressingMode::Extended ,  5, 3 },
    { 0xFC, "LDD      ", AddressingMode::Extended ,  6, 3 },
    { 0xFD, "STD      ", AddressingMode::Extended ,  6, 3 },
    { 0xFE, "LDU      ", AddressingMode::Extended ,  6, 3 },
    { 0xFF, "STU      ", AddressingMode::Extended ,  6, 3 },
    // clang-format on
};
static_assert(sizeof(CpuOpsPage0) / sizeof(CpuOpsPage0[0]) == 256, "");

constexpr CpuOp CpuOpsPage1[] = {
    // clang-format off
    { 0x21, "LBRN     ", AddressingMode::Relative ,  5, 4 },
    { 0x22, "LBHI     ", AddressingMode::Relative ,  5, 4 },
    { 0x23, "LBLS     ", AddressingMode::Relative ,  5, 4 },
    { 0x24, "LBHS/LBCC", AddressingMode::Relative ,  5, 4 },
    { 0x25, "LBLO/LBCS", AddressingMode::Relative ,  5, 4 },
    { 0x26, "LBNE     ", AddressingMode::Relative ,  5, 4 },
    { 0x27, "LBEQ     ", AddressingMode::Relative ,  5, 4 },
    { 0x28, "LBVC     ", AddressingMode::Relative ,  5, 4 },
    { 0x29, "LBVS     ", AddressingMode::Relative ,  5, 4 },
    { 0x2A, "LBPL     ", AddressingMode::Relative ,  5, 4 },
    { 0x2B, "LBMI     ", AddressingMode::Relative ,  5, 4 },
    { 0x2C, "LBGE     ", AddressingMode::Relative ,  5, 4 },
    { 0x2D, "LBLT     ", AddressingMode::Relative ,  5, 4 },
    { 0x2E, "LBGT     ", AddressingMode::Relative ,  5, 4 },
    { 0x2F, "LBLE     ", AddressingMode::Relative ,  5, 4 },
    { 0x3F, "SWI2     ", AddressingMode::Inherent , 20, 2 },
    { 0x83, "CMPD     ", AddressingMode::Immediate,  5, 4 },
    { 0x8C, "CMPY     ", AddressingMode::Immediate,  5, 4 },
    { 0x8E, "LDY      ", AddressingMode::Immediate,  4, 4 },
    { 0x93, "CMPD     ", AddressingMode::Direct   ,  7, 3 },
    { 0x9C, "CMPY     ", AddressingMode::Direct   ,  7, 3 },
    { 0x9E, "LDY      ", AddressingMode::Direct   ,  6, 3 },
    { 0x9F, "STY      ", AddressingMode::Direct   ,  6, 3 },
    { 0xA3, "CMPD     ", AddressingMode::Indexed  ,  7, 3 },
    { 0xAC, "CMPY     ", AddressingMode::Indexed  ,  7, 3 },
    { 0xAE, "LDY      ", AddressingMode::Indexed  ,  6, 3 },
    { 0xAF, "STY      ", AddressingMode::Indexed  ,  6, 3 },
    { 0xB3, "CMPD     ", AddressingMode::Extended ,  8, 4 },
    { 0xBC, "CMPY     ", AddressingMode::Extended ,  8, 4 },
    { 0xBE, "LDY      ", AddressingMode::Extended ,  7, 4 },
    { 0xBF, "STY      ", AddressingMode::Extended ,  7, 4 },
    { 0xCE, "LDS      ", AddressingMode::Immediate,  4, 4 },
    { 0xDE, "LDS      ", AddressingMode::Direct   ,  6, 3 },
    { 0xDF, "STS      ", AddressingMode::Direct   ,  6, 3 },
    { 0xEE, "LDS      ", AddressingMode::Indexed  ,  6, 3 },
    { 0xEF, "STS      ", AddressingMode::Indexed  ,  6, 3 },
    { 0xFE, "LDS      ", AddressingMode::Extended ,  7, 4 },
    { 0xFF, "STS      ", AddressingMode::Extended ,  7, 4 },
    // clang-format on
};

constexpr CpuOp CpuOpsPage2[] = {
    // clang-format off
    { 0x3F, "SWI3     ", AddressingMode::Inherent , 20, 2 },
    { 0x83, "CMPU     ", AddressingMode::Immediate,  5, 4 },
    { 0x8C, "CMPS     ", AddressingMode::Immediate,  5, 4 },
    { 0x93, "CMPU     ", AddressingMode::Direct   ,  7, 3 },
    { 0x9C, "CMPS     ", AddressingMode::Direct   ,  7, 3 },
    { 0xA3, "CMPU     ", AddressingMode::Indexed  ,  7, 3 },
    { 0xAC, "CMPS     ", AddressingMode::Indexed  ,  7, 3 },
    { 0xB3, "CMPU     ", AddressingMode::Extended ,  8, 4 },
    { 0xBC, "CMPS     ", AddressingMode::Extended ,  8, 4 },
    // clang-format on
};

constexpr size_t NumCpuOpsPage0 = sizeof(CpuOpsPage0) / sizeof(CpuOpsPage0[0]);
constexpr size_t NumCpuOpsPage1 = sizeof(CpuOpsPage1) / sizeof(CpuOpsPage1[0]);
constexpr size_t NumCpuOpsPage2 = sizeof(CpuOpsPage2) / sizeof(CpuOpsPage2[0]);

// First byte of instruction is 0x10
inline bool IsOpCodePage1(uint8_t firstByte) {
    return firstByte == 0x10;
}

// First byte of instruction is 0x11
inline bool IsOpCodePage2(uint8_t firstByte) {
    return firstByte == 0x11;
}

constexpr const CpuOp& FindCpuOp(const CpuOp table[], uint8_t opCode, int index = 0) {
    return table[index].opCode == opCode ? table[index] : FindCpuOp(table, opCode, index + 1);
}

// Compile-time lookup - use LookupCpuOpRuntime for runtime lookups
//@TODO: Make this faster and get rid of LookupCpuOpRuntime! One way is to fill up the page 1+2
// tables with invalid entries so that valid entries are on their opCode index so we can look them
// up the same way we do page 0.
constexpr const CpuOp& LookupCpuOp(int page, uint8_t opCode) {
    return page == 0 ? CpuOpsPage0[opCode]
                     : page == 1 ? FindCpuOp(CpuOpsPage1, opCode) : FindCpuOp(CpuOpsPage2, opCode);
}

// Faster version for runtime lookup
inline const CpuOp& LookupCpuOpRuntime(int page, uint8_t opCode) {
    auto InitLookupTables = [] {
        // static std::array<const struct CpuOp*, 256> opCodeTables[3];
        static const CpuOp* opCodeTables[3][256];
        for (auto& opCodeTable : opCodeTables)
            std::fill(std::begin(opCodeTable), std::end(opCodeTable), nullptr);

        for (size_t i = 0; i < NumCpuOpsPage0; ++i)
            opCodeTables[0][CpuOpsPage0[i].opCode] = &CpuOpsPage0[i];
        for (size_t i = 0; i < NumCpuOpsPage1; ++i)
            opCodeTables[1][CpuOpsPage1[i].opCode] = &CpuOpsPage1[i];
        for (size_t i = 0; i < NumCpuOpsPage2; ++i)
            opCodeTables[2][CpuOpsPage2[i].opCode] = &CpuOpsPage2[i];
        return opCodeTables;
    };

    static const auto& lookupTables = InitLookupTables();
    return *lookupTables[page][opCode];
}
