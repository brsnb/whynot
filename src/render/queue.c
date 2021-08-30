#include "queue.h"

wn_result queue_submit(wn_queue_t *queue)
{
    vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence)
    return WN_OK;
}
