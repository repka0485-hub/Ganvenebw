// libespmod.so: loaded via LD_PRELOAD (see tools/inject.sh). Hooks eglSwapBuffers with
// Dobby and draws an ImGui overlay (OpenGL ES3, render-only, no input handling).
#include <EGL/egl.h>
#include <android/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include "dobby.h"
#include "esp.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"

#define TAG "espmod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

namespace {

using SwapFn = EGLBoolean (*)(EGLDisplay, EGLSurface);

SwapFn g_orig_swap = nullptr;
ImGuiContext* g_imgui = nullptr;
EGLContext g_ctx = EGL_NO_CONTEXT;
double g_last_time = 0.0;

double now_s() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

// (Re)creates the ImGui context when the GL context changes. The old context's GL objects
// died with it, so the old ImGui context is deliberately leaked rather than shut down:
// Shutdown would call glDelete* with stale ids inside the new context.
bool ensure_imgui(EGLContext ctx) {
  if (g_imgui && ctx == g_ctx) return true;
  g_imgui = ImGui::CreateContext();
  ImGui::SetCurrentContext(g_imgui);
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
    LOGE("ImGui_ImplOpenGL3_Init failed");
    g_imgui = nullptr;
    g_ctx = EGL_NO_CONTEXT;
    return false;
  }
  g_ctx = ctx;
  g_last_time = 0.0;
  LOGI("imgui initialised for context %p", ctx);
  return true;
}

EGLBoolean hk_swap(EGLDisplay dpy, EGLSurface surf) {
  const EGLContext ctx = eglGetCurrentContext();
  EGLint w = 0, h = 0;
  if (ctx != EGL_NO_CONTEXT && eglQuerySurface(dpy, surf, EGL_WIDTH, &w) &&
      eglQuerySurface(dpy, surf, EGL_HEIGHT, &h) && w > 0 && h > 0 && ensure_imgui(ctx)) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
    const double t = now_s();
    io.DeltaTime = g_last_time > 0.0 ? static_cast<float>(t - g_last_time) : 1.0f / 60.0f;
    if (io.DeltaTime < 1e-4f) io.DeltaTime = 1e-4f;
    g_last_time = t;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    esp::draw(io.DisplaySize);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }
  return g_orig_swap(dpy, surf);
}

// libEGL may not be loaded yet when a preloaded library initialises, so install the hook
// from a worker thread and retry for up to 10 s.
void* install_thread(void*) {
  for (int i = 0; i < 100; ++i) {
    void* egl = dlopen("libEGL.so", RTLD_NOW | RTLD_NOLOAD);
    if (!egl) egl = dlopen("libEGL.so", RTLD_NOW);
    void* target = egl ? dlsym(egl, "eglSwapBuffers") : nullptr;
    if (target) {
      const int rc = DobbyHook(target, reinterpret_cast<void*>(hk_swap),
                               reinterpret_cast<void**>(&g_orig_swap));
      if (rc == 0 && g_orig_swap) {
        LOGI("eglSwapBuffers hooked at %p", target);
      } else {
        LOGE("DobbyHook failed rc=%d", rc);
      }
      return nullptr;
    }
    usleep(100 * 1000);
  }
  LOGE("eglSwapBuffers not found; hook not installed");
  return nullptr;
}

}  // namespace

__attribute__((constructor)) static void espmod_init() {
  pthread_t th;
  if (pthread_create(&th, nullptr, install_thread, nullptr) == 0) {
    pthread_detach(th);
  } else {
    LOGE("pthread_create failed");
  }
}
