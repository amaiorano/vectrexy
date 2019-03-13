#pragma once

#include "core/Base.h"
#include "core/Pimpl.h"

// Implementation of the AY-3-8912 Programmable Sound Generator (PSG)

class Psg {
public:
    Psg();
    ~Psg();

    void Init();

    void SetBDIR(bool enable);
    void SetBC1(bool enable);
    bool BDIR() const;
    bool BC1() const;

    void WriteDA(uint8_t value);
    uint8_t ReadDA();

    void Reset();
    void Update(cycles_t cycles);

    float Sample() const;

    void FrameUpdate(double frameTime);

private:
    pimpl::Pimpl<class PsgImpl, 256> m_impl;
};
