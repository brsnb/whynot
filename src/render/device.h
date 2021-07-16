/*
===========================================================================

whynot::device.h: gpu and logical device

===========================================================================
*/
#pragma once

#include "render_types.h"
#include "vk.h"

typedef struct wn_queue_t
{
    VkQueue queue;
    uint32_t fanily_idx;
    uint32_t queue_idx;
} wn_queue_t;

typedef struct wn_device_t
{
    VkDevice device;

    wn_queue_t graphics_queue;

    VkPhysicalDevice gpu;

    VkPhysicalDeviceProperties gpu_properties;
    VkPhysicalDeviceFeatures gpu_features;
    VkPhysicalDeviceMemoryProperties gpu_memory_properties;
} wn_device_t;
