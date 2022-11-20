#include <android_native_app_glue.h>
#include <android/log.h>
#include "pch.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))

// -------- Refactor ----------

static void handle_cmd_callback(struct android_app*, int32_t cmd)
{
    switch (cmd)
    {
        case APP_CMD_START:   LOGI("OWAIS STARTED APP"); break;
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