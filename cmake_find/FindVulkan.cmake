if(NOT VULKAN_FOUND)
    set(VULKAN_FOUND OFF)
    # TODO : win only and SDK only, fix that, note Lib for x86_64, should be Lib32 for x86
    if (DEFINED ENV{VULKAN_SDK})
        find_library(VULKAN_LIBRARY NAMES vulkan-1 HINTS "$ENV{VULKAN_SDK}/Lib")
        if (NOT ${VULKAN_LIBRARY} STREQUAL "VULKAN_LIBRARY-NOTFOUND")
            set(VULKAN_INCLUDE "$ENV{VULKAN_SDK}/Include")

            add_library(vulkan UNKNOWN IMPORTED)
            set_target_properties(vulkan PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${VULKAN_INCLUDE})
            set_target_properties(vulkan PROPERTIES IMPORTED_LOCATION ${VULKAN_LIBRARY})
            set(VULKAN_FOUND ON)
        endif()
    endif()
endif()