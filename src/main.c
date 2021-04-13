#include "log.h"
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>
#include <xcb/xcb.h>
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include "util.h"

#define NAME "whynot"
#define VK_API_VERSION VK_API_VERSION_1_2

#ifndef NDEBUG
#define DEBUG_ENABLED
#endif

struct _Core {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
} core;

void wn_init_vulkan(void)
{
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = NAME,
        .pEngineName = NAME,
        .apiVersion = VK_API_VERSION,
    };

    const char** exts = NULL;
    arrput(exts, VK_KHR_SURFACE_EXTENSION_NAME);
    arrput(exts, VK_KHR_XCB_SURFACE_EXTENSION_NAME);

    const char** layers = NULL;
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = arrlen(exts),
        .ppEnabledExtensionNames = exts,
        .enabledLayerCount = arrlen(layers),
        .ppEnabledLayerNames = layers,
    };

#ifdef DEBUG_ENABLED
    arrput(exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    arrput(layers, "VK_LAYER_KHRONOS_validation");

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
    instance_info.enabledExtensionCount = arrlen(exts);
    instance_info.ppEnabledExtensionNames = exts;
    instance_info.enabledLayerCount = arrlen(layers);
    instance_info.ppEnabledLayerNames = layers;
    instance_info.pNext = &debug_messenger_info;
#endif

    VK_CHECK_RESULT(vkCreateInstance(&instance_info, NULL, &core.instance));

#ifdef DEBUG_ENABLED
    PFN_vkCreateDebugUtilsMessengerEXT debug_messenger_pfn
        = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(core.instance, "vkCreateDebugUtilsMessengerEXT");
    assert(debug_messenger_pfn != NULL);
    VK_CHECK_RESULT(debug_messenger_pfn(core.instance, &debug_messenger_info, NULL, &core.debug_messenger));
#else
    core.debug_messenger = NULL;
#endif
}

// TODO
void wn_destroy(void)
{
#ifdef DEBUG_ENABLED
    PFN_vkDestroyDebugUtilsMessengerEXT messenger_destroyer
        = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(core.instance, "vkDestroyDebugUtilsMessengerEXT");
    if (messenger_destroyer != NULL) {
        messenger_destroyer(core.instance, core.debug_messenger, NULL);
    } else {
    }
#endif
    vkDestroyInstance(core.instance, NULL);
}

int main(void)
{
#ifdef DEBUG_ENABLED
    log_set_level(LOG_TRACE);
#else
    log_set_level(LOG_ERROR);
#endif
    wn_init_vulkan();
    wn_destroy();
    return 0;
}