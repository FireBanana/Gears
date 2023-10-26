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
#include <IUnityInterface.h>
#include <IUnityGraphics.h>

#define GL() /*LOGI("GearsNative:: GL_CHECK at line %d = %d", __LINE__ , glGetError());*/

const char* vertexSource = R"(
#version 320 es
precision mediump float;
out vec2 uv;

void main() 
{
    vec2 vertices[6];
    vertices[0] = vec2(-1.0, -1.0);
    vertices[1] = vec2( -1.0, 1.0);
    vertices[2] = vec2(1.0,  1.0);
    vertices[3] = vec2(-1.0,  -1.0);
    vertices[4] = vec2( 1.0, 1.0);
    vertices[5] = vec2( 1.0,  -1.0);
    
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    vec2 texCoord = (vertices[gl_VertexID] + 1.0) * 0.5;
    
    uv = texCoord;
}
)";

const char* fragmentSourceRgb = R"(
#version 320 es
precision mediump float;

uniform sampler2D sourceTex;

in vec2 uv;
out vec4 color;

void main() 
{
	vec4 img = texture(sourceTex, uv);
	vec3 col = pow(img.rgb, vec3(2.2));
    color = vec4(col, img.a);
}
)";

const char* fragmentSource = R"(
#version 320 es
precision mediump float;

uniform sampler2D sourceTex;

in vec2 uv;
out vec4 color;

void main() 
{
	vec4 img = texture(sourceTex, uv);
    color = img;
}
)";

struct Resource
{
	uint32_t id;
	uint32_t textureId;
};

GLuint						   mFb;
GLuint						   mShaderProgRgb;
GLuint						   mShaderProg;
EGLContext					   mUnityContext;
std::mutex					   mDefaultMutex;

std::queue<std::function<void()>> mEventQueue;

Resource*					   mResultArray;
int							   mResultArraySize;

using namespace std::chrono_literals;

RENDERDOC_API_1_1_2* rdoc_api = nullptr;

void loadRenderDoc()
{
	if (void* mod = dlopen("libVkLayer_GLES_RenderDoc.so", RTLD_NOW | RTLD_NOLOAD))
	{
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);

		if(ret != 1) LOGI("GearsNative:: Error, renderdoc not loaded");
	}
}

void initEgl()
{
	auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	if (display == EGL_NO_DISPLAY)
	{
		LOGI("GearsNative:: No display found");
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
		LOGI("GearsNative:: Error when initializing egl");
	}

	if (!eglChooseConfig(display, eglConfigAttribs, &eglConfig, 1, &numConfigs))
	{
		LOGI("GearsNative:: Problem when choosing config");
		return;
	}

	EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE
	};

	auto context = eglCreateContext(display, eglConfig, mUnityContext, contextAttribs);

	if (context == EGL_NO_CONTEXT)
	{
		LOGI("GearsNative:: Problem creating context");
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
		LOGI("GearsNative:: Problem creating surface");
		return;
	}

	if (eglMakeCurrent(display, surface, surface, context))
	{
		LOGI("GearsNative:: Context made successfully and set to current");
	}
	else
	{
		LOGI("GearsNative:: Problem when trying to make current");
	}
}

GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
	GLuint vertexShader, fragmentShader, shaderProgram;

	LOGI("GearsNative:: Creating shaders");

	// Create and compile the vertex shader.
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);

	// Check the vertex shader compilation status.
	GLint compileStatus;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus != GL_TRUE) {
		GLint infoLogLength;
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &infoLogLength);
		char* infoLog = new char[infoLogLength + 1];
		glGetShaderInfoLog(vertexShader, infoLogLength, NULL, infoLog);

		LOGI("GearsNative::(vertex) %s", infoLog);

		// Handle shader compilation error here.
		delete[] infoLog;
		glDeleteShader(vertexShader);
		return 0;
	}

	// Create and compile the fragment shader.
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);

	// Check the fragment shader compilation status.
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus != GL_TRUE) {
		GLint infoLogLength;
		glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &infoLogLength);
		char* infoLog = new char[infoLogLength + 1];
		glGetShaderInfoLog(fragmentShader, infoLogLength, NULL, infoLog);

		LOGI("GearsNative::(fragment) %s", infoLog);

		// Handle shader compilation error here.
		delete[] infoLog;
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		return 0;
	}

	// Create and link the shader program.
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	// Check the program linking status.
	GLint linkStatus;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE) {
		GLint infoLogLength;
		glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &infoLogLength);
		char* infoLog = new char[infoLogLength + 1];
		glGetProgramInfoLog(shaderProgram, infoLogLength, NULL, infoLog);

		LOGI("GearsNative:: %s", infoLog);

		// Handle program linking error here.
		delete[] infoLog;
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		glDeleteProgram(shaderProgram);
		return 0;
	}

	// Clean up individual shaders (they are no longer needed).
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

GLenum getGlTextureType(int type)
{
	switch (type)
	{
		case 8:   return GL_RGBA8;
		case 119: return GL_COMPRESSED_SRGB8_ETC2;    //Unity: RGB_ETC2_SRGB
		case 123: return GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;//Unity: RGBA_ETC2_SRGB
		case 129: return GL_COMPRESSED_RGBA_ASTC_4x4; //Unity: RGBA_ASTC4X4_SRGB
		case 130: return GL_COMPRESSED_RGBA_ASTC_4x4; //Unity: RGBA_ASTC4X4_UNorm
		default: LOGI("GearsNative:: Texture format (%d) is not implmented in library", type); return GL_RGBA8;
	};
}

GLenum getGlTextureFormat(int type)
{
	switch (type)
	{
		case 8:   return GL_RGBA;
		case 119: return GL_RGB;  //Unity: RGB_ETC2_SRGB
		case 123: return GL_RGBA; //Unity: RGBA_ETC2_SRGB
		case 129: return GL_RGBA; //Unity: RGBA_ASTC4X4_SRGB
		case 130: return GL_RGBA; //Unity: RGBA_ASTC4X4_UNorm
		default: LOGI("GearsNative:: Texture format (%d) is not implmented in library", type); return GL_RGBA;
	};
}

int getImageSize(int type, int width, int height)
{
	switch (type)
	{
		case 119: return ceil(width / 4.0f) * ceil(height / 4.0f) * 8.0f;   //Unity: RGB_ETC2_SRGB
		case 123: return ceil(width / 4.0f) * ceil(height / 4.0f) * 16.0f; //Unity: RGBA_ETC2_SRGB
		case 129: return ceil(width / 4.0f) * ceil(height / 4.0f) * 16.0f; //Unity: RGBA_ASTC4X4_SRGB
		case 130: return ceil(width / 4.0f) * ceil(height / 4.0f) * 16.0f; //Unity: RGBA_ASTC4X4_UNorm
		default: 
			LOGI("GearsNative:: Texture format (%d) is not implmented in library", type); 
			return ceil(width / 4.0f) * ceil(height / 4.0f) * 16.0f;
	};
}

GLuint getRequiredShaderProg(int type)
{
	switch (type)
	{
		case 8:   return mShaderProgRgb;
		case 119: return mShaderProg;  //Unity: RGB_ETC2_SRGB
		case 123: return mShaderProg; //Unity: RGBA_ETC2_SRGB
		case 129: return mShaderProgRgb; //Unity: RGBA_ASTC4X4_SRGB
		case 130: return mShaderProgRgb; //Unity: RGBA_ASTC4X4_UNorm
		default: LOGI("GearsNative:: Texture format (%d) is not implmented in library", type); return mShaderProgRgb;
	};
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

extern "C" void createTexture(uint32_t uniqueId, int textureType, int width, int height, int mipCount, uint8_t* bytes)
{
	//LOGI("GearsNative:: Start pushing fn to queue");
	std::lock_guard<std::mutex> guard(mDefaultMutex);

	auto fn = [&, uniqueId, textureType, width, height, mipCount, bytes]()
	{
		if (rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);

		GLuint texture;

		GL();
		glGenTextures(1, &texture);
		GL();
		glBindTexture(GL_TEXTURE_2D, texture);

		GL();
		//LOGI("GearsNative::w: %d h: %d", width, height);

		//LOGI("GearsNative:: suggested mip level is: %d", mipCount);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		GL();

		glTexStorage2D(GL_TEXTURE_2D, mipCount, getGlTextureType(textureType), width, height);
		auto type = getGlTextureType(textureType);
		auto imageSize = 0;

		if (type != GL_RGBA8)
		{
			imageSize = getImageSize(textureType, width, height);
			glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, type, imageSize, bytes);
		}
		else
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, getGlTextureFormat(textureType), GL_UNSIGNED_BYTE, bytes);
		}		

		// Convert texture ===

		glUseProgram(getRequiredShaderProg(textureType));
		
		glBindFramebuffer(GL_FRAMEBUFFER, mFb);

		GLuint destinationTexture; GL();
		glGenTextures(1, &destinationTexture); GL();
		glBindTexture(GL_TEXTURE_2D, destinationTexture); GL();

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr); GL();
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, destinationTexture, 0); GL();
		glViewport(0, 0, width, height);

		//auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		//switch (status)
		//{
		//case GL_FRAMEBUFFER_UNDEFINED: LOGI("GearsNative:: Error - GL_FRAMEBUFFER_UNDEFINED"); break;
		//case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: LOGI("GearsNative:: Error - GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT"); break;
		//case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: LOGI("GearsNative:: Error - GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"); break;
		//case GL_FRAMEBUFFER_UNSUPPORTED: LOGI("GearsNative:: Error - GL_FRAMEBUFFER_UNSUPPORTED"); break;
		//case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: LOGI("GearsNative:: Error - GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"); break;
		//case GL_FRAMEBUFFER_COMPLETE: LOGI("GearsNative:: Framebuffer complete"); break;
		//default: LOGI("Gears Error:: Uknown Framebuffer error"); break;
		//}

		glBindTexture(GL_TEXTURE_2D, texture);
		GLint baseImageLoc = glGetUniformLocation(getRequiredShaderProg(textureType), "sourceTex");
		glUniform1i(baseImageLoc, 0);
		glActiveTexture(GL_TEXTURE0);

		glDrawArrays(GL_TRIANGLES, 0, 6); GL();
		glDeleteTextures(1, &texture); GL();
		texture = destinationTexture;
		glBindTexture(GL_TEXTURE_2D, texture);

		// ===================
		
		GL();

		glGenerateMipmap(GL_TEXTURE_2D);

		GL();

		GL();
		glBindTexture(GL_TEXTURE_2D, 0);

		auto fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

		glClientWaitSync(fence, 0, GL_TIMEOUT_IGNORED);

		LOGI("GearsNative:: Texture created with id %d", (int)texture);
		
		for (int i = 0; i < mResultArraySize; ++i)
		{
			if (mResultArray[i].id == 0)
			{
				mResultArray[i].id = uniqueId;
				mResultArray[i].textureId = texture;
				break;
			}
		}

		glDeleteSync(fence);

		if (rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);
	};

	//LOGI("GearsNative:: Pushing fn to queue");
	mEventQueue.push(fn);
}

extern "C" void deleteTexture(uint32_t index)
{
	std::lock_guard<std::mutex> guard(mDefaultMutex);

	auto fn = [&, index]()
	{
		LOGI("GearsNative:: Deleting texture: %d", index);
		glDeleteTextures(1, &index);
	};

	mEventQueue.push(fn);
}

extern "C" void lockMutex() noexcept
{
	mDefaultMutex.lock();
}

extern "C" void unlockMutex() noexcept
{
	mDefaultMutex.unlock();
}

extern "C" void registerResultArray(Resource* resultArray, int size)
{
	mResultArray = resultArray;
	mResultArraySize = size;
}

extern "C" void startGearEngine()
{
	auto context = eglGetCurrentContext();

	if (context == EGL_NO_CONTEXT)
	{
		LOGI("GearsNative:: No current context found.");
	}
	else
	{
		LOGI("GearsNative:: Context found");
	}

	mUnityContext = context;

	auto thread = std::thread([]() noexcept
	{
		LOGI("GearsNative:: Starting thread");

		initEgl();
		loadRenderDoc();
		createDefaultFramebuffer();
		mShaderProgRgb = createShaderProgram(vertexSource, fragmentSourceRgb);
		mShaderProg = createShaderProgram(vertexSource, fragmentSource);

		LOGI("GearsNative:: Starting loop");

		try
		{
			while (true)
			{
				std::this_thread::sleep_for(100ms);

				std::lock_guard<std::mutex> guard(mDefaultMutex);

				while (!mEventQueue.empty())
				{
					auto& fn = mEventQueue.front();

					fn();

					mEventQueue.pop();
				}
			}
		}
		catch (std::exception e)
		{
			LOGI("GearsNative:: %s", e.what());
		}
	});

	thread.detach();
}

//================ UNITY SPECIFIC ====================

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	startGearEngine();
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
startEngine()
{
	return OnRenderEvent;
}

//====================================================