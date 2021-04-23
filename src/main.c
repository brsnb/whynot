#include <assert.h>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#define STB_DS_IMPLEMENTATION
#define STBDS_NO_SHORT_NAMES
#include "stb_ds.h"
#include "util.h"
#include "log.h"

#define NAME "whynot"
#define VK_API_VERSION VK_API_VERSION_1_2

#ifndef NDEBUG
#define DEBUG_ENABLED
#endif

struct _WnCore {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
} core;

typedef struct {
    uint32_t compute;
    uint32_t graphics;
    uint32_t present;
    uint32_t transfer;
    VkQueueFlags supported;
} WnQueueFamilyIndices;

typedef struct {
    VkQueue compute;
    VkQueue graphics;
    VkQueue present;
    VkQueue transfer;
} WnQueues;

struct _WnDevice {
    VkDevice logical;
    VkPhysicalDevice physical;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    WnQueueFamilyIndices queue_family_indices;
    WnQueues queues;
} device;

struct _Swapchain {
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_formats;
    VkExtent2D surface_extent;
} swapchain;


void wn_init_swapchain(GLFWwindow* window) {
    /*
        create surface
    */
    WN_VK_CHECK(glfwCreateWindowSurface(core.instance, window, NULL, &swapchain.surface));

    /*
        get surface info
    */
    VkSurfaceCapabilitiesKHR surface_caps;
    WN_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical, swapchain.surface, &surface_caps));

    uint32_t format_count = 0;
    WN_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical, swapchain.surface, &format_count, NULL));
    assert(format_count > 0);

    VkSurfaceFormatKHR* surface_formats = NULL;
    stbds_arrsetlen(surface_formats, format_count);
    WN_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical, swapchain.surface, &format_count, surface_formats));

    uint32_t present_mode_count = 0;
    WN_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical, swapchain.surface, &present_mode_count, NULL));
}

// TODO: Find other queues
WnQueueFamilyIndices wn_find_queue_families(void)
{
    WnQueueFamilyIndices qfi;
    uint32_t nqf = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device.physical, &nqf, NULL);
    VkQueueFamilyProperties qfp[nqf];
    vkGetPhysicalDeviceQueueFamilyProperties(device.physical, &nqf, qfp);

    for (uint32_t i = 0; i < nqf; ++i) {
        if (qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 supports_present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device.physical, i, swapchain.surface, &supports_present);
            // FIXME: Only selecting graphics + present queue, not seperate
            if (!supports_present) {
                continue;
            }

            qfi.graphics = i;
            qfi.present = i;
            qfi.supported |= VK_QUEUE_GRAPHICS_BIT;
            break;
        }
    }

    if (!qfi.supported & VK_QUEUE_GRAPHICS_BIT) {
        log_fatal("No graphics/present queue found");
        exit(1);
    }

    return qfi;
}

void wn_init_vulkan(GLFWwindow* window)
{
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = NAME,
        .pEngineName = NAME,
        .apiVersion = VK_API_VERSION,
    };

    uint32_t ext_count = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&ext_count);
    const char** exts = NULL;

    for (uint32_t i = 0; i < ext_count; i++) {
        stbds_arrput(exts, glfw_exts[i]);
    }

    const char** layers = NULL;
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = stbds_arrlen(exts),
        .ppEnabledExtensionNames = exts,
        .enabledLayerCount = stbds_arrlen(layers),
        .ppEnabledLayerNames = layers,
    };

#ifdef DEBUG_ENABLED
    stbds_arrput(exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    stbds_arrput(layers, "VK_LAYER_KHRONOS_validation");

    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = wn_util_debug_message_callback,
        .pUserData = NULL,
    };

    // FIXME: This seems hacky
    instance_info.enabledExtensionCount = stbds_arrlen(exts);
    instance_info.ppEnabledExtensionNames = exts;
    instance_info.enabledLayerCount = stbds_arrlen(layers);
    instance_info.ppEnabledLayerNames = layers;
    instance_info.pNext = &debug_messenger_info;
#endif

    /*
        instance
    */
    WN_VK_CHECK(vkCreateInstance(&instance_info, NULL, &core.instance));

#ifdef DEBUG_ENABLED
    PFN_vkCreateDebugUtilsMessengerEXT debug_messenger_pfn
        = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(core.instance, "vkCreateDebugUtilsMessengerEXT");
    assert(debug_messenger_pfn);
    WN_VK_CHECK(debug_messenger_pfn(core.instance, &debug_messenger_info, NULL, &core.debug_messenger));
#else
    core.debug_messenger = NULL;
#endif

    /*
        physical device
    */
    uint32_t device_count = 0;
    WN_VK_CHECK(vkEnumeratePhysicalDevices(core.instance, &device_count, NULL));
    assert(device_count > 0);

    VkPhysicalDevice physical_devices[device_count];
    VkResult err = vkEnumeratePhysicalDevices(core.instance, &device_count, physical_devices);

    // FIXME: make macro
    if (err) {
        log_error("Could not enumerate physical devices\n");
        exit(err);
    }

    // FIXME: Just picking first device rn
    device.physical = physical_devices[0];
    vkGetPhysicalDeviceProperties(device.physical, &device.properties);
    vkGetPhysicalDeviceFeatures(device.physical, &device.features);

    /*
        swapchain
    */
    wn_init_swapchain(window);

    /*
        logical device
    */

    // queue infos
    device.queue_family_indices = wn_find_queue_families();
    float* queue_prios = NULL;
    stbds_arrput(queue_prios, 1.0f);

    // FIXME: fix when supporting seperate graphics/present queues
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = stbds_arrlen(queue_prios),
        .queueFamilyIndex = device.queue_family_indices.graphics,
        .pQueuePriorities = queue_prios,
    };

    VkPhysicalDeviceFeatures *enabled_features = NULL;
    const char** device_exts = NULL;
    stbds_arrput(device_exts, VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // device
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queue_info,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = enabled_features,
        .ppEnabledExtensionNames = device_exts,
        .enabledExtensionCount = stbds_arrlen(device_exts),
#ifdef DEBUG_ENABLED
        .enabledLayerCount = stbds_arrlen(layers),
        .ppEnabledLayerNames = layers,
#else
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
#endif
    };

    WN_VK_CHECK(vkCreateDevice(device.physical,&device_info, NULL, &device.logical));

    // device queues
    vkGetDeviceQueue(device.logical, device.queue_family_indices.graphics, 0, &device.queues.graphics);
    vkGetDeviceQueue(device.logical, device.queue_family_indices.present, 0, &device.queues.present);
}

// TODO
void wn_destroy(void)
{
#ifdef DEBUG_ENABLED
    PFN_vkDestroyDebugUtilsMessengerEXT messenger_destroyer
        = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(core.instance, "vkDestroyDebugUtilsMessengerEXT");
    if (messenger_destroyer) {
        messenger_destroyer(core.instance, core.debug_messenger, NULL);
    } else {
    }
#endif
    vkDestroySurfaceKHR(core.instance, swapchain.surface, NULL);
    vkDestroyDevice(device.logical, NULL);
    vkDestroyInstance(core.instance, NULL);
}

int main(void)
{
#ifdef DEBUG_ENABLED
    log_set_level(LOG_TRACE);
#else
    log_set_level(LOG_ERROR);
#endif

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "whynot", NULL, NULL);

    wn_init_vulkan(window);
    wn_destroy();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
