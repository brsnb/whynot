# main
  * not all in main.c probably
  * all "_new" functions should return errcode + create by ref
      * should also create more clear naming scheme w/r/t allocation (i.e. "_create" and "_init")
  * check all asserts + WN_VK_CHECKS for recoverable errors
  * command pool and command buffer management in general
    * ideal is one command pool per thread per frame being rendered
    * command pool + device are coupled in command submission as it needs the device and queue
      * command buffer manager would need at least a ref to device
  * all Vk objects are opaque pointers, so fix those being passed for const correctness
  * remove stdlib dependency

# etc
  * all queues are created from same idx, so i.e. on a device that offers more than one queue per queue family it may not be as effecient, it may also be more effecient (look into it)
  * swapchain image size vs surface extent?
