#include <Logger.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>
#include <functional>

struct MutexHandle 
{
	std::mutex* mutex;
};

GLuint						   mFb;
std::mutex					   mDefaultMutex;

std::queue<std::function<void()>> mEventQueue;

unsigned int* mResultArray;
int			  mResultArraySize;
int			  mCurrentArrayIndex = 0;

using namespace std::chrono_literals;

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

	if (!eglChooseConfig(display, eglConfigAttribs, &eglConfig, 1, &numConfigs))
	{
		LOGI("Gears:: Problem when choosing config");
		return;
	}

	EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE
	};

	auto context = eglCreateContext(display, eglConfig, EGL_NO_CONTEXT, contextAttribs);

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
	glGenFramebuffers(1, &mFb);
	glBindFramebuffer(GL_FRAMEBUFFER, mFb);
	GLenum attachments[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, attachments);
}

extern "C" void createTexture()
{
	LOGI("Gears:: Start pushing fn to queue");
	std::lock_guard<std::mutex> guard(mDefaultMutex);

	auto fn = [&]()
	{
		GLuint texture;

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

		glClearColor(0, 1, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		mResultArray[mCurrentArrayIndex++] = texture;

		LOGI("Gears:: Texture created");
	};

	LOGI("Gears:: Pushing fn to queue");
	mEventQueue.push(fn);
}

extern "C" void registerMutex(MutexHandle* handle)
{
	handle->mutex = &mDefaultMutex;
}

extern "C" void lockMutex(MutexHandle* handle)
{
	handle->mutex->lock();
}

extern "C" void unlockMutex(MutexHandle* handle)
{
	handle->mutex->unlock();
}

extern "C" void registerResultArray(unsigned int* resultArray, int size)
{
	mResultArray = resultArray;
	mResultArraySize = size;
}

extern "C" void startGearEngine()
{
	auto thread = std::thread([]()
	{
		LOGI("Gears:: Starting thread");

		initEgl();
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