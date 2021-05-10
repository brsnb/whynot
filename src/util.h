/*
===========================================================================

whynot: util.h

===========================================================================
*/

#pragma once

#include "log.h"
#include <assert.h>
#include <bits/stdint-uintn.h>
#include <shaderc/shaderc.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define WN_VK_CHECK(f)                                                         \
    {                                                                          \
        VkResult res = (f);                                                    \
        if (res != VK_SUCCESS) {                                               \
            log_fatal("VkResult is \"%s\" in %s at line %d\n",                 \
                      wn_vk_result_to_string(res), __FILE__, __LINE__);        \
            assert(res);                                                       \
        }                                                                      \
    }

#define WN_MAX(a, b)                                                           \
    ({                                                                         \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        _a > _b ? _a : _b;                                                     \
    })

#define WN_MIN(a, b)                                                           \
    ({                                                                         \
        __typeof__(a) _a = (a);                                                \
        __typeof__(b) _b = (b);                                                \
        _a < _b ? _a : _b;                                                     \
    })

VKAPI_ATTR VkBool32 VKAPI_CALL wn_util_debug_message_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData) {
    char *message_type = "";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        message_type = "GENERAL";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        message_type = "VALIDATION";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        message_type = "PERFORMANCE";
    }

    const char *message_id_name =
        pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "";
    const char *message =
        pCallbackData->pMessage ? pCallbackData->pMessage : "";

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        log_info("%s [%s (%d)] : %s\n", message_type, message_id_name,
                 pCallbackData->messageIdNumber, message);
    } else if (messageSeverity &
               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        log_warn("%s [%s (%d)] : %s\n", message_type, message_id_name,
                 pCallbackData->messageIdNumber, message);
    } else if (messageSeverity &
               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        log_error("%s [%s (%d)] : %s\n", message_type, message_id_name,
                  pCallbackData->messageIdNumber, message);
    }
    return VK_FALSE;
}

char *wn_vk_result_to_string(VkResult result) {
    switch (result) {
#define TO_STR(s)                                                              \
    case VK_##s:                                                               \
        return #s
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

typedef struct {
    shaderc_compiler_t compiler;
} wn_shader_loader_t;

wn_shader_loader_t wn_util_create_shader_loader() {
    wn_shader_loader_t loader = {
        .compiler = shaderc_compiler_initialize(),
    };

    return loader;
}

void wn_util_destroy_shader_loader(wn_shader_loader_t *loader) {
    shaderc_compiler_release(loader->compiler);
}

typedef struct {
    char *file_name;
    char *content;
    size_t size;
} wn_file_source_t;

typedef struct {
    wn_file_source_t source;
    uint32_t *spirv;
    char *entry;
    size_t size;
    VkShaderStageFlags shader_stage;
} wn_shader_t;

// NOTE: Inefficient but w/e really
wn_file_source_t wn_util_read_file(const char *file_name) {
    FILE *file = fopen(file_name, "rb");
    char *buffer = NULL;
    wn_file_source_t ret;
    if (file) {
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        rewind(file);
        buffer = malloc(sizeof(char) * file_size);
        // FIXME
        assert(buffer);
        size_t read_size = fread(buffer, sizeof(char), file_size, file);
        // FIXME
        if (file_size != read_size) {
            log_fatal("Error reading shader file %s", file_name);
            exit(EXIT_FAILURE);
        }

        ret.file_name = file_name;
        ret.content = buffer;
        ret.size = file_size;

        fclose(file);
    } else {
        log_fatal("Couldn't load file");
        exit(EXIT_FAILURE);
    }

    return ret;
}

// FIXME: none of this gets freed
wn_shader_t wn_util_load_shader(wn_shader_loader_t *loader,
                                const char *file_name,
                                VkShaderStageFlags shader_stage) {
    wn_file_source_t content = wn_util_read_file(file_name);
    shaderc_shader_kind kind;
    switch (shader_stage) {
    case VK_SHADER_STAGE_COMPUTE_BIT:
        kind = shaderc_compute_shader;
        break;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        kind = shaderc_fragment_shader;
        break;
    case VK_SHADER_STAGE_VERTEX_BIT:
        kind = shaderc_vertex_shader;
        break;
    default:
        log_fatal("Need to specify a shader stage");
        exit(EXIT_FAILURE);
        break;
    }

    const shaderc_compilation_result_t result =
        shaderc_compile_into_spv(loader->compiler, content.content,
                                 content.size, kind, file_name, "main", NULL);

    char *shader_err = shaderc_result_get_error_message(result);

    if (!result) {
        log_error("Error compiling shader: %s.", file_name);
        exit(EXIT_FAILURE);
    }
    if (shader_err[0] != '\0') {
        log_debug("Shader error in compiling %s: %s", file_name, shader_err);
    }

    wn_shader_t shader = {
        .source = content,
        .entry = "main",
        .shader_stage = shader_stage,
        .size = shaderc_result_get_length(result),
        .spirv = (uint32_t *)shaderc_result_get_bytes(result),
    };

    return shader;
}
