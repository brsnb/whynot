/*
===========================================================================

whynot::render.h: main render interface

===========================================================================
*/
#pragma once

#include "core_vk.h"

typedef struct wn_render_t
{
    VkInstance instance;

    wn_device_t device;

    wn_surface_t surface;

    wn_swapchain_t swapchain;

    VkSemaphore* image_available;
    VkSemaphore* render_finished;
    VkFence* in_flight;
    VkFence* image_in_flight;
    size_t current_frame;

    VkRenderPass render_pass;

    VkPipelineLayout graphics_pipeline_layout;
    VkPipeline graphics_pipeline;

    VkCommandPool command_pool;
    VkCommandBuffer* command_buffers;

    wn_texture_t color_texture;

    wn_mesh_t mesh;

    wn_buffer_t vertex_buffer;
    wn_buffer_t index_buffer;

    // debug
    VkDebugUtilsMessengerEXT debug_messenger;
    bool debug_enabled;
} wn_render_t;

