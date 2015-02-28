Building Krust
==============

Krust comes with a CMake build system.
You must tell it where the Vulkan library you want to use is located.

On Linux
--------

For a debug makefile build do the following:

	cd build/debug
	ccmake ../../
	# press "c" to configure
	# Set VULKAN_INCLUDE_DIRECTORY and VULKAN_LIBRARY_DIRECTORY
	# to locations in which you have a Vulkan SDK.
	# press "g" to generate
	make.

On Windows
----------

cd build/vs15

...

Configuration Variables
=======================

Define these from the build system to affect how Krust is built.

KRUST_ASSERT_LEVEL
------------------
A number in the range 1 to 5. The higher the number, the more assertions
will be fired and the more expensive they will be.
Do debug builds at 5.
Deploy a true release at level 0 (no assertions).
Consider doing release builds at > 0 before deployment.
*  DEFAULT: 5
