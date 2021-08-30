/*
===========================================================================

whynot::render::swapchain.h:

===========================================================================
*/
#pragma once

#include "render_types.h"
#include "vk.h"

typedef struct wn_render_frame_t
{
    VkImage image;
    VkImageView image_view;
} wn_render_frame_t;

typedef struct wn_render_swapchain_o
{
    VkSwapchainKHR swapchain;
} wn_render_swapchain_o;

wn_result wn_render_swapchain_acquire_image(const wn_render_swapchain_o* swapchain, VkImage* image);
