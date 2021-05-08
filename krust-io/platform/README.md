Directory: platform
===================

The code outside this directory is portable, with varying levels of dependency
on the C++ standard libraries only.
Inside are rooted a set of source trees specific to:

  * architectures,
  * operating systems, and
  * compilers.

The build system is expected to pull in the right code for the current
combination of those three which define the library's notion of a "platform".
