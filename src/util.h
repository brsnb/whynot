#pragma once

#include "log.h"
#include <vulkan/vulkan.h>

#define VK_CHECK_RESULT(f)                                                                                             \
    {                                                                                                                  \
        VkResult res = (f);                                                                                            \
        if (res != VK_SUCCESS) {                                                                                       \
            log_fatal("Fatal: VkResult is \"%d\" in %s at line %d\n", res, __FILE__, __LINE__);                        \
            assert(res == VK_SUCCESS);                                                                                 \
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

    const char* message_id_name = pCallbackData->pMessageIdName != NULL ? pCallbackData->pMessageIdName : "";
    const char* message = pCallbackData->pMessage != NULL ? pCallbackData->pMessage : "";

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        log_info("%s [%s (%d)] : %s\n", message_type, message_id_name, pCallbackData->messageIdNumber, message);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        log_warn("%s [%s (%d)] : %s\n", message_type, message_id_name, pCallbackData->messageIdNumber, message);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        log_error("%s [%s (%d)] : %s\n", message_type, message_id_name, pCallbackData->messageIdNumber, message);
    }
    return VK_FALSE;
}