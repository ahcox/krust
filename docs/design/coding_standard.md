Krust Coding Standard
=====================

Introduction
------------
Krust is written in forward-looking C++, so all C++11, C++14, C++17 and even
C++20 level features are allowed as long as they are supported by GCC, Clang,
and the latest available Visual Studio free edition.
It is intended to be usable on constrained mobile and embedded devices however
so library and runtime features are chosen with care.
For example, the error handling strategy is deferred to the client so while
it is written to be weakly exception-safe using RAII, it does not necessarily
throw any exceptions itself other than standard library ones.
Similarly, with an eye to code size, templates should be used sparingly.

File Inclusion
--------------

Krust headers are always included inside Krust using quotation marks `""`,
never with angle brackets `<>`.

    #include "krust-internal-header-x.h" // Good!
    #include <krust-internal-header-x.h> // Bad!

Depending on your compiler and platform this may make no difference,
but for consistency's sake stick to quotations if working on Krust's code.
Users of Krust might want to use angle brackets if it has been installed on
their system in a central location.

External headers are always included using angle brackets.

    #include <string> // Good!
    #include "string" // Bad!

### Include Order

Include order for class implementation:

* class header
* module-internal headers
* module-external Krust headers
* system headers

Include order for class header:

* module-internal headers
* module-external Krust headers
* system headers

Include order for other files:

* module-internal headers
* module-external Krust headers
* system headers

Internal headers are ones in the same module as the file doing the including.
External headers include system headers and third-party libraries.
For each Krust module (krust-common, krust-io, krust), headers from other
modules are also considered external.
See [LLVM ordering](http://llvm.org/docs/CodingStandards.html#include-style)
for comparison.

### Include Dependencies

A file always includes every other it depends on rather than relying on
accidental transitive inclusions.
Forward declarations are preferred over inclusions.

Naming Conventions
------------------

The Vulkan API naming conventions are broadly adopted with some small
differences:

1.  Non-public class member variables use the common "m" prefix convention.
1.  Type prefixes on variable names such as "p" for pointers and "s" for structs
    are avoided.
1.  Enumerations in Krust use C++11 enum classes and so a simple naming can be
    used: `EnumerationType::Enumerant` rather than
    Vulkan's `VK_ENUMERATION_TYPE_ENUMERANT`.

Namespaces
----------

### Indenting of Namespaces
Very broad namespaces such as `Krust` and `Krust::Internal` are
not indented so as to avoid indenting the majority of lines in the codebase.
A tighter cluster of related functionality is indented however, to make it
pop out in a file. An example of this is an anonymous namespace holding
helper functions at the top of a files or a set of string manipulation
functions.

### Symbol Aliasing
Certain tools such as Doxygen fail to distinguish symbols that differ only by
namespace prefix. To combat that, care is taken to avoid repeating the same
undecorated symbol name in multiple namespaces even if that leads to clumsier
naming such as `Krust::sysx::Manager` and `Krust::sysy::Manager` becoming
`Krust::sysx::SysXManager` and `Krust::sysy::SysYManager`.

General Code Layout
-------------------
Spaces are used exclusively for intra-line whitespace.

Lines are 80 characters wide _from the leftmost non-whitespace character_.
This gives code a sort of position independence: as nesting depth changes
through refactoring, lines need not be reformatted to break at an absolute wrap
column. It also recognizes both that modern editors are capable of scrolling
left and right to keep the current line in view, and that while current displays
tend to be wide, that width is often used by developers to pack more columns of
windows and panels side by side, making a limited line length still valuable.

Comments
========

Doxygen comments used throughout.
Javadoc-style symbol used for Doxygen commands.

References
==========
Other style guides:

*  http://geosoft.no/development/cppstyle.html
*  https://google-styleguide.googlecode.com/svn/trunk/cppguide.html
