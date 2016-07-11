Krust Source Code
=================

All C++ code constituting Krust is stored under this subdirectory.
It is organised as follows:

* **public-api:** The headers in this directory constitute the portable API
  for applications and higher-level libraries to call.
* **internal:** Code used by Krust internally but not intended to be
  interacted-with by users.
* **platform:** The root of per-platform public and internal trees to account
  for platform differences.
