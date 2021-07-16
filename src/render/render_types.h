/*
===========================================================================

whynot::render_types.h: misc types used in the renderer

===========================================================================
*/
#pragma once

#include "core_types.h"

typedef enum wn_queue_type
{
    WN_QUEUE_TYPE_COMPUTE,
    WN_QUEUE_TYPE_GRAPHICS,
    WN_QUEUE_TYPE_TRANSFER,
    WN_QUEUE_TYPE_PRESENT,
} wn_queue_type;

typedef enum wn_shader_stage
{
    WN_SHADER_STAGE_VERTEX,
    WN_SHADER_STAGE_FRAGMENT,
    WN_SHADER_STAGE_COMPUTE,
} wn_shader_stage;
