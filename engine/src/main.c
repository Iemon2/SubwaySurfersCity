#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define PANIC(ERROR, FORMAT, ...) { if(ERROR) { fprintf(stderr, "%s -> %s -> %i -> ERROR(%i):\n\t" FORMAT "\n", __FILE_NAME__, __FUNCTION__, __LINE__, ERROR, ##__VA_ARGS__); raise(SIGABRT);}}

void glfwErrorCallback(int error_code, const char* description) {
  PANIC(error_code, "GLFW: %s", description)
}

void exitCallback() {
  glfwTerminate();
}

typedef struct {
  const char *window_title;
  int window_width, window_height;
  bool window_resizable;
  bool window_fullscreen;

  uint32_t apiVersion;

  GLFWmonitor *window_monitor;
  GLFWwindow *window;

  VkAllocationCallbacks *allocator;
  VkInstance instance;

} State;

void setupErrorHandling() {
  glfwSetErrorCallback(glfwErrorCallback);
  atexit(exitCallback);
}

void createWindow(State *state) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, state->window_resizable);

  if(state->window_fullscreen) {
    state->window_monitor = glfwGetPrimaryMonitor();
  }
  state->window = glfwCreateWindow(state->window_width, state->window_height, state->window_title, state->window_monitor, NULL);
}

void createInstance(State *state) {
  uint32_t requiredExtensionsCount;
  const char **requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionsCount);

  VkApplicationInfo applicationInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .apiVersion = state->apiVersion,
  };

  VkInstanceCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &applicationInfo,
    .enabledExtensionCount = requiredExtensionsCount,
    .ppEnabledExtensionNames = requiredExtensions,
  };

  PANIC(vkCreateInstance(&createInfo, state->allocator, &state->instance), "Couldnt create instance");
}

void logInfo(uint32_t instanceApiVersion) {
  vkEnumerateInstanceVersion(&instanceApiVersion);
  uint32_t ApiVersionVarient = VK_API_VERSION_VARIANT(instanceApiVersion);
  uint32_t ApiVersionMajor = VK_API_VERSION_MAJOR(instanceApiVersion);
  uint32_t ApiVersionMinor = VK_API_VERSION_MINOR(instanceApiVersion);
  uint32_t ApiVersionPatch = VK_API_VERSION_PATCH(instanceApiVersion);

  printf("Vulkan API %i.%i.%i.%i\n", ApiVersionVarient, ApiVersionMajor, ApiVersionMinor, ApiVersionPatch);
  printf("GLFW %s\n", glfwGetVersionString());
}

void init(State *state) {
  setupErrorHandling();
  logInfo(state->apiVersion);
  createWindow(state);
  createInstance(state);
}

void loop(State *state) {
  while(!glfwWindowShouldClose(state->window)) {
    glfwPollEvents();
  }
}

void cleanup(State *state) {
  glfwDestroyWindow(state->window);
  vkDestroyInstance(state->instance, state->allocator);
  state->window = NULL;
}

int main() {
  State state = {
    .window_title = "meow",
    .window_width = 720,
    .window_height = 480,
    .window_resizable = false,
    .window_fullscreen = false

  };

  init(&state);
  loop(&state);
  cleanup(&state);

  return EXIT_SUCCESS;
}
