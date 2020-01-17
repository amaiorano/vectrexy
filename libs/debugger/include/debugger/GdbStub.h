#pragma once
#include "core/Tcp.h"
#include "emulator/EngineTypes.h"

class Emulator;

class GdbStub {
public:
    void Init(Emulator& emulator);
    void Reset();

    bool Connected() const;
    void Connect();

    bool FrameUpdate(double frameTime, const EmuEvents& emuEvents, const Input& input,
                     RenderContext& renderContext, AudioContext& audioContext);

private:
    void GdbStub::ExecuteFrameInstructions(double frameTime, const Input& input,
                                           RenderContext& renderContext,
                                           AudioContext& audioContext);
    cycles_t ExecuteInstruction(const Input& input, RenderContext& renderContext,
        AudioContext& audioContext);

    Emulator* m_emulator;
    TcpServer m_socket;
    int m_lastSignal;
    bool m_running = false;
    double m_cpuCyclesLeft = 0;
};
