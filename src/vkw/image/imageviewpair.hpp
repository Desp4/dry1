#pragma once

#ifndef DRY_VK_IMAGEVIEW_PAIR_H
#define DRY_VK_IMAGEVIEW_PAIR_H

#include "image.hpp"
#include "imageview.hpp"

namespace dry::vkw {
// TODO : duplicat device ptr
class vk_image_view_pair {
public:
    vk_image_view_pair(const vk_device& device,
        VkExtent2D dimensions, u32_t mip_lvls, VkSampleCountFlagBits samples,
        VkFormat img_format, VkImageTiling tiling, VkImageUsageFlags usage,
        VmaMemoryUsage memory_usage, VkImageAspectFlags aspect_flags) noexcept :
        _image{ device, dimensions, mip_lvls, samples, img_format, tiling, usage, memory_usage },
        _view{ device, _image.handle(), img_format, mip_lvls, aspect_flags }
    {}

    vk_image_view_pair() noexcept = default;
    vk_image_view_pair(vk_image_view_pair&&) noexcept = default;

    const vk_image& image() const { return _image; }
    const vk_image_view& view() const { return _view; }

    vk_image_view_pair& operator=(vk_image_view_pair&&) noexcept = default;

private:
    vk_image _image;
    vk_image_view _view;
};

}

#endif
