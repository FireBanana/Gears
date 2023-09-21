#include <Logger.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>
#include <functional>
#include <renderdoc_app.h>
#include <dlfcn.h>

#define GL() LOGI("Gears:: GL_CHECK at line %d = %d", __LINE__ , glGetError());

GLuint						   mFb;
EGLContext					   mUnityContext;
std::mutex					   mDefaultMutex;

std::queue<std::function<void()>> mEventQueue;

unsigned int* mResultArray;
int			  mResultArraySize;
int			  mCurrentArrayIndex = 0;

using namespace std::chrono_literals;

RENDERDOC_API_1_1_2* rdoc_api = nullptr;

void loadRenderDoc()
{
	if (void* mod = dlopen("libVkLayer_GLES_RenderDoc.so", RTLD_NOW | RTLD_NOLOAD))
	{
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);

		if(ret != 1) LOGI("Gears:: Error, renderdoc not loaded");
	}
}

void initEgl()
{
	auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	if (display == EGL_NO_DISPLAY)
	{
		LOGI("Gears:: No display found");
		return;
	}

	EGLint eglConfigAttribs[] =
	{
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE
	};

	EGLConfig eglConfig;
	EGLint numConfigs;

	if (eglInitialize(display, nullptr, nullptr) != EGL_TRUE)
	{
		LOGI("Gears:: Error when initializing egl");
	}

	if (!eglChooseConfig(display, eglConfigAttribs, &eglConfig, 1, &numConfigs))
	{
		LOGI("Gears:: Problem when choosing config");
		return;
	}

	EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE
	};

	auto context = eglCreateContext(display, eglConfig, mUnityContext, contextAttribs);

	if (context == EGL_NO_CONTEXT)
	{
		LOGI("Gears:: Problem creating context");
		return;
	}

	EGLint surfAttr[] =
	{
		EGL_WIDTH, 64,
		EGL_HEIGHT, 64,
		EGL_NONE
	};

	auto surface = eglCreatePbufferSurface(display, eglConfig, surfAttr);

	if (surface == EGL_NO_SURFACE)
	{
		LOGI("Gears:: Problem creating surface");
		return;
	}

	if (eglMakeCurrent(display, surface, surface, context))
	{
		LOGI("Gears:: Context made successfully and set to current");
	}
	else
	{
		LOGI("Gears:: Problem when trying to make current");
	}
}

void createDefaultFramebuffer()
{
	GL();
	glGenFramebuffers(1, &mFb);
	GL();
	glBindFramebuffer(GL_FRAMEBUFFER, mFb);
	GL();
	glViewport(0, 0, 64, 64);
	GL();
	GLenum attachments[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, attachments);
}

extern "C" void createTexture()
{
	LOGI("Gears:: Start pushing fn to queue");
	std::lock_guard<std::mutex> guard(mDefaultMutex);

	auto fn = [&]()
	{

		if (rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);

		GLuint texture;

		GL();
		glGenTextures(1, &texture);
		GL();
		glBindTexture(GL_TEXTURE_2D, texture);

		GL();
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 64, 64);
		GL();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

		GL();
		glBindFramebuffer(GL_FRAMEBUFFER, mFb);
		GL();
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	
		GL();
		auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		switch (status)
		{
			case GL_FRAMEBUFFER_UNDEFINED: LOGI("Gears:: Error - GL_FRAMEBUFFER_UNDEFINED"); break;
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: LOGI("Gears:: Error - GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT"); break;
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: LOGI("Gears:: Error - GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"); break;
			case GL_FRAMEBUFFER_UNSUPPORTED: LOGI("Gears:: Error - GL_FRAMEBUFFER_UNSUPPORTED"); break;
			case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: LOGI("Gears:: Error - GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"); break;
			case GL_FRAMEBUFFER_COMPLETE: LOGI("Gears:: Framebuffer complete"); break;
			default: LOGI("Gears Error:: Uknown Framebuffer error"); break;
		}

		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		auto fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

		if (glClientWaitSync(fence, 0, GL_TIMEOUT_IGNORED) != GL_CONDITION_SATISFIED)
		{
			LOGI("Gears:: Error, wait failed");
		}

		LOGI("Gears:: Texture created with id %d", (int)texture);
		mResultArray[mCurrentArrayIndex++] = texture;

		glDeleteSync(fence);

		if (rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);
	};

	LOGI("Gears:: Pushing fn to queue");
	mEventQueue.push(fn);
}

extern "C" void lockMutex()
{
	mDefaultMutex.lock();
}

extern "C" void unlockMutex()
{
	mDefaultMutex.unlock();
}

extern "C" void registerResultArray(unsigned int* resultArray, int size)
{
	mResultArray = resultArray;
	mResultArraySize = size;
}

extern "C" void startGearEngine()
{
	mUnityContext = eglGetCurrentContext();

	auto thread = std::thread([]()
	{
		LOGI("Gears:: Starting thread");

		initEgl();
		loadRenderDoc();
		createDefaultFramebuffer();

		LOGI("Gears:: Starting loop");

		try
		{
			while (true)
			{
				std::this_thread::sleep_for(1s);

				std::lock_guard<std::mutex> guard(mDefaultMutex);

				LOGI("Gears:: Queue: %d", (int)(mEventQueue.size()));
				if (!mEventQueue.empty())
				{
					auto& fn = mEventQueue.front();

					fn();

					mEventQueue.pop();
				}
			}
		}
		catch (std::exception e)
		{
			LOGI("Gears:: %s", e.what());
		}
	});

	thread.detach();
}