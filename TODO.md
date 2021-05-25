# main
  * all "_new" functions should return errcode + create by ref
      * should also create more clear naming scheme w/r/t allocation (i.e. "_create" and "_init")
  * check all asserts + WN_VK_CHECKS for recoverable errors
  * not all in main.c probably

# etc
  * all queues are created from same idx, so i.e. on a device that offers more than one queue per queue family it may not be as effecient, it may also be more effecient (look into it)
