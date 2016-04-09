# Copyright (c) Andrew Helge Cox 2016.
# All rights reserved worldwide.
#
# Parse the vulkan XML specifiction using the standard Python APIs to generate
# a file of C++ functions to initialize standard Vulkan API structs.
#
# Usage, from project root:
# 
#     wget https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/1.0/src/spec/vk.xml
#     python3 tools/scripts/gen_info_struct_wrappers.py > krust/public-api/vulkan_struct_init.h
# 
# The API used:
# https://docs.python.org/2/library/xml.etree.elementtree.html#elementtree-objects
#

import xml.etree.ElementTree
import re

path_to_spec='./vk.xml'
IGNORED_STRUCTS = {
  'VkPhysicalDeviceProperties': True,
  'VkPhysicalDeviceFeatures': True,
  'VkPhysicalDeviceSparseProperties': True,
  'VkPhysicalDeviceLimits': True,
  'VkDisplayPropertiesKHR': True,
  'VkDisplayPlanePropertiesKHR': True,
  'VkDisplayModePropertiesKHR': True,
  'VkDisplayPlaneCapabilitiesKHR': True,
  'VkSurfaceCapabilitiesKHR': True,
  'VkSurfaceFormatKHR': True,
  'VkExtensionProperties': True, # Long char array
  'VkLayerProperties': True, # Long char arrays
  'VkPhysicalDeviceMemoryProperties': True # Long int arrays
}

root = xml.etree.ElementTree.parse(path_to_spec).getroot()


# Convert a structure name into the ALL_UPPER code for it:
def StructToCode(name):
    #pieces = re.findall('[A-Z][^A-Z]*', name)
    name = name[2:len(name)] # Nibble off Vk prefix.
    pieces = re.findall('([A-Z][A-Z]+|[A-Z][^A-Z]+)', name)
    code = "VK_STRUCTURE_TYPE_"
    for piece in pieces:
        upper = piece.upper()
        code = code + upper + "_"
    code = code[0:-1]
    # Fixup any special cases that violate the general convention:
    if code == 'VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT':
        code = 'VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT'
    return code

# Adjust types using clues in type and variable name since XML is not
# well-formed and has parts of some types outside the type tag in free
# text:
def FixType(inType, name):
  outType = inType
  if len(name) > 1:
    if (inType == 'char') & (name[0] == 'p') & (name[1] == 'p'):
      outType = 'const char * const *'
    elif (inType == 'char') & (name[0] == 'p'):
      outType = 'const char *'
    elif (name == 'pResults') | (name == 'pUserData'):
      outType = inType + ' *'
    elif (name[0] == 'p') & ('*' not in inType) & (name[1:2].isupper()):
      outType = 'const ' + inType + ' *'
  return outType

# Adjust names with array dimensions:
def AppendArrayToName(name):
  outName = name
  if name == 'blendConstants':
    outName = name + '[4]'
  return outName


# Capture the info we need out of the DOM:

structs = []

for atype in root.iter('type'):
    name = atype.get('name')
    if (atype.get('category') == 'struct') & (name != None):
      if name not in IGNORED_STRUCTS:
        #print(name)
        #if (name.find('CreateInfo') > -1): 
        #print(atype.tag, name, "attrib:", atype.attrib, "category:", atype.get('category')) # , "text:", atype.text, "tail:", atype.tail)
        members = []
        member_names = {}
        for amember in atype.findall('member'):
            member = {}
            #print("\t", amember.tag, amember.attrib)# , amember.text, amember.tail)
            for aelem in list(amember):
                #print("\t\t", aelem.tag, aelem.text)
                member[aelem.tag] = aelem.text
                if(aelem.tag == 'name'):
                    member_names[aelem.text] = True
            members.append(member)
        #print(members)
        struct = {}
        STRUCT_TYPE = StructToCode(name)
        struct['name'] = name
        struct['STRUCT_TYPE'] = STRUCT_TYPE
        struct['members'] = members
        if ('sType' in member_names) & ('pNext' in member_names):
          struct['tagged'] = True
        else:
          struct['tagged'] = False
        structs.append(struct)
  
# Free memory ASAP:
root = None

# Big strings to include in generated code:

COPYRIGHT = '''// Copyright (c) 2016 Andrew Helge Cox
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

'''

GENERATED_WARNING='''/// @note Do not edit this file. It is generated from the Vulkan XML
/// specification by the script at `tools/scripts/gen_info_struct_wrappers.py`.

'''

FILE_COMMENT='''/**
 * @file Three sets of functions used to initialize Vulkan API structs.
 * 1. Functions to initialize the type ID enum and pNext extension pointer
 *    of Vulkan API structures which require those as their first two fields.
 * 2. Functions to initialize all members of Vulkan API structures, automatically
 *    supplying the type ID and pNext extension pointer while requiring all other
 *    fields to be supplied by the user.
 * 3. Functions initialize all members of small Vulkan structures from parameters
 *    supplied by the user.
 *
 * @see VulkanTaggedStructSimpleInit, VulkanTaggedStructParamsInit,
 *      VulkanUntaggedStructParamsInit
 */

'''

INCLUDES='''// External includes:
#include <vulkan/vulkan.h>

'''

FILE_TOP = '''#ifndef KRUST_STRUCT_INIT_H_INCLUDED_E26EF
#define KRUST_STRUCT_INIT_H_INCLUDED_E26EF

''' + COPYRIGHT + GENERATED_WARNING + FILE_COMMENT + INCLUDES + '''namespace Krust
{
'''

SIMPLE_TOP='''/**
 * @name VulkanTaggedStructSimpleInit For each Vulkan API struct tagged with a
 * type enum and possessing an extension pointer, a function to initialize the
 * first two fields of that struct.
 *
 * The use of these functions saves some code and makes sure the type
 * and the extension field of each struct are set correctly and reliably.
 * 
 * Usage without these helpers:
 *
 *     VkImageCreateInfo info;
 *     info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
 *     info.pNext = nullptr;
 *     info.flags = 0;
 *     info.imageType = VK_IMAGE_TYPE_2D;
 *     // ...
 *
 * Usage with these helpers:
 *
 *     auto info = kr::ImageCreateInfo();
 *     info.flags = 0;
 *     info.imageType = VK_IMAGE_TYPE_2D;
 *     // ...
 *
 * In the second example those first two lines of member initialization are saved.
 *
 * See `krust-examples/clear/clear.cpp` for more usage examples. 
 */
 ///@{
'''

PARAMS_TOP='''
/**
 * @name VulkanTaggedStructParamsInit For each Vulkan API struct tagged with a
 * type enum and possessing an extension pointer, a function to initialize the
 * members of the struct without having to set the first two fields.
 *
 * The use of these functions saves some code and makes sure the type
 * and the extension field of each struct are set correctly and reliably.
 * It also ensures no member is forgotten by the user.
 * 
 * Usage without these helpers:
 *
 *     VkImageCreateInfo info;
 *     info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
 *     info.pNext = nullptr;
 *     info.flags = 0;
 *     info.imageType = VK_IMAGE_TYPE_2D;
 *     // ...
 *
 * Usage with these helpers:
 *
 *     auto info = kr::ImageCreateInfo(
 *       0,
 *       VK_IMAGE_TYPE_2D;
 *       // ...
 *     );
 *
 * See `krust-examples/clear2/clear2.cpp` for more usage examples. 
 */
 ///@{
'''

UNTAGGED_TOP='''
/**
 * @name VulkanUntaggedStructParamsInit For each small Vulkan API struct,
 * a function to initialize the members of the struct.
 *
 * The use of these functions ensures no member is forgotten by the user.
 * 
 * Usage without these helpers:
 *
 *     VkOffset2D offset;
 *     offset.x = 64;
 *     offset.y = 128;
 *     
 * Usage with these helpers:
 *
 *     auto offset = kr::Offset2D(64, 128);
 *
 */
 ///@{
'''

SIMPLE_BOTTOM = '''///@}'''

PARAMS_BOTTOM = '''///@}'''

FILE_BOTTOM = '''///@}

} // namespace Krust

#endif // #ifndef KRUST_STRUCT_INIT_H_INCLUDED_E26EF'''

# Print wrapper #ifdef for platform-specific functions:
# Don't forget to print the #endif after the function too.
def PrintPlatformIfdef(name):
    ifdefed = False
    if name == 'VkAndroidSurfaceCreateInfoKHR':
        ifdefed = True
        print("#ifdef VK_USE_PLATFORM_ANDROID_KHR")
    if name == 'VkMirSurfaceCreateInfoKHR':
        ifdefed = True
        print("#ifdef VK_USE_PLATFORM_MIR_KHR")
    if name == 'VkWaylandSurfaceCreateInfoKHR':
        ifdefed = True
        print("#ifdef VK_USE_PLATFORM_WAYLAND_KHR")
    if name == 'VkWin32SurfaceCreateInfoKHR':
        ifdefed = True
        print("#ifdef VK_USE_PLATFORM_WIN32_KHR")
    if name == 'VkXlibSurfaceCreateInfoKHR':
        ifdefed = True
        print("#ifdef VK_USE_PLATFORM_XLIB_KHR")
    if name == 'VkXcbSurfaceCreateInfoKHR':
        ifdefed = True
        print("#ifdef VK_USE_PLATFORM_XCB_KHR")
    return ifdefed

def PrintParameterList(members):
    parameter_list = ""
    for member in members:
        member_name = member['name']
        member_type = FixType(member['type'], member_name);
        if (member_name != "sType") & (member_name != "pNext"):
          parameter_list += "  " + member_type + " " + AppendArrayToName(member_name) + ",\n"
    # Trim training "," and newline:
    if len(parameter_list) > 0:
        parameter_list = parameter_list[0:len(parameter_list) - 2]
    print(parameter_list)

def PrintMemberAssignments(local, members):
    assignments = ""
    for member in members:
        member_name = member['name']
        member_type = member['type'];
        if (member_name != "sType") & (member_name != "pNext"):
          array_name = AppendArrayToName(member_name)
          if array_name != member_name:
            array_len = int(array_name[len(array_name) - 2: len(array_name) - 1])
            for i in range(0, array_len):
              assignments += "  " + local + "." + member_name + "[" + str(i) + "] = " + member_name + "[" + str(i) + "]" + ";\n"
          else:
            assignments += "  " + local + "." + member_name + " = " + member_name + ";\n"
    print(assignments)
  
# Print file header:
print(FILE_TOP)

# Generate the simple wrappers for tagged sructs that save the user from sType
# and pNext init:
#print(structs)
print(SIMPLE_TOP)
if 1 == 1:
  for struct in structs:
      #print(struct)
      if struct['tagged'] != True:
        continue;
      name = struct['name']
      funcName = name[2:len(name)]
      # generate platform-specific ifdefs if required:
      ifdefed = PrintPlatformIfdef(name)
      print("inline " + name, funcName + "()")
      print("{")
      print("  " + name, "info;")
      print("  info.sType =", struct['STRUCT_TYPE'] + ";")
      print("  info.pNext = nullptr;")
      print("  return info;")
      print("}")
      if ifdefed:
          print('#endif')
      print("")
print(SIMPLE_BOTTOM)

# Generate the fuller-featured init functions which set all members:
print(PARAMS_TOP)
for struct in structs:
    #print(struct)
    if struct['tagged'] != True:
      continue;
    name = struct['name']
    funcName = name[2:len(name)]
    # generate platform-specific ifdefs if required:
    ifdefed = PrintPlatformIfdef(name)
    print("inline " + name, funcName + "(")
    # Spew out the members as a parameter list:
    members = struct['members']
    PrintParameterList(members)
    print(")")
    print("{")
    print("  " + name, "info;")
    print("  info.sType =", struct['STRUCT_TYPE'] + ";")
    print("  info.pNext = nullptr;")
    # Generate member initialisations:
    PrintMemberAssignments("info", members)
    print("  return info;")
    print("}")
    if ifdefed:
        print('#endif')
    print("")
print(PARAMS_BOTTOM)

# Generate the creation wrappers for structs that are not tagged:
print(UNTAGGED_TOP)
if 1 == 1:
  for struct in structs:
      if struct['tagged'] == True:
        continue
      name = struct['name']
      if name == "VkRect3D":
        continue
      funcName = name[2:len(name)]
      print("inline " + name, funcName + "(")
      members = struct['members']
      #print(struct)
      #print(members)
      PrintParameterList(members)
      print(")")
      print("{")
      print("  " + name, "temp;")
      PrintMemberAssignments("temp", members)
      print("  return temp;")
      print("}")
      print("")

print(FILE_BOTTOM)

