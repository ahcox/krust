include_directories(${KRUST_PUBLIC_API_DIR})

set(KRUST_INTERNAL_HEADER_FILES
  ${KRUST_INTERNAL_DIR}/krust-internal.h
  ${KRUST_INTERNAL_DIR}/keep-alive-set.h
  ${KRUST_INTERNAL_DIR}/scoped-temp-array.h)

# Export the KRUST_INTERNAL_HEADER_FILES variable to the parent scope:
set(KRUST_INTERNAL_HEADER_FILES ${KRUST_INTERNAL_HEADER_FILES} PARENT_SCOPE)
