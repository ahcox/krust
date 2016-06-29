Krust
=====
<img style="width: 90%; max-width: 1006px; margin-left: auto; margin-right: auto;" src="http://cdn.ahcox.com/wp-ahcox.com/wp-content/uploads/2014/10/P1127242-1920x540-banner-top-503x283.jpg"/>
In a layered graphics stack, Krust seeks to solve problems one level up from
the explicit GPU API Vulkan.
There are currently three modules:

* **krust**: The helpers for working with Vulkan.
  Functions to initialise most of the structures used in the Vulkan API can be
  found in
  [`krust/public-api/vulkan_struct_init.h`](./krust/public-api/vulkan_struct_init.h).
  There is currently some reusable Vulkan helper
  code in [`krust/public-api/vulkan-utils.h`](./krust/public-api/vulkan-utils.h).
  Classes implementing RAII for Vulkan API objects are being built in
  [`krust/public-api/vulkan-objects.h`](./krust/public-api/vulkan-objects.h) and
  [`krust/public-api/vulkan-objects.cpp`](./krust/public-api/vulkan-objects.cpp).
* **krust-io**: A windowing and input library for Vulkan apps using
  core Krust.
  This allows very easily opening a window and starting writing Vulkan code on
  Windows or Linux.
  There is some reusable Vulkan code (subject to being rewritten and moved) in
  [`krust-io/internal/vulkan-helpers.h`](./krust-io/internal/vulkan-helpers.h)
  and Vulkan example code in
  [`krust-io/public-api/application.cpp`](./krust-io/public-api/application.cpp)
* **krust-examples**: Example Vulkan apps using krust-io and krust core.

Building
=======
See [BUILD.md](./BUILD.md) in this directory for instructions.

Using Krust
===========
Struct Initializers
-------------------
Vulkan has many structs to be passed into its API
which are tagged with a long enumerant and have an extension void pointer which
must always be set to null at present.
The utility functions in
[`krust/public-api/vulkan_struct_init.h`](./krust/public-api/vulkan_struct_init.h)
make sure the enumerant always has the correct value and the extension pointer
is null.
There are variants which also ensure that every struct member is initialised and
none are omitted.
In addition there are initialisation functions for small structs used in the
API.

### Examples

Usage without the initialization functions:

     VkImageCreateInfo info;
     info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
     info.pNext = nullptr;
     info.flags = 0;
     info.imageType = VK_IMAGE_TYPE_2D;
     // ...

Usage with the first set which only set the standard first two members:

     auto info = ImageCreateInfo();
     info.flags = 0;
     info.imageType = VK_IMAGE_TYPE_2D;
     // ...

In the second example those first two lines of member initialization are saved
and the enum is guaranteed correct.

Usage with the second set of initialisers which take parameters and guarantee
that all members are set:

     auto info = ImageCreateInfo(
       0,
       VK_IMAGE_TYPE_2D,
       // ...
     );

Initialising a simple struct without these helpers:

      VkOffset2D offset;
      offset.x = 64;
      offset.y = 128;

 Usage with these helpers:

    auto offset = Offset2D(64, 128)

See
[`krust-examples/clear/clear.cpp`](./krust-examples/clear/clear.cpp)
and
[`krust-examples/clear2/clear2.cpp`](./krust-examples/clear2/clear2.cpp)
for more usage examples.


Developing Krust
================
Coding Standard
---------------
See the design docs [here](./docs/design/coding_standard.md).

Adding a Header File to Krust
-----------------------------
To add a public header to the core Krust project, edit the CMakeLists.txt
for it which lives at: `krust/public-api/CMakeLists.txt`.

    set(KRUST_PUBLIC_API_HEADER_FILES
      ${KRUST_PUBLIC_API_DIR}/krust.h
      ${KRUST_PUBLIC_API_DIR}/vulkan_struct_init.h
      ${KRUST_PUBLIC_API_DIR}/new_file.h) //< Add your file here
