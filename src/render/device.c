/*
===========================================================================

whynot::device.c: gpu and logical device

===========================================================================
*/

#include "device.h"

#include "util_vk.inl"

#include "log.h"

#include <assert.h>
#include <stdlib.h>

wn_result find_queue_family_idx(
    VkPhysicalDevice gpu,
    wn_queue_type type,
    uint32_t* queue_family_idx,
    uint32_t* n_queues)
{
    uint32_t n_qf = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n_qf, NULL);
    VkQueueFamilyProperties* qfp = malloc(n_qf * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n_qf, qfp);

    for (uint32_t i = 0; i < n_qf; i++)
    {
        VkQueueFamilyProperties props = qfp[i];

        if (props.queueFlags & wn_queue_type_to_vk(type))
        {
            if (queue_family_idx)
            {
                *queue_family_idx = i;
            }
            if (n_queues)
            {
                *n_queues = props.queueCount;
            }
            break;
        }
    }

    free(qfp);

    return WN_OK;
}

wn_result wn_render_device_create(VkPhysicalDevice gpu)
{
    wn_device_t device = { 0 };

    device.gpu = gpu;

    vkGetPhysicalDeviceProperties(gpu, &device.gpu_properties);
    vkGetPhysicalDeviceFeatures(gpu, &device.gpu_features);
    vkGetPhysicalDeviceMemoryProperties(gpu, &device.gpu_memory_properties);

    wn_queue_t graphics_queue = { 0 };
    find_queue_family_idx(gpu, WN_QUEUE_TYPE_GRAPHICS, &graphics_queue.fanily_idx, NULL);
    const float default_queue_prio = 0.0f;
    // FIXME: hardcoded bad, there could be any number of acual queues based on how many are
    // available in the queue family and if it is even more efficient to do so
    VkDeviceQueueCreateInfo queue_infos[] = { {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .queueFamilyIndex = graphics_queue.fanily_idx,
        .pQueuePriorities = &default_queue_prio,
        .flags = 0,
        .pNext = NULL,
    } };

    // FIXME: placeholder, you just have to check if feature requests are supported somehow
    if (!device.gpu_features.samplerAnisotropy)
    {
        log_fatal("sampler anisotropy not supported on gpu");
        return WN_ERR;
    }
    VkPhysicalDeviceFeatures enabled_features = {
        .samplerAnisotropy = true,
    };
    const char* device_exts = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_infos,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = &enabled_features,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &device_exts,
        // NOTE: this may not be compatible with < 1.2 vulkan implementation
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .flags = 0,
        .pNext = NULL,
    };

    WN_VK_CHECK(vkCreateDevice(gpu, &device_info, NULL, &device.device));

    log_info("Getting graphics device queue at idx: %d", graphics_queue.fanily_idx);
    vkGetDeviceQueue(device.device, graphics_queue.fanily_idx, 0, &graphics_queue.queue);

    // FIXME: queue_idx is always 0 currently
    device.graphics_queue = graphics_queue;

    return WN_OK;
}

void wn_render_device_destroy(wn_device_t* device)
{
    vkDestroyDevice(device->device, NULL);
}

// NOTE: For future reference if applicable
#if 0
// NOTE: spec allows for separate graphics and present queues, but it does not exist in any
// implementation afaik
typedef struct wn_qfi_t
{
    uint32_t compute;
    uint32_t graphics;
    uint32_t present;
    uint32_t transfer;
    VkQueueFlags supported;
} wn_qfi_t;

wn_qfi_t wn_find_queue_families(VkPhysicalDevice gpu)
{
    wn_qfi_t qfi = { 0 };
    uint32_t n_qf = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n_qf, NULL);
    VkQueueFamilyProperties qfp[n_qf];
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n_qf, qfp);

    for (uint32_t i = 0; i < n_qf; i++)
    {
        VkQueueFamilyProperties props = qfp[i];

        log_info("QUEUE IDX: %d, QUEUE COUNT: %d", i, props.queueCount);

        // TODO: there is such a thing as using compute queue for present, look into it
        if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            if (!(qfi.supported & VK_QUEUE_GRAPHICS_BIT))
            {
                log_info("Picking graphics queue idx: %d", i);
                qfi.graphics = i;
                qfi.present = i;
                qfi.supported |= VK_QUEUE_GRAPHICS_BIT;
            }
        }

        // try to find dedicated compute queue
        if (props.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            if (!(qfi.supported & VK_QUEUE_COMPUTE_BIT) && qfi.graphics != i)
            {
                qfi.compute = i;
                qfi.supported |= VK_QUEUE_COMPUTE_BIT;
            }
        }

        // try to find non-graphics transfer queue
        if (props.queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            if (!(qfi.supported & VK_QUEUE_TRANSFER_BIT)
                && !(props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                && !(props.queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                log_info("Picking transfer queue idx: %d", i);
                qfi.transfer = i;
                qfi.supported |= VK_QUEUE_TRANSFER_BIT;
            }
        }
    }

    if (!(qfi.supported & VK_QUEUE_GRAPHICS_BIT))
    {
        log_fatal("No graphics/present queue found");
        exit(EXIT_FAILURE);
    }

    // TODO: there's probably a better way to do this
    if (!(qfi.supported & VK_QUEUE_COMPUTE_BIT))
    {
        log_info("Picking compute queue idx: %d", qfi.graphics);
        qfi.compute = qfi.graphics;
    }
    if (!(qfi.supported & VK_QUEUE_TRANSFER_BIT))
    {
        qfi.transfer = qfi.graphics;
    }

    return qfi;
}
#endif