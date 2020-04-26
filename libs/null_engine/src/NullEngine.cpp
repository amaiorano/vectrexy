#include "null_engine/NullEngine.h"
#include "core/ConsoleOutput.h"
#include "engine/EngineUtil.h"
#include "engine/Paths.h"

namespace {
    IEngineClient* g_client = nullptr;
}

void NullEngine::RegisterClient(IEngineClient& client) {
    g_client = &client;
}

bool NullEngine::Run(int argc, char** argv) {
    const auto args = std::vector<std::string_view>(argv + 1, argv + argc);

    if (!EngineUtil::FindAndSetRootPath(fs::path(fs::absolute(argv[0])))) {
        Errorf("Failed to find and set root path\n");
        return false;
    }

    std::shared_ptr<IEngineService> engineService =
        std::make_shared<aggregate_adapter<IEngineService>>(
            // SetFocusMainWindow
            [] {},
            // SetFocusConsole
            [] {},
            // ResetOverlay
            [](const char* /*file*/) {});

    if (!g_client->Init(args, engineService, Paths::biosRomFile.string())) {
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
