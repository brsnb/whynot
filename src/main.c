/*
===========================================================================

whynot::main.c

===========================================================================
*/

#include "util.h"

#include "log.h"
#define STB_DS_IMPLEMENTATION
#define STBDS_NO_SHORT_NAMES
#include "stb_ds.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// clang-format off
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
// clang-format on
#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define ENGINE_NAME "whynot"
#define VK_API_VERSION VK_API_VERSION_1_2

#define MAX_FRAMES_IN_FLIGHT 2

typedef union wn_v2f_t
{
    float v2f[2];
    struct
    {
        float x, y;
    };
    struct
    {
        float u, v;
    };
    struct
    {
        float r, g;
    };
} wn_v2f_t;

typedef union wn_v3f_t
{
    float v3f[3];
    struct
    {
        float x, y, z;
    };
    struct
    {
        float u, v, w;
    };
    struct
    {
        float r, g, b;
    };
} wn_v3f_t;

typedef union wn_v4f_t
{
    float v4f[4];
    struct
    {
        float x, y, z, w;
    };
    struct
    {
        float r, g, b, a;
    };
} wn_v4f_t;

typedef union wn_mat4f_t
{
    float mat4f[4][4];
    wn_v4f_t v4f[4];
    struct
    {
        float xx, xy, xz, xw;
        float yx, yy, yz, yw;
        float zx, zy, zz, zw;
        float wx, wy, wz, ww;
    };
} wn_mat4f_t;

/*
 * NOTE: source coordinates are right handed y-up, +z out, +x right
 *       dest coordinates are right handed y-down with z(depth) clip from 0.0(near) - 1.0(far)
 */

float wn_v3f_sqr_magnitude(const wn_v3f_t *v)
{
    return ((v->x * v->x) + (v->y * v->y) + (v->z * v->z));
}

float wn_v3f_magnitude(const wn_v3f_t *v)
{
    float sqr_mag = wn_v3f_sqr_magnitude(v);
    return sqrtf(sqr_mag);
}

void wn_v3f_normalize(wn_v3f_t *v)
{
    float inv_mag = 1.0f / wn_v3f_magnitude(v);
    v->x *= inv_mag;
    v->y *= inv_mag;
    v->z *= inv_mag;
}

wn_v3f_t wn_v3f_normalized(const wn_v3f_t *v)
{
    wn_v3f_t r = {
        .x = v->x,
        .y = v->y,
        .z = v->z,
    };
    wn_v3f_normalize(&r);
    return r;
}

float wn_v3f_dot(const wn_v3f_t *a, const wn_v3f_t *b)
{
    return ((a->x * b->x) + (a->y * b->y) + (a->z * b->z));
}

wn_v3f_t wn_v3f_cross(const wn_v3f_t *a, const wn_v3f_t *b)
{
    // clang-format off
    return (wn_v3f_t) {
        .x = (a->y * b->z) - (a->z * b->y),
        .y = (a->z * b->x) - (a->x * b->z),
        .z = (a->x * b->y) - (a->y * b->x),
    };
    // clang-format on
}

wn_v3f_t wn_v3f_minus(const wn_v3f_t *a, const wn_v3f_t *b)
{
    return (wn_v3f_t) { .x = (a->x - b->x), .y = (a->y - b->y), .z = (a->z - b->z) };
}

wn_mat4f_t wn_mat4f_look_at(const wn_v3f_t *eye, const wn_v3f_t *at, const wn_v3f_t *up)
{
    wn_v3f_t f = wn_v3f_minus(at, eye);
    wn_v3f_normalize(&f);
    wn_v3f_t r = wn_v3f_cross(&f, up);
    wn_v3f_normalize(&r);
    wn_v3f_t u = wn_v3f_cross(&r, &f);

    wn_v3f_t w = { -wn_v3f_dot(&r, eye), -wn_v3f_dot(&u, eye), wn_v3f_dot(&f, eye) };

    // clang-format off
    return (wn_mat4f_t) {
        .v4f = {
            (wn_v4f_t) {r.x, u.x, -f.x, 0.0f},
            (wn_v4f_t) {r.y, u.y, -f.y, 0.0f},
            (wn_v4f_t) {r.z, u.z, -f.z, 0.0f},
            (wn_v4f_t) {w.x, w.y, w.z, 1.0f},
        }
    };
    // clang-format on
}

wn_mat4f_t wn_mat4f_perspective(float vertical_fov, float aspect_ratio, float z_near, float z_far)
{
    float t = tanf(vertical_fov / 2.0f);
    float sy = 1.0f / t;
    float sx = sy / aspect_ratio;
    float nmf = z_near - z_far;

    // clang-format off
    return (wn_mat4f_t) {
        .v4f = {
            (wn_v4f_t) {sx, 0.0f, 0.0f, 0.0f},
            (wn_v4f_t) {0.0f, -sy, 0.0f, 0.0f},
            (wn_v4f_t) {0.0f, 0.0f, z_far / nmf, -1.0},
            (wn_v4f_t) {0.0f, 0.0f, z_near * z_far / nmf, 0.0f},
        }
    };
    // clang-format on
}

wn_mat4f_t wn_mat4f_from_rotation_z(float angle)
{
    float s = sinf(angle);
    float c = cosf(angle);

    // clang-format off
    return (wn_mat4f_t) {
        .v4f = {
            (wn_v4f_t) {c, s, 0.0f, 0.0f},
            (wn_v4f_t) {-s, c, 0.0f, 0.0f},
            (wn_v4f_t) {0.0f, 0.0f, 1.0f, 0.0f},
            (wn_v4f_t) {0.0f, 0.0f, 0.0f, 1.0f},
        }
    };
    // clang-format on
}

wn_mat4f_t wn_mat4f_indentity()
{
    // clang-format off
    return (wn_mat4f_t) {
        .v4f = {
            (wn_v4f_t) {1.0f, 0.0f, 0.0f, 0.0f},
            (wn_v4f_t) {0.0f, 1.0f, 0.0f, 0.0f},
            (wn_v4f_t) {0.0f, 0.0f, 1.0f, 0.0f},
            (wn_v4f_t) {0.0f, 0.0f, 0.0f, 1.0f},
        }
    };
    // clang-format on
}

typedef struct wn_vertex_t
{
    wn_v2f_t pos;
    wn_v3f_t color;
} wn_vertex_t;

VkVertexInputBindingDescription wn_vertex_get_input_binding_desc()
{
    VkVertexInputBindingDescription desc = {
        .binding = 0,
        .stride = sizeof(wn_vertex_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    return desc;
}

VkVertexInputAttributeDescription *wn_vertex_get_attribute_desc()
{
    static VkVertexInputAttributeDescription desc[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(wn_vertex_t, pos),
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(wn_vertex_t, color),
        },
    };

    return desc;
}

typedef struct wn_mvp_t
{
    wn_mat4f_t model;
    wn_mat4f_t view;
    wn_mat4f_t proj;
} wn_mvp_t;

#define N_VERTICES 4
wn_vertex_t vertices[4] = { { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
                            { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
                            { { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
                            { { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f } } };

#define N_INDICES 6
uint16_t indices[6] = { 0, 1, 2, 2, 3, 0 };

// try and wrap GLFW dependency...
typedef struct wn_window_t
{
    GLFWwindow *window;
} wn_window_t;

wn_window_t wn_window_new(int width, int height, char *title)
{
    if (!glfwInit())
    {
        log_fatal("Could not initialize GLFW");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *os_window = glfwCreateWindow(width, height, title, NULL, NULL);

    if (!os_window)
    {
        log_fatal("Could not create OS window");
        exit(EXIT_FAILURE);
    }

    wn_window_t window = {
        .window = os_window,
    };

    return window;
}

void wn_window_destroy(wn_window_t *window)
{
    glfwDestroyWindow(window->window);
    glfwTerminate();
}

void wn_window_get_framebuffer_size(wn_window_t *window, int *width, int *height)
{
    glfwGetFramebufferSize(window->window, width, height);
}

// in the style of..., but exits on failure
void wn_window_create_surface(VkInstance instance, wn_window_t *window, VkSurfaceKHR *surface)
{
    WN_VK_CHECK(glfwCreateWindowSurface(instance, window->window, NULL, surface));
}

int wn_window_should_close(wn_window_t *window)
{
    return glfwWindowShouldClose(window->window);
}

void wn_window_poll_events(void)
{
    glfwPollEvents();
}

const char **wn_window_get_required_exts(uint32_t *nexts)
{
    return glfwGetRequiredInstanceExtensions(nexts);
}

// FIXME: very temporary convienence, single command buffer allocation + submission + full
// vkQueueWaitIdle for the single command is incredibly inefficient
VkCommandBuffer wn_begin_command_buffer(VkDevice device, VkCommandPool command_pool)
{
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .commandBufferCount = 1,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .pNext = NULL,
    };

    VkCommandBuffer command_buffer = NULL;
    WN_VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, &command_buffer));

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
        .pNext = NULL,
    };

    WN_VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));

    return command_buffer;
}

void wn_end_command_buffer(
    VkDevice device,
    VkCommandPool command_pool,
    VkCommandBuffer command_buffer,
    VkQueue queue)
{
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .pNext = NULL,
    };

    vkQueueSubmit(queue, 1, &submit_info, NULL);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

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

// TODO: could be useful for queuesubmit and commandpool management
// it also allows for different queues to be created from same family if its available (with idx)
// currently all queues are created with same idx in vkgetdevicequeue
#if 0
typedef struct wn_queue_t
{
    VkQueue queue;
    uint32_t qfi;
    uint32_t idx;
} wn_queue_t;
#endif

wn_qfi_t wn_find_queue_families(VkPhysicalDevice gpu)
{
    wn_qfi_t qfi = { 0 };
    uint32_t n_qf = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n_qf, NULL);
    VkQueueFamilyProperties qfp[n_qf];
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n_qf, qfp);

    for (uint32_t i = 0; i < n_qf; ++i)
    {
        VkQueueFamilyProperties props = qfp[i];

        log_info("QUEUE IDX: %d, QUEUE COUNT: %d", i, props.queueCount);

        // TODO: there is such a thing as using compute queue for present, look into it
        if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            if (!(qfi.supported & VK_QUEUE_GRAPHICS_BIT))
            {
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
                qfi.compute = i;
                qfi.supported |= VK_QUEUE_COMPUTE_BIT;
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
        qfi.compute = qfi.graphics;
    }
    if (!(qfi.supported & VK_QUEUE_TRANSFER_BIT))
    {
        qfi.transfer = qfi.graphics;
    }

    return qfi;
}

typedef struct wn_device_t
{
    VkDevice device;

    wn_qfi_t qfi;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;

    VkPhysicalDevice gpu;

    VkPhysicalDeviceProperties gpu_properties;
    VkPhysicalDeviceFeatures gpu_features;
    VkPhysicalDeviceMemoryProperties gpu_memory_properties;
} wn_device_t;

wn_device_t wn_device_new(VkPhysicalDevice gpu)
{
    wn_device_t device = { 0 };

    device.gpu = gpu;

    vkGetPhysicalDeviceProperties(gpu, &device.gpu_properties);
    vkGetPhysicalDeviceFeatures(gpu, &device.gpu_features);

    device.qfi = wn_find_queue_families(gpu);

    const float default_queue_prio = 0.0f;

    // FIXME: hardcoded bad, there could be any number of acual queues based on how many are
    // available in the queue family and if it is even more efficient to do so
    VkDeviceQueueCreateInfo queue_infos[3] = { 0 };
    {
        VkDeviceQueueCreateInfo queue_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount = 1,
            .queueFamilyIndex = device.qfi.graphics,
            .pQueuePriorities = &default_queue_prio,
            .flags = 0,
            .pNext = NULL,
        };
        queue_infos[0] = queue_info;
    }
    {
        VkDeviceQueueCreateInfo queue_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount = 1,
            .queueFamilyIndex = device.qfi.compute,
            .pQueuePriorities = &default_queue_prio,
            .flags = 0,
            .pNext = NULL,
        };
        queue_infos[1] = queue_info;
    }
    {
        VkDeviceQueueCreateInfo queue_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount = 1,
            .queueFamilyIndex = device.qfi.transfer,
            .pQueuePriorities = &default_queue_prio,
            .flags = 0,
            .pNext = NULL,
        };
        queue_infos[2] = queue_info;
    }

    VkPhysicalDeviceFeatures *enabled_features = NULL;
    const char *device_exts = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_infos,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = enabled_features,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &device_exts,
        // NOTE: this may not be compatible with < 1.2 vulkan implementation
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .flags = 0,
        .pNext = NULL,
    };

    WN_VK_CHECK(vkCreateDevice(gpu, &device_info, NULL, &device.device));

    vkGetDeviceQueue(device.device, device.qfi.graphics, 0, &device.graphics_queue);
    vkGetDeviceQueue(device.device, device.qfi.compute, 0, &device.compute_queue);
    vkGetDeviceQueue(device.device, device.qfi.transfer, 0, &device.transfer_queue);

    device.present_queue = NULL;

    return device;
}

void wn_device_destroy(wn_device_t *device)
{
    vkDestroyDevice(device->device, NULL);
}

typedef struct wn_buffer_t
{
    VkBuffer handle;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkSharingMode sharing_mode;
} wn_buffer_t;

wn_buffer_t wn_buffer_new(
    const wn_device_t *device,
    VkBufferCreateInfo *info,
    VkMemoryPropertyFlags properties)
{

    wn_buffer_t buffer = { 0 };

    buffer.size = info->size;
    buffer.usage = info->usage;
    buffer.sharing_mode = info->sharingMode;

    WN_VK_CHECK(vkCreateBuffer(device->device, info, NULL, &buffer.handle));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device->device, buffer.handle, &mem_reqs);

    uint32_t mem_idx = 0;
    for (uint32_t i = 0; i < device->gpu_memory_properties.memoryTypeCount; i++)
    {
        if ((mem_reqs.memoryTypeBits & (1 << i))
            && (device->gpu_memory_properties.memoryTypes[i].propertyFlags & properties))
        {
            mem_idx = i;
        }
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_idx,
        .pNext = NULL,
    };

    WN_VK_CHECK(vkAllocateMemory(device->device, &alloc_info, NULL, &buffer.memory));

    WN_VK_CHECK(vkBindBufferMemory(device->device, buffer.handle, buffer.memory, 0));

    return buffer;
}

void wn_buffer_destroy(wn_buffer_t *buffer, VkDevice logical_device)
{
    vkDestroyBuffer(logical_device, buffer->handle, NULL);
    vkFreeMemory(logical_device, buffer->memory, NULL);
}

typedef struct wn_image_t
{
    VkImage handle;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkFormat format;
    VkImageLayout layout;
} wn_image_t;

wn_image_t wn_image_new(
    const wn_device_t *device,
    VkImageCreateInfo *info,
    VkMemoryPropertyFlags properties)
{
    wn_image_t image = { 0 };

    WN_VK_CHECK(vkCreateImage(device->device, info, NULL, &image.handle));

    VkMemoryRequirements mem_reqs = { 0 };
    vkGetImageMemoryRequirements(device->device, image.handle, &mem_reqs);

    // NOTE: duplicated x2
    uint32_t mem_idx = 0;
    for (uint32_t i = 0; i < device->gpu_memory_properties.memoryTypeCount; i++)
    {
        if ((mem_reqs.memoryTypeBits & (1 << i))
            && (device->gpu_memory_properties.memoryTypes[i].propertyFlags & properties))
        {
            mem_idx = i;
        }
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_idx,
        .pNext = NULL,
    };

    WN_VK_CHECK(vkAllocateMemory(device->device, &alloc_info, NULL, &image.memory));

    WN_VK_CHECK(vkBindImageMemory(device->device, image.handle, image.memory, 0));

    image.size = mem_reqs.size;
    image.format = info->format;
    image.layout = info->initialLayout;

    return image;
}

void wn_image_destroy(wn_image_t *image, VkDevice device)
{
    vkDestroyImage(device, image->handle, NULL);
    vkFreeMemory(device, image->memory, NULL);
}

// FIXME: bad function but the general idea is fine, reliance on wn_begin/end_command_buffer
void wn_transition_image_layout(
    const wn_device_t *device,
    VkCommandPool command_pool,
    wn_image_t *image,
    VkImageLayout layout)
{
    VkCommandBuffer cmd = wn_begin_command_buffer(device->device, command_pool);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .image = image->handle,
        .oldLayout = image->layout,
        .newLayout = layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcAccessMask = 0,
        .dstAccessMask = 0,
        .pNext = NULL,
    };

    VkPipelineStageFlags source_stage = 0;
    VkPipelineStageFlags dest_stage = 0;

    if (image->layout == VK_IMAGE_LAYOUT_UNDEFINED
        && layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (
        image->layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        && layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        log_fatal("Unsupported layout transition");
        exit(EXIT_FAILURE);
    }

    vkCmdPipelineBarrier(cmd, source_stage, dest_stage, 0, 0, NULL, 0, NULL, 1, &barrier);

    wn_end_command_buffer(device->device, command_pool, cmd, device->graphics_queue);
}

wn_image_t wn_texture_new(
    const wn_device_t *device,
    VkCommandPool command_pool,
    const char *filename)
{
    int width, height, channels;
    uint8_t *image_data = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);

    VkDeviceSize size = width * height * STBI_rgb_alpha;

    wn_buffer_t staging = wn_buffer_new(
        device,
        &(VkBufferCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .flags = 0,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .pNext = NULL,
        },
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void *data = NULL;
    WN_VK_CHECK(vkMapMemory(device->device, staging.memory, 0, size, 0, &data));
    memcpy(data, image_data, (size_t)size);
    vkUnmapMemory(device->device, staging.memory);

    stbi_image_free(image_data);

    // TODO: in actual api this would obv need to be parameterized (i.e. normal map texture format
    // would be different) the format is also not guaranteed to be supported, so caching supported
    // formats per gpu is probably a good idea
    VkImageCreateInfo tex_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .extent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .flags = 0,
        .pNext = NULL,
    };

    wn_image_t texture = wn_image_new(device, &tex_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    wn_transition_image_layout(
        device,
        command_pool,
        &texture,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkCommandBuffer cmd = wn_begin_command_buffer(device->device, command_pool);

    VkBufferImageCopy copy_region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0},
        .imageExtent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
    };

    vkCmdCopyBufferToImage(
        cmd,
        staging.handle,
        texture.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copy_region);

    wn_end_command_buffer(device->device, command_pool, cmd, device->transfer_queue);

    wn_buffer_destroy(&staging, device->device);

    return texture;
}

typedef struct wn_surface_t
{
    VkSurfaceKHR surface;

    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t n_formats;
    VkSurfaceFormatKHR *formats;
    uint32_t n_present_modes;
    VkPresentModeKHR *present_modes;

    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
} wn_surface_t;

typedef struct wn_frame_t
{
    VkImage image;
    VkImageView image_view;
    // VkImage depth_image;
    // VkImageView depth_image_view;
    VkFramebuffer framebuffer;
    wn_buffer_t ubo;
    VkDescriptorSet ubo_desc_set;
    // VkCommandBuffer draw_buffer // TODO: maybe??
    // VkSemaphore image_available
} wn_frame_t;

typedef struct wn_swapchain_t
{
    VkSwapchainKHR swapchain;
    uint32_t n_frames;
    wn_frame_t *frames;
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorPool descriptor_pool; // FIXME: this is getting sloppy
} wn_swapchain_t;

typedef struct wn_render_t
{
    VkInstance instance;

    wn_device_t device;

    wn_surface_t surface;

    wn_swapchain_t swapchain;

    VkSemaphore *image_available;
    VkSemaphore *render_finished;
    VkFence *in_flight;
    VkFence *image_in_flight;
    size_t current_frame;

    VkRenderPass render_pass;

    VkPipelineLayout graphics_pipeline_layout;
    VkPipeline graphics_pipeline;

    VkCommandPool command_pool;
    VkCommandBuffer *command_buffers;

    wn_image_t color_texture;

    wn_buffer_t vertex_buffer;
    wn_buffer_t index_buffer;

    // debug
    VkDebugUtilsMessengerEXT debug_messenger;
    bool debug_enabled;
} wn_render_t;

wn_surface_t wn_surface_new(VkSurfaceKHR window_surface, VkPhysicalDevice gpu, wn_window_t *window)
{
    wn_surface_t surface = { 0 };

    assert(window_surface);
    surface.surface = window_surface;

    // capabilities
    WN_VK_CHECK(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface.surface, &surface.capabilities));

    // formats
    WN_VK_CHECK(
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface.surface, &surface.n_formats, NULL));
    assert(surface.n_formats > 0);

    surface.formats = (VkSurfaceFormatKHR *)malloc(surface.n_formats * sizeof(VkSurfaceFormatKHR));
    assert(surface.formats);

    WN_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        gpu,
        surface.surface,
        &surface.n_formats,
        surface.formats));

    // present modes
    WN_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        gpu,
        surface.surface,
        &surface.n_present_modes,
        NULL));
    assert(surface.n_present_modes > 0);

    surface.present_modes
        = (VkPresentModeKHR *)malloc(surface.n_present_modes * sizeof(VkPresentModeKHR));
    assert(surface.present_modes);

    WN_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        gpu,
        surface.surface,
        &surface.n_present_modes,
        surface.present_modes));

    /*
     *  choose desired format/present mode/extent
     */
    surface.format = surface.formats[0];
    for (uint32_t i = 0; i < surface.n_formats; i++)
    {
        if (surface.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB
            && surface.formats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
        {
            surface.format = surface.formats[i];
        }
    }

    surface.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < surface.n_present_modes; i++)
    {
        if (surface.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            surface.present_mode = surface.present_modes[i];
        }
    }

    VkSurfaceCapabilitiesKHR surface_caps = surface.capabilities;

    if (surface_caps.currentExtent.width != UINT32_MAX)
    {
        surface.extent = surface_caps.currentExtent;
    }
    else
    {
        int width, height;
        wn_window_get_framebuffer_size(window, &width, &height);

        VkExtent2D surface_extent = {
            width = (uint32_t)width,
            height = (uint32_t)height,
        };

        surface.extent.width = wn_u32_max(
            surface_caps.maxImageExtent.width,
            wn_u32_min(surface_caps.maxImageExtent.width, surface_extent.width));
        surface.extent.height = wn_u32_max(
            surface_caps.maxImageExtent.height,
            wn_u32_min(surface_caps.maxImageExtent.height, surface_extent.height));
    }

    return surface;
}

void wn_surface_destroy(wn_surface_t *surface)
{
    free(surface->formats);
    free(surface->present_modes);
}

void wn_surface_setup_present_queue(const wn_surface_t *surface, wn_device_t *device)
{
    if (!device->present_queue)
    {
        VkBool32 is_supported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            device->gpu,
            device->qfi.present,
            surface->surface,
            &is_supported);

        if (is_supported)
        {
            vkGetDeviceQueue(device->device, device->qfi.present, 0, &device->present_queue);
        }
        else
        {
            log_fatal("No suitable present queue found");
            exit(EXIT_FAILURE);
        }
    }
}

// TODO: maybe decouple from glfw and just pass desired width/height
wn_swapchain_t wn_swapchain_new(
    const wn_device_t *device,
    wn_surface_t *surface,
    VkRenderPass render_pass)
{
    wn_swapchain_t swapchain = { 0 };

    VkSurfaceCapabilitiesKHR surface_caps = surface->capabilities;

    swapchain.n_frames = surface_caps.minImageCount + 1;
    if (surface_caps.maxImageCount > 0 && swapchain.n_frames > surface_caps.maxImageCount)
    {
        swapchain.n_frames = surface_caps.maxImageCount;
    }

    /*
     *  create swapchain
     */

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface->surface,
        .minImageCount = swapchain.n_frames,
        .imageFormat = surface->format.format,
        .imageColorSpace = surface->format.colorSpace,
        .imageExtent = surface->extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, // FIXME: Only if graphics queue ==
        // present queue
        .preTransform = surface_caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = surface->present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = NULL,
    };

    WN_VK_CHECK(vkCreateSwapchainKHR(device->device, &swapchain_info, NULL, &swapchain.swapchain));

    /*
     *  swapchain images
     */
    WN_VK_CHECK(
        vkGetSwapchainImagesKHR(device->device, swapchain.swapchain, &swapchain.n_frames, NULL));

    VkImage *images = (VkImage *)malloc(swapchain.n_frames * sizeof(VkImage));
    assert(images);

    WN_VK_CHECK(
        vkGetSwapchainImagesKHR(device->device, swapchain.swapchain, &swapchain.n_frames, images));

    swapchain.frames = (wn_frame_t *)malloc(swapchain.n_frames * sizeof(wn_frame_t));
    assert(swapchain.frames);

    /*
     * descriptor set
     */
    VkDescriptorSetLayoutBinding mvp_binding = {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutCreateInfo desc_set_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &mvp_binding,
        .flags = 0,
        .pNext = NULL,
    };

    WN_VK_CHECK(vkCreateDescriptorSetLayout(
        device->device,
        &desc_set_layout_info,
        NULL,
        &swapchain.desc_set_layout));

    /*
     * descriptor pool
     */
    VkDescriptorPoolSize desc_pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = swapchain.n_frames,
    };

    VkDescriptorPoolCreateInfo desc_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &desc_pool_size,
        .maxSets = swapchain.n_frames,
        .flags = 0,
        .pNext = NULL,
    };

    WN_VK_CHECK(
        vkCreateDescriptorPool(device->device, &desc_pool_info, NULL, &swapchain.descriptor_pool));

    VkDescriptorSetLayout *layouts = malloc(sizeof(VkDescriptorSetLayout) * swapchain.n_frames);
    assert(layouts);

    for (uint32_t i = 0; i < swapchain.n_frames; i++)
    {
        layouts[i] = swapchain.desc_set_layout;
    }

    VkDescriptorSetAllocateInfo desc_set_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = swapchain.descriptor_pool,
        .descriptorSetCount = swapchain.n_frames,
        .pSetLayouts = layouts,
        .pNext = NULL,
    };

    VkDescriptorSet *desc_sets = malloc(sizeof(VkDescriptorSet) * swapchain.n_frames);
    assert(desc_sets);
    WN_VK_CHECK(vkAllocateDescriptorSets(device->device, &desc_set_alloc_info, desc_sets));

    for (uint32_t i = 0; i < swapchain.n_frames; i++)
    {
        swapchain.frames[i].image = images[i];

        VkImageViewCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface->format.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        WN_VK_CHECK(vkCreateImageView(device->device, &info, NULL, &swapchain.frames[i].image_view))

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = &swapchain.frames[i].image_view,
            .width = surface->extent.width,
            .height = surface->extent.height,
            .layers = 1,
        };

        WN_VK_CHECK(vkCreateFramebuffer(
            device->device,
            &framebuffer_info,
            NULL,
            &swapchain.frames[i].framebuffer));

        VkDeviceSize buffer_size = sizeof(wn_mvp_t);

        swapchain.frames[i].ubo = wn_buffer_new(
            device,
            &(VkBufferCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = buffer_size,
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .flags = 0,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices = NULL,
                .pNext = NULL,
            },
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        /*
         * descriptor set
         */
        swapchain.frames[i].ubo_desc_set = desc_sets[i];

        VkDescriptorBufferInfo desc_buf_info = {
            .buffer = swapchain.frames[i].ubo.handle,
            .offset = 0,
            .range = sizeof(wn_mvp_t),
        };

        VkWriteDescriptorSet desc_set_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = swapchain.frames[i].ubo_desc_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .pBufferInfo = &desc_buf_info,
            .pImageInfo = NULL,
            .pTexelBufferView = NULL,
            .pNext = NULL,
        };

        vkUpdateDescriptorSets(device->device, 1, &desc_set_write, 0, NULL);
    }

    free(layouts);
    free(desc_sets);

    return swapchain;
}

// TODO: destroy depth image/view
void wn_swapchain_destroy(VkDevice device, wn_swapchain_t *swapchain)
{
    for (uint32_t i = 0; i < swapchain->n_frames; i++)
    {
        vkDestroyImageView(device, swapchain->frames[i].image_view, NULL);
        vkDestroyFramebuffer(device, swapchain->frames[i].framebuffer, NULL);
        wn_buffer_destroy(&swapchain->frames[i].ubo, device);
    }
    free(swapchain->frames);
    vkDestroyDescriptorSetLayout(device, swapchain->desc_set_layout, NULL);
    vkDestroyDescriptorPool(device, swapchain->descriptor_pool, NULL);
    vkDestroySwapchainKHR(device, swapchain->swapchain, NULL);
}

wn_render_t wn_render_init(wn_window_t *window)
{
    wn_render_t render = { 0 };

#ifndef NDEBUG
    render.debug_enabled = true;
#else
    render.debug_enabled = false;
#endif

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = ENGINE_NAME,
        .pEngineName = ENGINE_NAME,
        .apiVersion = VK_API_VERSION,
    };

    uint32_t ext_count = 0;
    const char **glfw_exts = wn_window_get_required_exts(&ext_count);
    const char **exts = NULL;

    for (uint32_t i = 0; i < ext_count; i++)
    {
        stbds_arrput(exts, glfw_exts[i]);
    }

    const char **layers = NULL;
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = stbds_arrlen(exts),
        .ppEnabledExtensionNames = exts,
        .enabledLayerCount = stbds_arrlen(layers),
        .ppEnabledLayerNames = layers,
    };

    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = wn_util_debug_message_callback,
        .pUserData = NULL,
    };

    if (render.debug_enabled)
    {
        stbds_arrput(exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        stbds_arrput(layers, "VK_LAYER_KHRONOS_validation");

        instance_info.enabledExtensionCount = stbds_arrlen(exts);
        instance_info.ppEnabledExtensionNames = exts;
        instance_info.enabledLayerCount = stbds_arrlen(layers);
        instance_info.ppEnabledLayerNames = layers;
        instance_info.pNext = &debug_messenger_info;
    }

    /*
     *    instance
     */
    WN_VK_CHECK(vkCreateInstance(&instance_info, NULL, &render.instance));

    if (render.debug_enabled)
    {
        PFN_vkCreateDebugUtilsMessengerEXT debug_messenger_pfn
            = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                render.instance,
                "vkCreateDebugUtilsMessengerEXT");
        assert(debug_messenger_pfn);
        WN_VK_CHECK(debug_messenger_pfn(
            render.instance,
            &debug_messenger_info,
            NULL,
            &render.debug_messenger));
    }
    else
    {
        render.debug_messenger = NULL;
    }

    /*
     * device
     */
    uint32_t n_gpus = 0;
    WN_VK_CHECK(vkEnumeratePhysicalDevices(render.instance, &n_gpus, NULL));
    assert(n_gpus > 0);

    VkPhysicalDevice gpus[n_gpus];
    VkResult err = vkEnumeratePhysicalDevices(render.instance, &n_gpus, gpus);

    if (err)
    {
        log_error("Could not enumerate physical devices\n");
        exit(err);
    }

    // FIXME: Just picking first device rn
    VkPhysicalDevice gpu = gpus[0];

    render.device = wn_device_new(gpu);
    wn_device_t *device = &render.device;

    /*
     *    surface
     */
    VkSurfaceKHR window_surface = NULL;
    wn_window_create_surface(render.instance, window, &window_surface);

    render.surface = wn_surface_new(window_surface, render.device.gpu, window);

    // TODO: this could probably be moved into surface creation
    wn_surface_setup_present_queue(&render.surface, &render.device);

    wn_surface_t *surface = &render.surface;

    /*
     *  render pass
     */
    VkAttachmentDescription color_attachment = {
        .format = surface->format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass_desc = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };

    VkSubpassDependency subpass_dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass_desc,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency,
    };

    WN_VK_CHECK(vkCreateRenderPass(device->device, &render_pass_info, NULL, &render.render_pass));

    /*
     *    swapchain
     */
    render.swapchain = wn_swapchain_new(device, surface, render.render_pass);

    wn_swapchain_t *swapchain = &render.swapchain;

    /*
     *  pipeline
     */

    // shaders
    wn_shader_loader_t loader = wn_util_create_shader_loader();
    wn_shader_t vert = wn_util_load_shader(
        &loader,
        "../assets/shaders/triangle.vert",
        VK_SHADER_STAGE_VERTEX_BIT);
    wn_shader_t frag = wn_util_load_shader(
        &loader,
        "../assets/shaders/triangle.frag",
        VK_SHADER_STAGE_FRAGMENT_BIT);
    wn_util_destroy_shader_loader(&loader);

    VkShaderModuleCreateInfo vert_sm_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert.size,
        .pCode = vert.spirv,
    };
    VkShaderModuleCreateInfo frag_sm_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = frag.size,
        .pCode = frag.spirv,
    };

    VkShaderModule vert_sm;
    WN_VK_CHECK(vkCreateShaderModule(device->device, &vert_sm_info, NULL, &vert_sm));

    VkShaderModule frag_sm;
    WN_VK_CHECK(vkCreateShaderModule(device->device, &frag_sm_info, NULL, &frag_sm));

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = vert.shader_stage,
        .module = vert_sm,
        .pName = vert.entry,
    };
    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = frag.shader_stage,
        .module = frag_sm,
        .pName = frag.entry,
    };

    VkPipelineShaderStageCreateInfo shader_stage_infos[]
        = { frag_shader_stage_info, vert_shader_stage_info };

    // vertex input
    VkVertexInputBindingDescription binding_desc = wn_vertex_get_input_binding_desc();
    VkVertexInputAttributeDescription *attrib_desc = wn_vertex_get_attribute_desc();
    VkPipelineVertexInputStateCreateInfo vert_input_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attrib_desc,
    };

    // input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    // viewport state
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)surface->extent.width,
        .height = (float)surface->extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .extent = surface->extent,
        .offset = { .x = 0, .y = 0 },
    };

    VkPipelineViewportStateCreateInfo viewport_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    // rasterization state
    VkPipelineRasterizationStateCreateInfo rasterization_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };

    // multisample state
    VkPipelineMultisampleStateCreateInfo multisample_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // depth stencil state
    // TODO

    // color blending
    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_state,
    };

    // TODO: dynamic state
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates
        = (VkDynamicState[]) { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
    };

    // pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &render.swapchain.desc_set_layout,
        .flags = 0,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
        .pNext = NULL,
    };

    WN_VK_CHECK(vkCreatePipelineLayout(
        device->device,
        &pipeline_layout_info,
        NULL,
        &render.graphics_pipeline_layout));

    // graphics pipeline
    VkGraphicsPipelineCreateInfo graphics_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stage_infos,
        .pVertexInputState = &vert_input_state_info,
        .pInputAssemblyState = &input_assembly_state_info,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasterization_state_info,
        .pMultisampleState = &multisample_state_info,
        .pDepthStencilState = NULL,
        .pColorBlendState = &color_blend_state_info,
        .pDynamicState = &dynamic_state_info,
        .layout = render.graphics_pipeline_layout,
        .renderPass = render.render_pass,
        .subpass = 0,
        .basePipelineHandle = NULL,
        .basePipelineIndex = -1,
    };

    WN_VK_CHECK(vkCreateGraphicsPipelines(
        device->device,
        NULL,
        1,
        &graphics_pipeline_info,
        NULL,
        &render.graphics_pipeline));

    vkDestroyShaderModule(device->device, vert_sm, NULL);
    vkDestroyShaderModule(device->device, frag_sm, NULL);

    /*
     *  command pool
     */
    VkCommandPoolCreateInfo command_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = device->qfi.graphics,
        .flags = 0,
        .pNext = NULL,
    };

    WN_VK_CHECK(
        vkCreateCommandPool(device->device, &command_pool_info, NULL, &render.command_pool));

    /*
     * babby's first texture
     */
    render.color_texture = wn_texture_new(device, render.command_pool, "../assets/textures/uv_test_1k.png");

    /*
     *  vertex buffer
     */
    VkDeviceSize buffer_size = sizeof(vertices[0]) * N_VERTICES;

    wn_buffer_t staging_buffer = wn_buffer_new(
        device,
        &(VkBufferCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = buffer_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .flags = 0,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .pNext = NULL,
        },
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void *data = NULL;
    WN_VK_CHECK(vkMapMemory(device->device, staging_buffer.memory, 0, buffer_size, 0, &data));
    memcpy(data, vertices, (size_t)buffer_size);
    vkUnmapMemory(device->device, staging_buffer.memory);

    render.vertex_buffer = wn_buffer_new(
        device,
        &(VkBufferCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = buffer_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .flags = 0,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .pNext = NULL,
        },
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBuffer transfer_cmd_buf = wn_begin_command_buffer(device->device, render.command_pool);

    vkCmdCopyBuffer(
        transfer_cmd_buf,
        staging_buffer.handle,
        render.vertex_buffer.handle,
        1,
        &(VkBufferCopy) { .srcOffset = 0, .dstOffset = 0, .size = buffer_size });

    wn_end_command_buffer(
        device->device,
        render.command_pool,
        transfer_cmd_buf,
        device->transfer_queue);

    wn_buffer_destroy(&staging_buffer, device->device);

    /*
     *  index buffer
     */
    buffer_size = sizeof(indices[0]) * N_INDICES;

    staging_buffer = wn_buffer_new(
        device,
        &(VkBufferCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = buffer_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .flags = 0,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .pNext = NULL,
        },
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    data = NULL;
    WN_VK_CHECK(vkMapMemory(device->device, staging_buffer.memory, 0, buffer_size, 0, &data));
    memcpy(data, indices, (size_t)buffer_size);
    vkUnmapMemory(device->device, staging_buffer.memory);

    render.index_buffer = wn_buffer_new(
        device,
        &(VkBufferCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = buffer_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .flags = 0,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .pNext = NULL,
        },
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    transfer_cmd_buf = wn_begin_command_buffer(device->device, render.command_pool);

    vkCmdCopyBuffer(
        transfer_cmd_buf,
        staging_buffer.handle,
        render.index_buffer.handle,
        1,
        &(VkBufferCopy) { .srcOffset = 0, .dstOffset = 0, .size = buffer_size });

    wn_end_command_buffer(
        device->device,
        render.command_pool,
        transfer_cmd_buf,
        device->transfer_queue);

    wn_buffer_destroy(&staging_buffer, device->device);

    /*
     *  command buffers // TODO: Pull out for per wn_frame_t draw buffer??
     */
    render.command_buffers = malloc(sizeof(VkCommandBuffer) * swapchain->n_frames);
    assert(render.command_buffers);

    VkCommandBufferAllocateInfo command_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = render.command_pool,
        .commandBufferCount = swapchain->n_frames,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };
    WN_VK_CHECK(
        vkAllocateCommandBuffers(device->device, &command_buffer_info, render.command_buffers));

    for (uint32_t i = 0; i < swapchain->n_frames; i++)
    {
        VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };

        WN_VK_CHECK(vkBeginCommandBuffer(render.command_buffers[i], &command_buffer_begin_info));

        VkRenderPassBeginInfo render_pass_begin_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render.render_pass,
            .framebuffer = swapchain->frames[i].framebuffer,
            .renderArea = {
                .offset = { .x = 0, .y = 0 },
                .extent = surface->extent,
            },
            .clearValueCount = 1,
            .pClearValues = &(VkClearValue) { { { 0.0f, 0.0f, 0.0f, 1.0f } } },
        };

        vkCmdBeginRenderPass(
            render.command_buffers[i],
            &render_pass_begin_info,
            VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(
            render.command_buffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            render.graphics_pipeline);

        vkCmdSetViewport(render.command_buffers[i], 0, 1, &viewport);
        vkCmdSetScissor(render.command_buffers[i], 0, 1, &scissor);

        VkDeviceSize offsets = { 0 };
        vkCmdBindVertexBuffers(
            render.command_buffers[i],
            0,
            1,
            &render.vertex_buffer.handle,
            &offsets);
        vkCmdBindIndexBuffer(
            render.command_buffers[i],
            render.index_buffer.handle,
            0,
            VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(
            render.command_buffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            render.graphics_pipeline_layout,
            0,
            1,
            &render.swapchain.frames[i].ubo_desc_set,
            0,
            NULL);

        vkCmdDrawIndexed(render.command_buffers[i], N_INDICES, 1, 0, 0, 0);

        vkCmdEndRenderPass(render.command_buffers[i]);

        WN_VK_CHECK(vkEndCommandBuffer(render.command_buffers[i]));
    }

    /*
     *  semaphores and fences
     */
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    render.image_available = (VkSemaphore *)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    assert(render.image_available);

    render.render_finished = (VkSemaphore *)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    assert(render.render_finished);

    render.in_flight = (VkFence *)malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    assert(render.in_flight);

    render.image_in_flight = (VkFence *)malloc(sizeof(VkFence) * swapchain->n_frames);
    assert(render.image_in_flight);

    for (uint32_t i = 0; i < swapchain->n_frames; i++)
    {
        render.image_in_flight[i] = NULL;
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        WN_VK_CHECK(
            vkCreateSemaphore(device->device, &semaphore_info, NULL, &render.image_available[i]));
        WN_VK_CHECK(
            vkCreateSemaphore(device->device, &semaphore_info, NULL, &render.render_finished[i]));
        WN_VK_CHECK(vkCreateFence(device->device, &fence_info, NULL, &render.in_flight[i]));
    }

    render.current_frame = 0;

    return render;
}

void wn_swapchain_recreate(wn_render_t *render, wn_window_t *window)
{
    wn_device_t *device = &render->device;
    /*
     *    destroy existing swapchain and dependencies
     */
    vkDeviceWaitIdle(device->device);
    wn_swapchain_destroy(device->device, &render->swapchain);
    vkFreeCommandBuffers(
        device->device,
        render->command_pool,
        render->swapchain.n_frames,
        render->command_buffers);
    vkDestroyRenderPass(device->device, render->render_pass, NULL);

    free(render->surface.formats);
    free(render->surface.present_modes);

    /*
     *    start anew...
     */
    render->surface = wn_surface_new(render->surface.surface, device->gpu, window);

    VkAttachmentDescription color_attachment = {
        .format = render->surface.format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass_desc = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };

    VkSubpassDependency subpass_dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass_desc,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency,
    };

    WN_VK_CHECK(vkCreateRenderPass(device->device, &render_pass_info, NULL, &render->render_pass));

    render->swapchain = wn_swapchain_new(device, &render->surface, render->render_pass);

    VkCommandBufferAllocateInfo command_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = render->command_pool,
        .commandBufferCount = render->swapchain.n_frames,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };
    WN_VK_CHECK(
        vkAllocateCommandBuffers(device->device, &command_buffer_info, render->command_buffers));

    for (uint32_t i = 0; i < render->swapchain.n_frames; i++)
    {
        VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };

        WN_VK_CHECK(vkBeginCommandBuffer(render->command_buffers[i], &command_buffer_begin_info));

        VkRenderPassBeginInfo render_pass_begin_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render->render_pass,
            .framebuffer = render->swapchain.frames[i].framebuffer,
            .renderArea = {
                .offset = { .x = 0, .y = 0 },
                .extent = render->surface.extent,
            },
            .clearValueCount = 1,
            .pClearValues = &(VkClearValue) { { { 0.0f, 0.0f, 0.0f, 1.0f } } },
        };

        vkCmdBeginRenderPass(
            render->command_buffers[i],
            &render_pass_begin_info,
            VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(
            render->command_buffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            render->graphics_pipeline);

        // dynamic states
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)render->surface.extent.width,
            .height = (float)render->surface.extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        VkRect2D scissor = {
            .extent = render->surface.extent,
            .offset = { .x = 0, .y = 0 },
        };

        vkCmdSetViewport(render->command_buffers[i], 0, 1, &viewport);
        vkCmdSetScissor(render->command_buffers[i], 0, 1, &scissor);

        VkDeviceSize offsets = { 0 };
        vkCmdBindVertexBuffers(
            render->command_buffers[i],
            0,
            1,
            &render->vertex_buffer.handle,
            &offsets);
        vkCmdBindIndexBuffer(
            render->command_buffers[i],
            render->index_buffer.handle,
            0,
            VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(
            render->command_buffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            render->graphics_pipeline_layout,
            0,
            1,
            &render->swapchain.frames[i].ubo_desc_set,
            0,
            NULL);

        vkCmdDrawIndexed(render->command_buffers[i], N_INDICES, 1, 0, 0, 0);

        vkCmdEndRenderPass(render->command_buffers[i]);

        WN_VK_CHECK(vkEndCommandBuffer(render->command_buffers[i]));
    }
}

void wn_draw(wn_render_t *render, wn_window_t *window)
{
    wn_device_t *device = &render->device;

    vkWaitForFences(
        device->device,
        1,
        &render->in_flight[render->current_frame],
        VK_TRUE,
        UINT64_MAX);

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(
        device->device,
        render->swapchain.swapchain,
        UINT64_MAX,
        render->image_available[render->current_frame],
        NULL,
        &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        wn_swapchain_recreate(render, window);
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        log_fatal("Could not acquire swapchain image");
        exit(EXIT_FAILURE);
    }

    // ubo
    wn_v3f_t eye = { 2.0f, 2.0f, 2.0f };
    wn_v3f_t at = { 0.0f, 0.0f, 0.0f };
    wn_v3f_t up = { 0.0f, 0.0f, 1.0f };
    wn_mvp_t mvp = {
        .model = wn_mat4f_from_rotation_z(M_PI_2),
        .view = wn_mat4f_look_at(&eye, &at, &up),
        .proj = wn_mat4f_perspective(
            M_PI_4,
            (float)render->surface.extent.width / (float)render->surface.extent.height,
            0.1f,
            10.0f),
    };

    void *data = NULL;
    WN_VK_CHECK(vkMapMemory(
        device->device,
        render->swapchain.frames[image_index].ubo.memory,
        0,
        sizeof(mvp),
        0,
        &data));
    memcpy(data, &mvp, sizeof(mvp));
    vkUnmapMemory(device->device, render->swapchain.frames[image_index].ubo.memory);

    if (render->image_in_flight[image_index] != NULL)
    {
        vkWaitForFences(
            device->device,
            1,
            &render->image_in_flight[image_index],
            VK_TRUE,
            UINT64_MAX);
    }
    render->image_in_flight[image_index] = render->in_flight[render->current_frame];

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render->image_available[render->current_frame],
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &render->command_buffers[image_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &render->render_finished[render->current_frame],
    };

    vkResetFences(device->device, 1, &render->in_flight[render->current_frame]);

    WN_VK_CHECK(vkQueueSubmit(
        device->graphics_queue,
        1,
        &submit_info,
        render->in_flight[render->current_frame]));

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render->render_finished[render->current_frame],
        .swapchainCount = 1,
        .pSwapchains = &render->swapchain.swapchain,
        .pImageIndices = &image_index,
        .pResults = NULL,
    };

    result = vkQueuePresentKHR(device->present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        wn_swapchain_recreate(render, window);
    }
    else if (result != VK_SUCCESS)
    {
        log_fatal("Could not acquire swapchain image");
        exit(EXIT_FAILURE);
    }

    render->current_frame = (render->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// TODO
void wn_destroy(wn_render_t *render)
{
    wn_device_t *device = &render->device;
    if (render->debug_enabled)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT messenger_destroyer
            = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                render->instance,
                "vkDestroyDebugUtilsMessengerEXT");
        if (messenger_destroyer)
        {
            messenger_destroyer(render->instance, render->debug_messenger, NULL);
        }
        else
        {
            log_error("Could not find PFN_vkDestroyDebugUtilsMessengerEXT adress");
        }
    }
    wn_swapchain_destroy(device->device, &render->swapchain);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(device->device, render->image_available[i], NULL);
        vkDestroySemaphore(device->device, render->render_finished[i], NULL);
        vkDestroyFence(device->device, render->in_flight[i], NULL);
    }

    wn_image_destroy(&render->color_texture, device->device);

    wn_buffer_destroy(&render->vertex_buffer, device->device);
    wn_buffer_destroy(&render->index_buffer, device->device);

    // FIXME
    wn_surface_destroy(&render->surface);
    vkDestroySurfaceKHR(render->instance, render->surface.surface, NULL);

    vkDestroyPipeline(device->device, render->graphics_pipeline, NULL);
    vkDestroyPipelineLayout(device->device, render->graphics_pipeline_layout, NULL);
    vkDestroyRenderPass(device->device, render->render_pass, NULL);
    vkDestroyCommandPool(device->device, render->command_pool, NULL);
    vkDestroyDevice(device->device, NULL);
    vkDestroyInstance(render->instance, NULL);
}

int main(void)
{
#ifndef NDEBUG
    log_set_level(LOG_TRACE);
#else
    log_set_level(LOG_ERROR);
#endif

    wn_window_t window = wn_window_new(640, 480, "whynot");

    wn_render_t render = wn_render_init(&window);

    while (!wn_window_should_close(&window))
    {
        wn_window_poll_events();
        wn_draw(&render, &window);
    }

    vkDeviceWaitIdle(render.device.device);

    wn_destroy(&render);

    wn_window_destroy(&window);

    return 0;
}
