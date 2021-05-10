# Copyright (c) Andrew Helge Cox 2016-2019.
# All rights reserved worldwide.
#
# Parse the vulkan XML specifiction using Python XML APIs to generate
# a file of C++ functions to initialize standard Vulkan API structs.
#
# Usage, from project root:
# 
#     wget https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/xml/vk.xml
#     python3 tools/scripts/gen_info_struct_wrappers.py > krust/public-api/vulkan_struct_init.h
#
# If the library fails to compile due to unknown vulkan symbols, try downloading
# the tagged xml file that matches the vulkan headers installed on your system
# that your are building against. E.g. on ubuntu, check the package version of
# vulkan-headers and use that for the tag. Tagged versions live on paths like:
# https://github.com/KhronosGroup/Vulkan-Docs/releases/tag/v1.2.176
# from where you can navigate to find the raw file blob.
#
#     wget https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/67f599afee77b0e598e7a325f13b9878edcacdfd/xml/vk.xml
# 
# The API used:
# https://docs.python.org/3/library/xml.etree.elementtree.html
#

import xml.etree.ElementTree
import re

path_to_spec='./vk.xml'

# Structures to not generate for:
IGNORED_STRUCTS = {
  'VkBaseInStructure': True,                  # < Because we never make one: it is an "abstract base class".
  'VkBaseOutStructure': True,                 # < Because we never make one: it is an "abstract base class".
  'VkPhysicalDeviceFeatures': True,           # < Because it is big and we will typically query the implementation, change a few fields, and send back the diff.
  'VkTransformMatrixKHR' : True               # < Because it holds a 2D matrix which our generated code mishandles [todo: fix this]
}

# Structures to not generate the longer function with parameters for:
IGNORED_STRUCTS_ALL_PARAMS = {
  'VkPhysicalDeviceProperties2': True,  # < Because it is big and we will typically query the implementation, change a few fields, and send back the diff
  'VkPhysicalDeviceDescriptorIndexingFeaturesEXT': True,
}

ARRAY_LEN = "array_len"

root = xml.etree.ElementTree.parse(path_to_spec).getroot()

# Print to standard error for printf debugging.
import sys
def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)

# Convert a structure name into the ALL_UPPER code for it:
# The RE is fragile and may require updating with new spec versions.
structNameSplitter = re.compile('(ID|8Bit|16Bit|Float16|Int8|Int64|Uint8|[1-9][1-9]*|[A-Z][a-z]+|[A-Z][^A-Z\d]+|[A-Z][A-Z]+)')
def StructToCode(name):
    name = name[2:len(name)] # Nibble off Vk prefix.
    pieces = structNameSplitter.findall(name)
    code = "VK_STRUCTURE_TYPE_"
   
    for piece in pieces:
        upper = piece.upper()
        code = code + upper + "_"
    code = code[0:-1]

    # Fixup any special cases that violate the general convention:
    if code == 'VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT':
      code = 'VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT'
    elif code == 'VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_WS_STATE_CREATE_INFO_NV':
      code = 'VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_W_SCALING_STATE_CREATE_INFO_NV'
    elif code == 'VK_STRUCTURE_TYPE_TEXTURE_LODG_FORMAT_PROPERTIES_AMD':
      code = 'VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD'
    elif code == 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCIB_INFO_PROPERTIES_EXT':
      code = 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT'
    elif code == 'VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTCD_MODE_EXT':
      code = 'VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT'
    elif code == 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTCD_FEATURES_EXT':
      code = 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT'
    elif code == 'VK_STRUCTURE_TYPE_GEOMETRY_AABBNV':
      code = 'VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV'
    elif code == 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTCHDRF_EXT':
      code = 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES_EXT'
    elif code == 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SMB_PROPERTIES_NV':
      code = 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV'
    elif code == 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SMB_FEATURES_NV':
      code = 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_FEATURES_NV'
    elif code == 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_11_FEATURES':
      code = 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES'
    elif code == 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_12_FEATURES':
      code = 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES'
    elif code == 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_11_PROPERTIES':
      code = 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES'
    elif code == 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_12_PROPERTIES':
      code = 'VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES'

    return code

# Capture the info we need out of the DOM:
arrayCapture  = re.compile(r"\s*\[(\d+)\]\s*");
nonWhitespace = re.compile(r"[^\s]")
constKeyword  = re.compile(r"\bconst\b")
structs = []           # < For reach struct we generate wrappers for, the data captured from the xml tree to let us generate the wrapper for it.
platform_to_macro = {} # < For each plaform, the associated macro for ifdefs
type_macros = {}       # < For each type the macro which ifdefs its use, else None.

# Find all the platforms so we can use the macros defined in here for ifdefing
# rather than hard-coding them:
platforms = root.find('platforms')
for platform in platforms.findall('platform'):
  name = platform.get('name')
  protect = platform.get('protect')
  platform_to_macro[name] = protect

# Find all the extensions which are platform-specific so we can wrap their
# struct creation functions in platform macro ifdefs:
extensions = root.find('extensions')
for extension in extensions.findall('extension'):
  platform = extension.get('platform')
  if(platform != None):
    for atype in extension.iter('type'):
      type_name = atype.get('name')
      type_macros[type_name] = platform_to_macro[platform]

# Grab the structs we want to wrap:
for atype in root.iter('type'):
    name = atype.get('name')
    if (atype.get('category') == 'struct') & (name != None):
      if (name not in IGNORED_STRUCTS) and (atype.get('alias') == None) and (atype.get('returnedonly') != 'true'):
        members = []
        member_names = {}
        for amember in atype.findall('member'):
            member = {}
            for aelem in list(amember):
                member[aelem.tag] = aelem.text # < this includes name and type nested elements
                if(aelem.tag == 'type'):
                  # Add const to front of type if it is floating in member text:
                  if(amember.text != None):
                    if(constKeyword.search(amember.text) != None):
                      member[aelem.tag] = "const " + member[aelem.tag]

                  if (aelem.tail != None):
                    if (nonWhitespace.search(aelem.tail) != None):
                      type_suffix = aelem.tail.strip()
                      member[aelem.tag] += type_suffix
                elif(aelem.tag == 'name'):
                    member_names[aelem.text] = True
                    # Look for and array size in the tail text of this name element inside the member element:
                    if(aelem.tail != None):
                      array_size_enum = amember.find('enum')
                      if(array_size_enum == None):
                        m = arrayCapture.search(aelem.tail);
                        if(m != None):
                          member[ARRAY_LEN] = int(m.group(1))
                      else:
                        member[ARRAY_LEN] = array_size_enum.text
                        #eprint(name, "member", aelem.text, "array size =", array_size_enum.text)
                    
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
      #else:
        #eprint("Ignoring", name, ", alias", atype.get('alias'))
  
# Free memory of the DOM ASAP:
root = None

# Big strings to include in generated code:

COPYRIGHT = '''// Copyright (c) 2016-2020 Andrew Helge Cox
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
    platform_macro = type_macros.get(name)
    if platform_macro != None:
      ifdefed = True
      print("#ifdef ", platform_macro)
    return ifdefed

def PrintParameterList(members):
    parameter_list = ""
    for member in members:
        member_name = member['name']
        member_type = member['type']
        
        if (member_name != "sType") & (member_name != "pNext"):
          member_array_len = None
          if ARRAY_LEN in member:
            member_array_len = member[ARRAY_LEN]
          parameter_list += "  " + member_type + " " + member_name
          if member_array_len != None:
            parameter_list += "[" + str(member_array_len) + "]"
          parameter_list += ",\n"
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
          member_array_len = None
          if ARRAY_LEN in member:
            member_array_len = member[ARRAY_LEN]
          if member_array_len != None:
            assignments += "  for(size_t i = 0; i < " + str(member_array_len) + "; ++i){\n"
            assignments += "    " + local + "." + member_name + "[i] = " + member_name + "[i];\n  }\n"
          else:
            assignments += "  " + local + "." + member_name + " = " + member_name + ";\n"
    print(assignments)
  
# Print file header:
print(FILE_TOP)

# ------------------------------------------------------------------------------
# Generate the simple wrappers for tagged sructs that save the user from sType
# and pNext init:
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

# ------------------------------------------------------------------------------
# Generate the fuller-featured init functions which set all members:
print(PARAMS_TOP)
for struct in structs:
    #print(struct)
    if struct['tagged'] != True:
      continue;
    name = struct['name']
    if name in IGNORED_STRUCTS_ALL_PARAMS:
      continue

    # Skip if there are no parameters to initialise:
    members = struct['members']
    if len(members) < 3:
      continue

    funcName = name[2:len(name)]
    # generate platform-specific ifdefs if required:
    ifdefed = PrintPlatformIfdef(name)
    print("inline " + name, funcName + "(")
    # Spew out the members as a parameter list:
    PrintParameterList(members)
    print(")")
    print("{")
    print("  " + name, "temp;")
    print("  temp.sType =", struct['STRUCT_TYPE'] + ";")
    print("  temp.pNext = nullptr;")
    # Generate member initialisations:
    PrintMemberAssignments("temp", members)
    print("  return temp;")
    print("}")
    if ifdefed:
        print('#endif')
    print("")
print(PARAMS_BOTTOM)

# ------------------------------------------------------------------------------
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
      # generate platform-specific ifdefs if required:
      ifdefed = PrintPlatformIfdef(name)
      print("inline " + name, funcName + "(")
      members = struct['members']
      PrintParameterList(members)
      print(")")
      print("{")
      print("  " + name, "temp;")
      PrintMemberAssignments("temp", members)
      print("  return temp;")
      print("}")
      if ifdefed:
        print('#endif')
      print("")

print(FILE_BOTTOM)
