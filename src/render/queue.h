/*
===========================================================================

whynot::render::queue.h:

===========================================================================
*/
#pragma once

#include "core_types.h"
#include "vk.h"

typedef struct wn_queue_t
{
    VkQueue queue;
    uint32_t family_idx;
    uint32_t quueue_idx;
} wn_queue_t;

wn_result queue_submit(wn_queue_t* queue);
