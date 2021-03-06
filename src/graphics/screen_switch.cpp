/*
 * bookr-modern: a graphics based document reader 
 * Copyright (C) 2019 pathway27 (Sree)
 * IS A MODIFICATION OF THE ORIGINAL
 * Bookr and bookr-mod for PSP
 * Copyright (C) 2005 Carlos Carrasco Martinez (carloscm at gmail dot com),
 *               2007 Christian Payeur (christian dot payeur at gmail dot com),
 *               2009 Nguyen Chi Tam (nguyenchitam at gmail dot com),
 * AND VARIOUS OTHER FORKS, See Forks in README.md
 * Licensed under GPLv3+, see LICENSE
*/

#include "screen.hpp"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <switch.h>

#include <EGL/egl.h>    // EGL library
#include <EGL/eglext.h> // EGL extensions
#include <glad/glad.h>  // glad library (OpenGL loader)

#include "texture.hpp"
#include "controls.hpp"
#include "../resource_manager.hpp"

#include "textures_frag.h"
#include "textures_vert.h"
#include "icon0_t_png.h"
#include "NotoSans-Regular_ttf.h"
#include "text_vert.h"
#include "text_frag.h"
//-----------------------------------------------------------------------------
// nxlink support
//-----------------------------------------------------------------------------

#ifndef DEBUG
#define TRACE(fmt, ...) ((void)0)
#else
#include <unistd.h>
#define TRACE(fmt, ...) printf("%s: " fmt "\n", __PRETTY_FUNCTION__, ##__VA_ARGS__)

static int s_nxlinkSock = -1;

static void initNxLink()
{
  if (R_FAILED(socketInitializeDefault()))
    return;

  s_nxlinkSock = nxlinkStdio();
  if (s_nxlinkSock >= 0)
    TRACE("printf output now goes to nxlink server");
  else
    socketExit();
}

static void deinitNxLink()
{
  if (s_nxlinkSock >= 0)
  {
    close(s_nxlinkSock);
    socketExit();
    s_nxlinkSock = -1;
  }
}

extern "C" void userAppInit()
{
  initNxLink();
}

extern "C" void userAppExit()
{
  deinitNxLink();
}

#endif


namespace bookr { namespace Screen {

unsigned int WIDTH = 1280;
unsigned int HEIGHT = 720;
//-----------------------------------------------------------------------------
// EGL initialization
//-----------------------------------------------------------------------------

static EGLDisplay s_display;
static EGLContext s_context;
static EGLSurface s_surface;

static bool initEgl(NWindow *win)
{
  // Connect to the EGL default display
  s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!s_display)
  {
    TRACE("Could not connect to display! error: %d", eglGetError());
    goto _fail0;
  }

  // Initialize the EGL display connection
  eglInitialize(s_display, nullptr, nullptr);

  // Select OpenGL (Core) as the desired graphics API
  if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
  {
    TRACE("Could not set API! error: %d", eglGetError());
    goto _fail1;
  }

  // Get an appropriate EGL framebuffer configuration
  EGLConfig config;
  EGLint numConfigs;
  static const EGLint framebufferAttributeList[] =
      {
          EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
          EGL_RED_SIZE, 8,
          EGL_GREEN_SIZE, 8,
          EGL_BLUE_SIZE, 8,
          EGL_ALPHA_SIZE, 8,
          EGL_DEPTH_SIZE, 24,
          EGL_STENCIL_SIZE, 8,
          EGL_NONE};
  eglChooseConfig(s_display, framebufferAttributeList, &config, 1, &numConfigs);
  if (numConfigs == 0)
  {
    TRACE("No config found! error: %d", eglGetError());
    goto _fail1;
  }

  // Create an EGL window surface
  s_surface = eglCreateWindowSurface(s_display, config, win, nullptr);
  if (!s_surface)
  {
    TRACE("Surface creation failed! error: %d", eglGetError());
    goto _fail1;
  }

  // Create an EGL rendering context
  static const EGLint contextAttributeList[] =
      {
          EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
          EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
          EGL_CONTEXT_MINOR_VERSION_KHR, 3,
          EGL_NONE};
  s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, contextAttributeList);
  if (!s_context)
  {
    TRACE("Context creation failed! error: %d", eglGetError());
    goto _fail2;
  }

  // Connect the context to the surface
  eglMakeCurrent(s_display, s_surface, s_surface, s_context);
  return true;

_fail2:
  eglDestroySurface(s_display, s_surface);
  s_surface = nullptr;
_fail1:
  eglTerminate(s_display);
  s_display = nullptr;
_fail0:
  return false;
}

static void deinitEgl()
{
  if (s_display)
  {
    eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s_context)
    {
      eglDestroyContext(s_display, s_context);
      s_context = nullptr;
    }
    if (s_surface)
    {
      eglDestroySurface(s_display, s_surface);
      s_surface = nullptr;
    }
    eglTerminate(s_display);
    s_display = nullptr;
  }
}

static const char *const vertexShaderSource = R"text(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    out vec3 ourColor;
    void main()
    {
        gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
        ourColor = aColor;
    }
)text";

static const char *const fragmentShaderSource = R"text(
    #version 330 core
    in vec3 ourColor;
    out vec4 fragColor;
    void main()
    {
        fragColor = vec4(ourColor, 1.0f);
    }
)text";

static GLuint createAndCompileShader(GLenum type, const char *source)
{
  GLint success;
  GLchar msg[512];

  GLuint handle = glCreateShader(type);
  if (!handle)
  {
    TRACE("%u: cannot create shader", type);
    return 0;
  }
  glShaderSource(handle, 1, &source, nullptr);
  glCompileShader(handle);
  glGetShaderiv(handle, GL_COMPILE_STATUS, &success);

  if (!success)
  {
    glGetShaderInfoLog(handle, sizeof(msg), nullptr, msg);
    TRACE("%u: %s\n", type, msg);
    glDeleteShader(handle);
    return 0;
  }

  return handle;
}

static GLuint s_program;
static GLuint s_vao, s_vbo;

static void sceneInit()
{
  GLint vsh = createAndCompileShader(GL_VERTEX_SHADER, vertexShaderSource);
  GLint fsh = createAndCompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

  s_program = glCreateProgram();
  glAttachShader(s_program, vsh);
  glAttachShader(s_program, fsh);
  glLinkProgram(s_program);

  GLint success;
  glGetProgramiv(s_program, GL_LINK_STATUS, &success);
  if (!success)
  {
    char buf[512];
    glGetProgramInfoLog(s_program, sizeof(buf), nullptr, buf);
    TRACE("Link error: %s", buf);
  }
  glDeleteShader(vsh);
  glDeleteShader(fsh);

  struct Vertex
  {
    float position[3];
    float color[3];
  };

  static const Vertex vertices[] =
      {
          {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
          {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
          {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
      };

  glGenVertexArrays(1, &s_vao);
  glGenBuffers(1, &s_vbo);
  // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
  glBindVertexArray(s_vao);

  glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, position));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color));
  glEnableVertexAttribArray(1);

  // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
  // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
  glBindVertexArray(0);
}

static TextRenderer* ui_text_renderer;
static void sceneRender()
{
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  ResourceManager::getSpriteRenderer()->DrawSprite(
    ResourceManager::GetTexture("logo"),
    glm::vec2(380, 150),
    glm::vec2(128, 128));

  ResourceManager::getSpriteRenderer()->DrawQuad(
      glm::vec2(450, 150),
      glm::vec2(128, 128),
      0.0f, glm::vec4(170/255.0, 170/255.0, 170/255.0, 255/255.0));

  ui_text_renderer->RenderText("Testing:", 5.0f, 5.0f, 1.0f, glm::vec3(0.0f,0.0f,0.0f));
}

static void sceneExit()
{
  glDeleteBuffers(1, &s_vbo);
  glDeleteVertexArrays(1, &s_vao);
  glDeleteProgram(s_program);
}

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

static void loadShaders() {
    ResourceManager::LoadShader((const char*)textures_vert, (const char*)textures_frag, nullptr, "sprite", false,
      textures_vert_size, textures_frag_size);
    ResourceManager::CreateSpriteRenderer(ResourceManager::GetShader("sprite"));

    glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(SCR_WIDTH), static_cast<GLfloat>(SCR_HEIGHT), 0.0f, -1.0f, 1.0f);
    // glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f);

    ResourceManager::GetShader("sprite").Use().SetInteger("sprite", 0);
    ResourceManager::GetShader("sprite").SetMatrix4("projection", projection, true);
    
    ResourceManager::LoadTexture((const char*)icon0_t_png, GL_TRUE, "logo", false, icon0_t_png_size);

    // text
    ResourceManager::LoadShader((const char*)text_vert, (const char*)text_frag, nullptr, "text", false,
      text_vert_size, text_frag_size);

    ui_text_renderer = new TextRenderer(ResourceManager::GetShader("text"), static_cast<GLfloat>(SCR_WIDTH), static_cast<GLfloat>(SCR_HEIGHT));

    // u8 a[]         -> char*          ; const
    // const FT_Byte* -> unsigned char*

    // std::string noto_font(reinterpret_cast<const char*>(NotoSans_Regular_ttf), NotoSans_Regular_ttf_size);
    std::string noto_font((const char *)NotoSans_Regular_ttf, NotoSans_Regular_ttf_size);
    ui_text_renderer->Load(noto_font, 24, true);
}

// Move this to constructor?
void open(int argc, char **argv)
{
  #ifdef DEBUG
    printf("screen::open\n");
    // Set mesa configuration (useful for debugging)
    // setMesaConfig();
  #endif
  // Initialize EGL on the default window
  if (!initEgl(nwindowGetDefault())) {
    #ifdef DEBUG
      printf("!initEgl(nwindowGetDefault())\n");
    #endif
    return;
  }

  gladLoadGL();
  #ifdef DEBUG
    printf("gladLoadGL()\n");
  #endif

  glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
  glEnable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  std::cout << glGetString(GL_VERSION) << std::endl;

  loadShaders();

  while (appletMainLoop())
  {
    // Get and process input
    hidScanInput();
    u32 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
    if (kDown & KEY_PLUS)
      break;

    // Render stuff!
    sceneRender();
    swapBuffers();
  }
}

int setupCallbacks(void) {
  return 0;
}

static void initalDraw() {
  sceneInit();
}



void close() {
  // Deinitialize our scene
  sceneExit();

  // Deinitialize EGL
  deinitEgl();
}

void exit() {

}

void drawText(int x, int y, unsigned int color, float scale, const char *text) {

}

void drawFontTextf(Font *font, int x, int y, unsigned int color, unsigned int size, const char *text, ...) {

}

void setTextSize(float x, float y) {

}

static bool stickyKeys = false;

static int breps[16];
static void updateReps(int keyState) {
  if (stickyKeys && keyState == 0) {
    stickyKeys = false;
  }
  if (stickyKeys) {
    memset((void*)breps, 0, sizeof(int)*16);
    return;
  }
  if (keyState & FZ_CTRL_SELECT  ) breps[FZ_REPS_SELECT  ]++; else breps[FZ_REPS_SELECT  ] = 0;
  if (keyState & FZ_CTRL_START   ) breps[FZ_REPS_START   ]++; else breps[FZ_REPS_START   ] = 0;
  if (keyState & FZ_CTRL_UP      ) breps[FZ_REPS_UP      ]++; else breps[FZ_REPS_UP      ] = 0;
  if (keyState & FZ_CTRL_RIGHT   ) breps[FZ_REPS_RIGHT   ]++; else breps[FZ_REPS_RIGHT   ] = 0;
  if (keyState & FZ_CTRL_DOWN    ) breps[FZ_REPS_DOWN    ]++; else breps[FZ_REPS_DOWN    ] = 0;
  if (keyState & FZ_CTRL_LEFT    ) breps[FZ_REPS_LEFT    ]++; else breps[FZ_REPS_LEFT    ] = 0;
  if (keyState & FZ_CTRL_LTRIGGER) breps[FZ_REPS_LTRIGGER]++; else breps[FZ_REPS_LTRIGGER] = 0;
  if (keyState & FZ_CTRL_RTRIGGER) breps[FZ_REPS_RTRIGGER]++; else breps[FZ_REPS_RTRIGGER] = 0;
  if (keyState & FZ_CTRL_TRIANGLE) breps[FZ_REPS_TRIANGLE]++; else breps[FZ_REPS_TRIANGLE] = 0;
  if (keyState & FZ_CTRL_CIRCLE  ) breps[FZ_REPS_CIRCLE  ]++; else breps[FZ_REPS_CIRCLE  ] = 0;
  if (keyState & FZ_CTRL_CROSS   ) breps[FZ_REPS_CROSS   ]++; else breps[FZ_REPS_CROSS   ] = 0;
  if (keyState & FZ_CTRL_SQUARE  ) breps[FZ_REPS_SQUARE  ]++; else breps[FZ_REPS_SQUARE  ] = 0;
  if (keyState & FZ_CTRL_HOME    ) breps[FZ_REPS_HOME    ]++; else breps[FZ_REPS_HOME    ] = 0;
  if (keyState & FZ_CTRL_HOLD    ) breps[FZ_REPS_HOLD    ]++; else breps[FZ_REPS_HOLD    ] = 0;
  if (keyState & FZ_CTRL_NOTE    ) breps[FZ_REPS_NOTE    ]++; else breps[FZ_REPS_NOTE    ] = 0;
}

static void normalizeControls(u64 kDown, u64 kHeld, u64 kUp)
{
  switch (kDown) {
    // case GLFW_KEY_H: keyState |= FZ_CTRL_HOLD;break;
  }
}


void resetReps() {

}

int* ctrlReps() {
  return breps;
}

void setupCtrl() {
  resetReps();
}

int readCtrl() {
  hidScanInput();
  u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
  //hidKeysHeld returns information about which buttons have are held down in this frame
  u64 kHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
  //hidKeysUp returns information about which buttons have been just released
  u64 kUp = hidKeysUp(CONTROLLER_P1_AUTO);

  normalizeControls(kDown, kHeld, kUp);
}

void getAnalogPad(int& x, int& y) {
  JoystickPosition pos_left, pos_right;

  //Read the joysticks' position
  hidJoystickRead(&pos_left, CONTROLLER_P1_AUTO, JOYSTICK_LEFT);
  hidJoystickRead(&pos_right, CONTROLLER_P1_AUTO, JOYSTICK_RIGHT);
}

void startDirectList() {

}

void endAndDisplayList() {

}


void swapBuffers() {
  eglSwapBuffers(s_display, s_surface);
}

void waitVblankStart() {

}

void* getListMemory(int s) {
  return 0;
}

void shadeModel(int mode) {

}

void color(unsigned int c) {

}

void ambientColor(unsigned int c) {

}

void clear(unsigned int color, int b) {

}

void checkEvents(int buttons) {
}

void matricesFor2D(int rotation) {
}

struct T32FV32F2D {
    float u,v;
    float x,y,z;
};

void setBoundTexture(Texture *t) {

}

void drawRectangle(float x, float y, float w, float h, unsigned int color) {

}

void drawFontText(Font *font, int x, int y, unsigned int color, unsigned int size, const char *text) {

}

void drawTextureScale(const Texture *texture, float x, float y, float x_scale, float y_scale) {

}

void drawTextureTintScale(const Texture *texture, float x, float y, float x_scale, float y_scale, unsigned int color) {

}

void drawTextureTintScaleRotate(const Texture *texture, float x, float y, float x_scale, float y_scale, float rad, unsigned int color) {

}

/*  Active Shader
    bind correct vertex array
  */
void drawArray(int prim, int vtype, int count, void* indices, void* vertices) {
}

void copyImage(int psm, int sx, int sy, int width, int height, int srcw, void *src,
    int dx, int dy, int destw, void *dest) {

}

void drawPixel(float x, float y, unsigned int color) {

}

void* framebuffer() {
  return 0;
}

void blendFunc(int op, int src, int dst) {

}

void enable(int m) {

}

void disable(int m) {

}

void dcacheWritebackAll() {

}

string basePath() {
  return "./";
}

struct CompareDirent {
  bool operator()(const Dirent& a, const Dirent& b) {
      if ((a.stat & FZ_STAT_IFDIR) == (b.stat & FZ_STAT_IFDIR))
          return a.name < b.name;
      if (b.stat & FZ_STAT_IFDIR)
          return false;
      return true;
  }
};

int dirContents(const char* path, vector<Dirent>& a) {
  return 0;
}

int getSuspendSerial() {
  return 0;
}

void setSpeed(int v) {

}

int getSpeed() {
  return 0;
}

void getTime(int &h, int &m) {

}

int getBattery() {
  return 0;
}

int getUsedMemory() {
  return 0;
}

void setBrightness(int b){

}

bool isClosing() {
  return !appletMainLoop();
}

}}
