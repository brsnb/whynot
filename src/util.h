#pragma once

#include "log.h"
#include <vulkan/vulkan.h>

#define WN_VK_CHECK(f)                                                                                           \
    {                                                                                                                  \
        VkResult res = (f);                                                                                            \
        if (res != VK_SUCCESS) {                                                                                       \
            log_fatal("VkResult is \"%s\" in %s at line %d\n", wn_vk_result_to_string(res), __FILE__, __LINE__);                               \
            assert(res);                                                                                               \
        }                                                                                                              \
    }

VKAPI_ATTR VkBool32 VKAPI_CALL wn_util_debug_message_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    char* message_type = "";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        message_type = "GENERAL";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        message_type = "VALIDATION";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        message_type = "PERFORMANCE";
    }

    const char* message_id_name = pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "";
    const char* message = pCallbackData->pMessage ? pCallbackData->pMessage : "";

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        log_info("%s [%s (%d)] : %s\n", message_type, message_id_name, pCallbackData->messageIdNumber, message);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        log_warn("%s [%s (%d)] : %s\n", message_type, message_id_name, pCallbackData->messageIdNumber, message);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        log_error("%s [%s (%d)] : %s\n", message_type, message_id_name, pCallbackData->messageIdNumber, message);
    }
    return VK_FALSE;
}

char* wn_vk_result_to_string(VkResult result) {
    switch (result) {
#define TO_STR(s) case VK_ ##s: return #s
    TO_STR(SUCCESS);
    TO_STR(NOT_READY);
    TO_STR(TIMEOUT);
    TO_STR(EVENT_SET);
    TO_STR(EVENT_RESET);
    TO_STR(INCOMPLETE);
    TO_STR(ERROR_OUT_OF_HOST_MEMORY);
    TO_STR(ERROR_OUT_OF_DEVICE_MEMORY);
    TO_STR(ERROR_INITIALIZATION_FAILED);
    TO_STR(ERROR_DEVICE_LOST);
    TO_STR(ERROR_MEMORY_MAP_FAILED);
    TO_STR(ERROR_LAYER_NOT_PRESENT);
    TO_STR(ERROR_EXTENSION_NOT_PRESENT);
    TO_STR(ERROR_FEATURE_NOT_PRESENT);
    TO_STR(ERROR_INCOMPATIBLE_DRIVER);
    TO_STR(ERROR_TOO_MANY_OBJECTS);
    TO_STR(ERROR_FORMAT_NOT_SUPPORTED);
    TO_STR(ERROR_FRAGMENTED_POOL);
    TO_STR(ERROR_UNKNOWN);
    TO_STR(ERROR_OUT_OF_POOL_MEMORY);
    TO_STR(ERROR_INVALID_EXTERNAL_HANDLE);
    TO_STR(ERROR_FRAGMENTATION);
    TO_STR(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
    TO_STR(ERROR_SURFACE_LOST_KHR);
    TO_STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
    TO_STR(SUBOPTIMAL_KHR);
    TO_STR(ERROR_OUT_OF_DATE_KHR);
    TO_STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
    TO_STR(ERROR_VALIDATION_FAILED_EXT);
    TO_STR(ERROR_INVALID_SHADER_NV);
    TO_STR(ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
    TO_STR(ERROR_NOT_PERMITTED_EXT);
    TO_STR(ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
    TO_STR(THREAD_IDLE_KHR);
    TO_STR(THREAD_DONE_KHR);
    TO_STR(OPERATION_DEFERRED_KHR);
    TO_STR(OPERATION_NOT_DEFERRED_KHR);
    TO_STR(PIPELINE_COMPILE_REQUIRED_EXT);
    TO_STR(RESULT_MAX_ENUM);
#undef TO_STR
        default:
        return "UNKNOWN_ERROR";
   }
}
