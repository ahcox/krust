set(KRUST_PUBLIC_API_HEADER_FILES
  ${KRUST_PUBLIC_API_DIR}/krust.h
  ${KRUST_PUBLIC_API_DIR}/compiler.h
  ${KRUST_PUBLIC_API_DIR}/conditional-value.h
  ${KRUST_PUBLIC_API_DIR}/intrusive-pointer.h
  ${KRUST_PUBLIC_API_DIR}/krust-assertions.h
  ${KRUST_PUBLIC_API_DIR}/krust-errors.h
  ${KRUST_PUBLIC_API_DIR}/logging.h
  ${KRUST_PUBLIC_API_DIR}/ref-object.h
  ${KRUST_PUBLIC_API_DIR}/scoped-free.h
  ${KRUST_PUBLIC_API_DIR}/vulkan-logging.h
  ${KRUST_PUBLIC_API_DIR}/vulkan-utils.h
  ${KRUST_PUBLIC_API_DIR}/vulkan_struct_init.h)
# Export the KRUST_PUBLIC_API_HEADER_FILES variable to the parent scope:
set(KRUST_PUBLIC_API_HEADER_FILES ${KRUST_PUBLIC_API_HEADER_FILES} PARENT_SCOPE)

# Temp diagnostics:
MESSAGE( STATUS "[public] KRUST_PUBLIC_API_DIR:" ${KRUST_PUBLIC_API_DIR} )
MESSAGE( STATUS "[public] KRUST_PUBLIC_API_HEADER_FILES: " ${KRUST_PUBLIC_API_HEADER_FILES})
