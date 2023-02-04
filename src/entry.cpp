#include <android_native_app_glue.h>

#include "Logger.h"
#include "graphics.h"

// -------- Refactor ----------

static void handle_cmd_callback(struct android_app* app, int32_t cmd)
{
    switch (cmd)
    {
        case APP_CMD_INIT_WINDOW: { LOGI("Creating Vulkan"); Gears::Graphics g{ app }; break; }
        case APP_CMD_START:   break;
        case APP_CMD_RESUME:  break;
        case APP_CMD_PAUSE:   break;
        case APP_CMD_STOP:    break;
        case APP_CMD_DESTROY: break;
    }
}

// ----------------------------

struct AndroidAppState {
    ANativeWindow* NativeWindow = nullptr;
    bool Resumed = false;
};

// This is the entry point of the app
void android_main(struct android_app* app) 
{
    AndroidAppState appState {};

    app->userData = &appState;
    app->onAppCmd = handle_cmd_callback;

    while (app->destroyRequested == 0)
    {
        while (1)
        {
            struct    android_poll_source* pollSource;
            int       events;
            const int timeoutMilliseconds =
                (!appState.Resumed && app->destroyRequested == 0) ? -1 : 0;

            if (ALooper_pollAll(timeoutMilliseconds, nullptr, &events, (void**)&pollSource) < 0)
            {
                break;
            }

            if(pollSource != nullptr)
                pollSource->process(app, pollSource);

            
        }
    }
}