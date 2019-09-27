#include "null_engine/NullEngine.h"
#include "engine/EngineUtil.h"
#include "engine/Paths.h"

namespace {
    IEngineClient* g_client = nullptr;
}

void NullEngine::RegisterClient(IEngineClient& client) {
    g_client = &client;
}

bool NullEngine::Run(int argc, char** argv) {
    if (!EngineUtil::FindAndSetRootPath(fs::path(fs::absolute(argv[0]))))
        return false;

    std::shared_ptr<IEngineService> engineService =
        std::make_shared<aggregate_adapter<IEngineService>>(
            // SetFocusMainWindow
            [] {},
            // SetFocusConsole
            [] {},
            // ResetOverlay
            [](const char* /*file*/) {});

    if (!g_client->Init(engineService, Paths::biosRomFile.string(), argc, argv)) {
        return false;
    }

    bool quit = false;
    while (!quit) {
        double frameTime = 1.0 / 60;
        EmuEvents emuEvents{};
        Options options{};
        Input input{};
        RenderContext renderContext{};
        AudioContext audioContext{0};

        if (!g_client->FrameUpdate(frameTime, {std::ref(emuEvents), std::ref(options)}, input,
                                   renderContext, audioContext)) {
            quit = true;
        }
    }

    return false;
}
