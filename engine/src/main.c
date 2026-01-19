#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define EXPECT(ERROR, FORMAT, ...) { int errCode; if((errCode == ERROR)) { fprintf(stderr, "%s -> %s -> %i -> ERROR(%i):\n\t" FORMAT "\n", __FILE_NAME__, __FUNCTION__, __LINE__, ERROR, ##__VA_ARGS__); raise(SIGABRT);}}

void glfwErrorCallback(int error_code, const char* description) {
  EXPECT(error_code, "GLFW: %s", description)
}

void exitCallback() {
  glfwTerminate();
}

typedef struct {
  const char *windowTitle;
  int windowWidth, windowHeight;
  bool windowFullscreen;

  //glfw
  GLFWmonitor *windowMonitor;
  GLFWwindow *window;

  //vulkan
  uint32_t apiVersion;
  uint32_t queueFamily;
  uint32_t swapchainImageCount;

  VkAllocationCallbacks *allocator;
  VkInstance instance;

  VkPhysicalDevice physicalDevice;
  VkSurfaceKHR surface;
  VkDevice device;
  VkQueue queue;

  VkSwapchainKHR swapchain;
  VkImage *swapchainImages;
  VkImageView *swapchainImageViews;

} State;

void setupErrorHandling() {
  glfwSetErrorCallback(glfwErrorCallback);
  atexit(exitCallback);
}


void createWindow(State *state) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  if(state->windowFullscreen) {
    state->windowMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(state->windowMonitor);
    state->windowWidth = mode->width;
    state->windowHeight = mode->height;
  }
  state->window = glfwCreateWindow(state->windowWidth, state->windowHeight, state->windowTitle, state->windowMonitor, NULL);
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

  EXPECT(vkCreateInstance(&createInfo, state->allocator, &state->instance), "Couldnt create instance");
}

void logInfo(uint32_t instanceApiVersion) {
  EXPECT(vkEnumerateInstanceVersion(&instanceApiVersion), "couldnt enumerate instance version");
  uint32_t ApiVersionVarient = VK_API_VERSION_VARIANT(instanceApiVersion);
  uint32_t ApiVersionMajor = VK_API_VERSION_MAJOR(instanceApiVersion);
  uint32_t ApiVersionMinor = VK_API_VERSION_MINOR(instanceApiVersion);
  uint32_t ApiVersionPatch = VK_API_VERSION_PATCH(instanceApiVersion);

  printf("Vulkan API %i.%i.%i.%i\n", ApiVersionVarient, ApiVersionMajor, ApiVersionMinor, ApiVersionPatch);
  printf("GLFW %s\n", glfwGetVersionString());
}

void selectPhysicalDevice(State *state) {
  uint32_t count;
  EXPECT(vkEnumeratePhysicalDevices(state->instance, &count, NULL), "Couldnt enumerate physical devices count");
  EXPECT(count == 0, "couldnt find a vulkan supported physical device");
	VkResult result = vkEnumeratePhysicalDevices(state->instance, &(uint32_t){1}, &state->physicalDevice);
	if(result != VK_INCOMPLETE) EXPECT(result, "coulndt enumerate physical devices");
}

void createSurface(State *state) {
  EXPECT(glfwCreateWindowSurface(state->instance, state->window, state->allocator, &state->surface), "Couldnt create window surface");
}

void selectQueueFamily(State *state) {
  state->queueFamily = UINT32_MAX;
  uint32_t count;
  vkGetPhysicalDeviceQueueFamilyProperties(state->physicalDevice, &count, NULL);

	VkQueueFamilyProperties *queueFamilies = malloc(count*sizeof(VkQueueFamilyProperties));
  EXPECT(queueFamilies == NULL, "couldnt allocate memory");
  vkGetPhysicalDeviceQueueFamilyProperties(state->physicalDevice, &count, queueFamilies);

  for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < count; ++queueFamilyIndex) {
    VkQueueFamilyProperties properties = queueFamilies[queueFamilyIndex];
    if(properties.queueFlags & VK_QUEUE_GRAPHICS_BIT && glfwGetPhysicalDevicePresentationSupport(state->instance, state->physicalDevice, queueFamilyIndex)) {
      state->queueFamily = queueFamilyIndex;
      break;
    }
  }

  EXPECT(state->queueFamily == UINT32_MAX, "Couldnt find a suitable queue family");
  free(queueFamilies);
}

void createDevice(State *state) {
  EXPECT(vkCreateDevice(state->physicalDevice, &(VkDeviceCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pQueueCreateInfos = &(VkDeviceQueueCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = state->queueFamily,
      .queueCount = 1,
      .pQueuePriorities = &(float){1.0},
    },
    .queueCreateInfoCount = 1,
    .enabledExtensionCount = 1,
    .ppEnabledExtensionNames = &(const char *) {VK_KHR_SWAPCHAIN_EXTENSION_NAME},
  }, state->allocator, &state->device), "couldnt create device and queues")
}

void getQueue(State *state) {
  vkGetDeviceQueue(state->device, state->queueFamily, 0, &state->queue);
}

uint32_t clamp(uint32_t value, uint32_t min, uint32_t max) {
  if(value < min) {
    return min;
  } else if(value > max) {
    return max;
  }
  return value;
}

void createSwapchain(State *state) {
  VkSurfaceCapabilitiesKHR capabilities;
  EXPECT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->physicalDevice, state->surface, &capabilities), "failed to get surface capabilities");
  uint32_t formatCount;
  EXPECT(vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, NULL), "couldnt get surface formats");
  VkSurfaceFormatKHR *formats = malloc(formatCount*sizeof(VkSurfaceFormatKHR));
  EXPECT(!formats, "Couldnt allocate memory to var formats");
  EXPECT(vkGetPhysicalDeviceSurfaceFormatsKHR(state->physicalDevice, state->surface, &formatCount, formats), "couldnt get surface formats");

  uint32_t formatIndex = 0;

  for (int i = 0; i < formatCount; i++) {
    VkSurfaceFormatKHR format = formats[i];
    if(format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR && format.format == VK_FORMAT_B8G8R8A8_SRGB) {
      formatIndex = i;
      break;
    }
  }

  VkSurfaceFormatKHR format = formats[formatIndex];

  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
  uint32_t presentModeCount;
  EXPECT(vkGetPhysicalDeviceSurfacePresentModesKHR(state->physicalDevice, state->surface, &presentModeCount, NULL), "couldnt get surface present modes");
  VkPresentModeKHR *presentModes = malloc(presentModeCount*sizeof(VkPresentModeKHR));
  EXPECT(!presentModes, "couldnt allocate present modes memory");

  uint32_t presentModeIndex = UINT32_MAX;

  for (int i = 0; i < presentModeCount; i++) {
    VkPresentModeKHR mode = presentModes[i];
    if(mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      presentModeIndex = i;
      break;
    }
  }

  if(presentModeIndex != UINT32_MAX) {
    presentMode = VK_PRESENT_MODE_FIFO_KHR;
  }

  free(formats);
  free(presentModes);

  VkSwapchainKHR swapchain;

  EXPECT(vkCreateSwapchainKHR(state->device, &(VkSwapchainCreateInfoKHR) {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = state->surface,
    .queueFamilyIndexCount = 1,
    .pQueueFamilyIndices = &state->queueFamily,
    .clipped = true,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .oldSwapchain = state->swapchain,
    .preTransform = capabilities.currentTransform,
    .imageExtent = capabilities.currentExtent,
    .imageFormat = format.format,
    .imageColorSpace = format.colorSpace,
    .presentMode = presentMode,
  	.minImageCount = clamp(3, capabilities.minImageCount, capabilities.maxImageCount ? capabilities.maxImageCount : UINT32_MAX),
  }, state->allocator, &swapchain), "Couldnt create swapchain");

  if(!state->swapchainImageCount) {
    for (int i = 0; i < state->swapchainImageCount; i++) {
      vkDestroyImageView(state->device, state->swapchainImageViews[i], state->allocator);
    }
    free(state->swapchainImageViews);
    state->swapchainImageViews = NULL;
    free(state->swapchainImages);
    state->swapchainImages = NULL;
  }

  EXPECT(vkGetSwapchainImagesKHR(state->device, state->swapchain, &state->swapchainImageCount, NULL), "couldnt get swapchain images count");
  state->swapchainImages = malloc(state->swapchainImageCount*sizeof(VkImage));
  EXPECT(!state->swapchainImages, "couldnt allocate memory for swapchain images");
  EXPECT(vkGetSwapchainImagesKHR(state->device, state->swapchain, &state->swapchainImageCount, state->swapchainImages), "couldnt get swapchain images");
  state->swapchainImageViews = malloc(state->swapchainImageCount*sizeof(VkImageView));
  EXPECT(!state->swapchainImageViews, "Couldnt allocate memory for swapchain image views");

  for (int i = 0; i < state->swapchainImageCount; i++) {
    EXPECT(vkCreateImageView(state->device, &(VkImageViewCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .format = format.format,
      .image = state->swapchainImages[i],
      .components = (VkComponentMapping) {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      .subresourceRange = (VkImageSubresourceRange) {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .layerCount = 1,
        .levelCount = 1,
      },
      .viewType = VK_IMAGE_VIEW_TYPE_2D,

    }, state->allocator, &state->swapchainImageViews[i]), "Couldnt create image view %i", i)
  }
}

void init(State *state) {
  setupErrorHandling();
  logInfo(state->apiVersion);

  createWindow(state);
  createInstance(state);

  selectPhysicalDevice(state);
  createSurface(state);
  selectQueueFamily(state);
  createDevice(state);

  getQueue(state);
}

void loop(State *state) {
  while(!glfwWindowShouldClose(state->window)) {
    glfwPollEvents;
  }
}

void cleanup(State *state) {
  for (int i = 0; i < state->swapchainImageCount; i++) {
    vkDestroyImageView(state->device, state->swapchainImageViews[i], state->allocator);
  }
	free(state->swapchainImageViews);
	free(state->swapchainImages);
  vkDestroyDevice(state->device, state->allocator);
  vkDestroySurfaceKHR(state->instance, state->surface, state->allocator);
  vkDestroyInstance(state->instance, state->allocator);
  state->window = NULL;
}

int main() {
  State state = {
    .windowTitle = "meow",
    .windowWidth = 720,
    .windowHeight = 480,
    .windowFullscreen = false,
		.apiVersion = VK_API_VERSION_1_3,
  };

  init(&state);
  loop(&state);
  cleanup(&state);

  return EXIT_SUCCESS;
}
