Krust IO
========

Krust IO gives you an application class to derive from.
It comes with a swapchain setup for you to pump your frames into
(see `krust-examples/clear`for simple example of how to do that).
Windows to draw into and input to drive applications from.

The code is distrubuted among the following directories:

 **public-api:** The headers in this directory constitute the portable API
  for applications and higher-level libraries to call.
* **internal:** Code used by Krust IO internally but not intended to be
  interacted-with by users.
* **platform:** The root of per-platform public and internal trees to account
  for platform differences.
