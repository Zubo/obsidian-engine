cmake_minimum_required(VERSION 3.24)

add_library(ThirdPartyImpl
    "stbi_impl.cpp"
    "vma_impl.cpp"
)

target_link_libraries(ThirdPartyImpl
    PUBLIC
        VulkanMemoryAllocator
)
