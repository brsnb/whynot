/*
===========================================================================

whynot: main.c

===========================================================================
*/

// clang-format off
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
// clang-format on
#include <assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <vulkan/vulkan_core.h>
#define STB_DS_IMPLEMENTATION
#define STBDS_NO_SHORT_NAMES
#include "log.h"
#include "stb_ds.h"
#include "util.h"

#define ENGINE_NAME "whynot"
#define VK_API_VERSION VK_API_VERSION_1_2

#define MAX_FRAMES_IN_FLIGHT 2

// try and wrap GLFW dependency...
typedef struct {
    GLFWwindow *window;
} wn_window_t;

wn_window_t wn_window_new(int width, int height, char *title) {
    if (!glfwInit()) {
        log_fatal("Could not initialize GLFW");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *os_window = glfwCreateWindow(width, height, title, NULL, NULL);

    if (!os_window) {
        log_fatal("Could not create OS window");
        exit(EXIT_FAILURE);
    }

    wn_window_t window = {
        .window = os_window,
    };

    return window;
}

void wn_window_destroy(wn_window_t *window) {
    glfwDestroyWindow(window->window);
    glfwTerminate();
}

void wn_window_get_framebuffer_size(wn_window_t *window, int *width,
                                    int *height) {
    glfwGetFramebufferSize(window->window, width, height);
}

// in the style of..., but exits on failure
void wn_window_create_surface(VkInstance instance, wn_window_t *window,
                              VkSurfaceKHR *surface) {
    WN_VK_CHECK(
        glfwCreateWindowSurface(instance, window->window, NULL, surface));
}

int wn_window_should_close(wn_window_t *window) {
    return glfwWindowShouldClose(window->window);
}

void wn_window_poll_events(void) { glfwPollEvents(); }

const char **wn_window_get_required_exts(uint32_t *nexts) {
    return glfwGetRequiredInstanceExtensions(nexts);
}

typedef struct {
    uint32_t compute;
    uint32_t graphics;
    uint32_t present;
    uint32_t transfer;
    VkQueueFlags supported;
} wn_qfi_t;

// TODO: Find other queues
wn_qfi_t wn_find_queue_families(VkPhysicalDevice physical_device,
                                VkSurfaceKHR surface) {
    wn_qfi_t qfi;
    uint32_t nqf = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &nqf, NULL);
    VkQueueFamilyProperties qfp[nqf];
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &nqf, qfp);

    for (uint32_t i = 0; i < nqf; ++i) {
        if (qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 supports_present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface,
                                                 &supports_present);
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

    if (!(qfi.supported & VK_QUEUE_GRAPHICS_BIT)) {
        log_fatal("No graphics/present queue found");
        exit(EXIT_FAILURE);
    }

    return qfi;
}

typedef struct {
    VkSurfaceKHR surface;

    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t nformats;
    VkSurfaceFormatKHR *formats;
    uint32_t npresent_modes;
    VkPresentModeKHR *present_modes;

    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
} wn_surface_t;

typedef struct {
    VkImage image;
    VkImageView image_view;
    // VkImage depth_image;
    // VkImageView depth_image_view;
    VkFramebuffer framebuffer;
    // VkCommandBuffer draw_buffer // TODO: maybe??
    // VkSemaphore image_available
} wn_frame_t;

typedef struct {
    VkSwapchainKHR swapchain;
    uint32_t nframes;
    wn_frame_t *frames;
} wn_swapchain_t;

typedef struct {
    VkInstance instance;

    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties physical_device_props;  // maybe don't need
    VkPhysicalDeviceFeatures physical_device_features; // maybe don't need

    VkDevice logical_device;
    wn_qfi_t qfi;
    VkQueue grapics_queue;
    VkQueue present_queue;

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

    // debug
    VkDebugUtilsMessengerEXT debug_messenger;
    bool debug_enabled;
} wn_render_t;

wn_surface_t wn_surface_new(VkSurfaceKHR window_surface,
                            VkPhysicalDevice physical_device,
                            wn_window_t *window) {
    wn_surface_t surface = {0};

    surface.surface = window_surface;

    // capabilities
    WN_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device, surface.surface, &surface.capabilities));

    // formats
    WN_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface.surface, &surface.nformats, NULL));
    assert(surface.nformats > 0);

    surface.formats = (VkSurfaceFormatKHR *)malloc(surface.nformats *
                                                   sizeof(VkSurfaceFormatKHR));
    assert(surface.formats);

    WN_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface.surface, &surface.nformats, surface.formats));

    // present modes
    WN_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface.surface, &surface.npresent_modes, NULL));
    assert(surface.npresent_modes > 0);

    surface.present_modes = (VkPresentModeKHR *)malloc(
        surface.npresent_modes * sizeof(VkPresentModeKHR));
    assert(surface.present_modes);

    WN_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface.surface, &surface.npresent_modes,
        surface.present_modes));

    /*
     *  choose desired format/present mode/extent
     */
    surface.format = surface.formats[0];
    for (uint32_t i = 0; i < surface.nformats; i++) {
        if (surface.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            surface.formats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
            surface.format = surface.formats[i];
        }
    }

    surface.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < surface.npresent_modes; i++) {
        if (surface.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            surface.present_mode = surface.present_modes[i];
        }
    }

    VkSurfaceCapabilitiesKHR surface_caps = surface.capabilities;

    if (surface_caps.currentExtent.width != UINT32_MAX) {
        surface.extent = surface_caps.currentExtent;
    } else {
        int width, height;
        wn_window_get_framebuffer_size(window, &width, &height);

        VkExtent2D surface_extent = {
            width = (uint32_t)width,
            height = (uint32_t)height,
        };

        surface.extent.width = WN_MAX(
            surface_caps.maxImageExtent.width,
            WN_MIN(surface_caps.maxImageExtent.width, surface_extent.width));
        surface.extent.height = WN_MAX(
            surface_caps.maxImageExtent.height,
            WN_MIN(surface_caps.maxImageExtent.height, surface_extent.height));
    }

    return surface;
}

void wn_surface_destroy(wn_surface_t *surface) {
    free(surface->formats);
    free(surface->present_modes);
}

// TODO: maybe decouple from glfw and just pass desired width/height
wn_swapchain_t wn_swapchain_new(VkDevice logical_device, wn_surface_t *surface,
                                VkRenderPass render_pass) {
    wn_swapchain_t swapchain = {0};

    VkSurfaceCapabilitiesKHR surface_caps = surface->capabilities;

    swapchain.nframes = surface_caps.minImageCount + 1;
    if (surface_caps.maxImageCount > 0 &&
        swapchain.nframes > surface_caps.maxImageCount) {
        swapchain.nframes = surface_caps.maxImageCount;
    }

    /*
     *  create swapchain
     */

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface->surface,
        .minImageCount = swapchain.nframes,
        .imageFormat = surface->format.format,
        .imageColorSpace = surface->format.colorSpace,
        .imageExtent = surface->extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode =
            VK_SHARING_MODE_EXCLUSIVE, // FIXME: Only if graphics queue ==
                                       // present queue
        .preTransform = surface_caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = surface->present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = NULL,
    };

    WN_VK_CHECK(vkCreateSwapchainKHR(logical_device, &swapchain_info, NULL,
                                     &swapchain.swapchain));

    /*
     *  swapchain images
     */
    WN_VK_CHECK(vkGetSwapchainImagesKHR(logical_device, swapchain.swapchain,
                                        &swapchain.nframes, NULL));

    VkImage *images = (VkImage *)malloc(swapchain.nframes * sizeof(VkImage));
    assert(images);

    WN_VK_CHECK(vkGetSwapchainImagesKHR(logical_device, swapchain.swapchain,
                                        &swapchain.nframes, images));

    swapchain.frames =
        (wn_frame_t *)malloc(swapchain.nframes * sizeof(wn_frame_t));
    assert(swapchain.frames);

    for (uint32_t i = 0; i < swapchain.nframes; i++) {
        swapchain.frames[i].image = images[i];

        VkImageViewCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface->format.format,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        WN_VK_CHECK(vkCreateImageView(logical_device, &info, NULL,
                                      &swapchain.frames[i].image_view))

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = &swapchain.frames[i].image_view,
            .width = surface->extent.width,
            .height = surface->extent.height,
            .layers = 1,
        };

        WN_VK_CHECK(vkCreateFramebuffer(logical_device, &framebuffer_info, NULL,
                                        &swapchain.frames[i].framebuffer));
    }

    return swapchain;
}

// TODO: destroy depth image/view
void wn_swapchain_destroy(VkDevice device, wn_swapchain_t *swapchain) {
    for (uint32_t i = 0; i < swapchain->nframes; i++) {
        vkDestroyImageView(device, swapchain->frames[i].image_view, NULL);
        vkDestroyFramebuffer(device, swapchain->frames[i].framebuffer, NULL);
    }
    free(swapchain->frames);
    vkDestroySwapchainKHR(device, swapchain->swapchain, NULL);
}

wn_render_t wn_render_init(wn_window_t *window) {
    wn_render_t render = {0};

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

    for (uint32_t i = 0; i < ext_count; i++) {
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
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = wn_util_debug_message_callback,
        .pUserData = NULL,
    };

    if (render.debug_enabled) {
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

    if (render.debug_enabled) {
        PFN_vkCreateDebugUtilsMessengerEXT debug_messenger_pfn =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                render.instance, "vkCreateDebugUtilsMessengerEXT");
        assert(debug_messenger_pfn);
        WN_VK_CHECK(debug_messenger_pfn(render.instance, &debug_messenger_info,
                                        NULL, &render.debug_messenger));

    } else {
        render.debug_messenger = NULL;
    }

    /*
     *    physical device
     */
    uint32_t device_count = 0;
    WN_VK_CHECK(
        vkEnumeratePhysicalDevices(render.instance, &device_count, NULL));
    assert(device_count > 0);

    VkPhysicalDevice physical_devices[device_count];
    VkResult err = vkEnumeratePhysicalDevices(render.instance, &device_count,
                                              physical_devices);

    if (err) {
        log_error("Could not enumerate physical devices\n");
        exit(err);
    }

    // FIXME: Just picking first device rn
    render.physical_device = physical_devices[0];
    vkGetPhysicalDeviceProperties(render.physical_device,
                                  &render.physical_device_props);
    vkGetPhysicalDeviceFeatures(render.physical_device,
                                &render.physical_device_features);

    /*
     *    surface
     */
    VkSurfaceKHR window_surface = NULL;
    wn_window_create_surface(render.instance, window, &window_surface);

    render.surface =
        wn_surface_new(window_surface, render.physical_device, window);

    wn_surface_t *surface = &render.surface;

    /*
     *    logical device
     */

    // queue infos
    render.qfi =
        wn_find_queue_families(render.physical_device, surface->surface);
    float queue_prios[] = {1.0f};

    // FIXME: fix when supporting seperate graphics/present queues
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .queueFamilyIndex = render.qfi.graphics,
        .pQueuePriorities = queue_prios,
    };

    VkPhysicalDeviceFeatures *enabled_features = NULL;
    const char **device_exts = NULL;
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

    WN_VK_CHECK(vkCreateDevice(render.physical_device, &device_info, NULL,
                               &render.logical_device));

    // device queues
    vkGetDeviceQueue(render.logical_device, render.qfi.graphics, 0,
                     &render.grapics_queue);
    vkGetDeviceQueue(render.logical_device, render.qfi.present, 0,
                     &render.present_queue);

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

    WN_VK_CHECK(vkCreateRenderPass(render.logical_device, &render_pass_info,
                                   NULL, &render.render_pass));

    /*
     *    swapchain
     */
    render.swapchain =
        wn_swapchain_new(render.logical_device, surface, render.render_pass);

    wn_swapchain_t *swapchain = &render.swapchain;

    /*
     *  pipeline
     */

    // shaders
    wn_shader_loader_t loader = wn_util_create_shader_loader();
    wn_shader_t vert = wn_util_load_shader(
        &loader, "../assets/shaders/triangle.vert", VK_SHADER_STAGE_VERTEX_BIT);
    wn_shader_t frag =
        wn_util_load_shader(&loader, "../assets/shaders/triangle.frag",
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
    WN_VK_CHECK(vkCreateShaderModule(render.logical_device, &vert_sm_info, NULL,
                                     &vert_sm));

    VkShaderModule frag_sm;
    WN_VK_CHECK(vkCreateShaderModule(render.logical_device, &frag_sm_info, NULL,
                                     &frag_sm));

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

    VkPipelineShaderStageCreateInfo shader_stage_infos[] = {
        frag_shader_stage_info, vert_shader_stage_info};

    // vertex input
    VkPipelineVertexInputStateCreateInfo vert_input_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL,
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
        .offset = {.x = 0, .y = 0},
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
        .cullMode = VK_CULL_MODE_BACK_BIT,
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
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_state,
    };

    // TODO: dynamic state
    /*
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        // FIXME: remove anan struct
        .pDynamicStates = (VkDynamicState[]){VK_DYNAMIC_STATE_VIEWPORT,
                                             VK_DYNAMIC_STATE_SCISSOR},
    };
    */

    // pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    WN_VK_CHECK(vkCreatePipelineLayout(render.logical_device,
                                       &pipeline_layout_info, NULL,
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
        .pDynamicState = NULL,
        .layout = render.graphics_pipeline_layout,
        .renderPass = render.render_pass,
        .subpass = 0,
        .basePipelineHandle = NULL,
        .basePipelineIndex = -1,
    };

    WN_VK_CHECK(vkCreateGraphicsPipelines(render.logical_device, NULL, 1,
                                          &graphics_pipeline_info, NULL,
                                          &render.graphics_pipeline));

    vkDestroyShaderModule(render.logical_device, vert_sm, NULL);
    vkDestroyShaderModule(render.logical_device, frag_sm, NULL);

    /*
     *  command pool
     */
    VkCommandPoolCreateInfo command_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = render.qfi.graphics,
        .flags = 0,
    };

    WN_VK_CHECK(vkCreateCommandPool(render.logical_device, &command_pool_info,
                                    NULL, &render.command_pool));

    /*
     *  command buffers // TODO: Pull out for per wn_frame_t draw buffer??
     */
    render.command_buffers =
        malloc(sizeof(VkCommandBuffer) * swapchain->nframes);
    assert(render.command_buffers);

    VkCommandBufferAllocateInfo command_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = render.command_pool,
        .commandBufferCount = swapchain->nframes,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };
    WN_VK_CHECK(vkAllocateCommandBuffers(
        render.logical_device, &command_buffer_info, render.command_buffers));

    for (uint32_t i = 0; i < swapchain->nframes; i++) {
        VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };

        WN_VK_CHECK(vkBeginCommandBuffer(render.command_buffers[i],
                                         &command_buffer_begin_info));

        VkRenderPassBeginInfo render_pass_begin_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render.render_pass,
            .framebuffer = swapchain->frames[i].framebuffer,
            .renderArea =
                {
                    .offset = {.x = 0, .y = 0},
                    .extent = surface->extent,
                },
            .clearValueCount = 1,
            .pClearValues = &(VkClearValue){{{0.0f, 0.0f, 0.0f, 1.0f}}},
        };

        vkCmdBeginRenderPass(render.command_buffers[i], &render_pass_begin_info,
                             VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(render.command_buffers[i],
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          render.graphics_pipeline);

        vkCmdDraw(render.command_buffers[i], 3, 1, 0, 0);

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

    render.image_available =
        (VkSemaphore *)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    assert(render.image_available);

    render.render_finished =
        (VkSemaphore *)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    assert(render.render_finished);

    render.in_flight =
        (VkFence *)malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    assert(render.in_flight);

    render.image_in_flight =
        (VkFence *)malloc(sizeof(VkFence) * swapchain->nframes);
    assert(render.image_in_flight);

    for (uint32_t i = 0; i < swapchain->nframes; i++) {
        render.image_in_flight[i] = NULL;
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        WN_VK_CHECK(vkCreateSemaphore(render.logical_device, &semaphore_info,
                                      NULL, &render.image_available[i]));
        WN_VK_CHECK(vkCreateSemaphore(render.logical_device, &semaphore_info,
                                      NULL, &render.render_finished[i]));
        WN_VK_CHECK(vkCreateFence(render.logical_device, &fence_info, NULL,
                                  &render.in_flight[i]));
    }

    render.current_frame = 0;

    return render;
}

void wn_swapchain_recreate(wn_render_t *render, wn_window_t *window) {
    /*
     *    destroy existing swapchain and dependencies
     */
    vkDeviceWaitIdle(render->logical_device);
    wn_swapchain_destroy(render->logical_device, &render->swapchain);
    vkFreeCommandBuffers(render->logical_device, render->command_pool,
                         render->swapchain.nframes, render->command_buffers);
    vkDestroyRenderPass(render->logical_device, render->render_pass, NULL);

    free(render->surface.formats);
    free(render->surface.present_modes);

    /*
     *    start anew...
     */
    render->surface = wn_surface_new(render->surface.surface,
                                     render->physical_device, window);

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

    WN_VK_CHECK(vkCreateRenderPass(render->logical_device, &render_pass_info,
                                   NULL, &render->render_pass));

    render->swapchain = wn_swapchain_new(render->logical_device,
                                         &render->surface, render->render_pass);

    VkCommandBufferAllocateInfo command_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = render->command_pool,
        .commandBufferCount = render->swapchain.nframes,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };
    WN_VK_CHECK(vkAllocateCommandBuffers(
        render->logical_device, &command_buffer_info, render->command_buffers));

    for (uint32_t i = 0; i < render->swapchain.nframes; i++) {
        VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };

        WN_VK_CHECK(vkBeginCommandBuffer(render->command_buffers[i],
                                         &command_buffer_begin_info));

        VkRenderPassBeginInfo render_pass_begin_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render->render_pass,
            .framebuffer = render->swapchain.frames[i].framebuffer,
            .renderArea =
                {
                    .offset = {.x = 0, .y = 0},
                    .extent = render->surface.extent,
                },
            .clearValueCount = 1,
            .pClearValues = &(VkClearValue){{{0.0f, 0.0f, 0.0f, 1.0f}}},
        };

        vkCmdBeginRenderPass(render->command_buffers[i],
                             &render_pass_begin_info,
                             VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(render->command_buffers[i],
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          render->graphics_pipeline);

        vkCmdDraw(render->command_buffers[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(render->command_buffers[i]);

        WN_VK_CHECK(vkEndCommandBuffer(render->command_buffers[i]));
    }
}

void wn_draw(wn_render_t *render, wn_window_t *window) {
    vkWaitForFences(render->logical_device, 1,
                    &render->in_flight[render->current_frame], VK_TRUE,
                    UINT64_MAX);

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(
        render->logical_device, render->swapchain.swapchain, UINT64_MAX,
        render->image_available[render->current_frame], NULL, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        wn_swapchain_recreate(render, window);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        log_fatal("Could not acquire swapchain image");
        exit(EXIT_FAILURE);
    }

    if (render->image_in_flight[image_index] != NULL) {
        vkWaitForFences(render->logical_device, 1,
                        &render->image_in_flight[image_index], VK_TRUE,
                        UINT64_MAX);
    }
    render->image_in_flight[image_index] =
        render->in_flight[render->current_frame];

    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

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

    vkResetFences(render->logical_device, 1,
                  &render->in_flight[render->current_frame]);

    WN_VK_CHECK(vkQueueSubmit(render->grapics_queue, 1, &submit_info,
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

    result = vkQueuePresentKHR(render->present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        wn_swapchain_recreate(render, window);
    } else if (result != VK_SUCCESS) {
        log_fatal("Could not acquire swapchain image");
        exit(EXIT_FAILURE);
    }

    render->current_frame = (render->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// TODO
void wn_destroy(wn_render_t *render) {
    if (render->debug_enabled) {
        PFN_vkDestroyDebugUtilsMessengerEXT messenger_destroyer =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                render->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (messenger_destroyer) {
            messenger_destroyer(render->instance, render->debug_messenger,
                                NULL);
        } else {
            log_error(
                "Could not find PFN_vkDestroyDebugUtilsMessengerEXT adress");
        }
    }
    wn_swapchain_destroy(render->logical_device, &render->swapchain);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(render->logical_device, render->image_available[i],
                           NULL);
        vkDestroySemaphore(render->logical_device, render->render_finished[i],
                           NULL);
        vkDestroyFence(render->logical_device, render->in_flight[i], NULL);
    }

    // FIXME
    wn_surface_destroy(&render->surface);
    vkDestroySurfaceKHR(render->instance, render->surface.surface, NULL);

    vkDestroyPipeline(render->logical_device, render->graphics_pipeline, NULL);
    vkDestroyPipelineLayout(render->logical_device,
                            render->graphics_pipeline_layout, NULL);
    vkDestroyRenderPass(render->logical_device, render->render_pass, NULL);
    vkDestroyCommandPool(render->logical_device, render->command_pool, NULL);
    vkDestroyDevice(render->logical_device, NULL);
    vkDestroyInstance(render->instance, NULL);
}

int main(void) {
#ifndef NDEBUG
    log_set_level(LOG_TRACE);
#else
    log_set_level(LOG_ERROR);
#endif

    wn_window_t window = wn_window_new(640, 480, "whynot");

    wn_render_t render = wn_render_init(&window);

    while (!wn_window_should_close(&window)) {
        wn_window_poll_events();
        wn_draw(&render, &window);
    }

    vkDeviceWaitIdle(render.logical_device);

    wn_destroy(&render);

    wn_window_destroy(&window);

    return 0;
}
