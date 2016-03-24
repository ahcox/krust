Krust
=====

<img style="width: 90%; max-width: 1006px; margin-left: auto; margin-right: auto;" src="http://cdn.ahcox.com/wp-ahcox.com/wp-content/uploads/2014/10/P1127242-1920x540-banner-top-503x283.jpg"/>

In a layered graphics stack, Krust seeks to solve problems one level up from
an explicit GPU API.
There are currently four submodules of which only three are yet useful:

* **krust-io**: A GLUT-like windowing and input library for Vulkan apps which
  can be used independently of the core Krust library by anyone wanting to get
  a window open and start writing raw Vulkan code.
* **krust-examples**: Example Vulkan apps using krust-io and later krust core.
* **krust-common**: Code that any other module could use and which they can use
  when talking to each other.
* **krust**: The helpers for working with Vulkan. Currently empty :'(.

Building
=======
See BUILD.md in this directory for instructions.
