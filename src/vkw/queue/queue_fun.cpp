#include "queue_fun.hpp"

#include "dbg/log.hpp"

namespace dry::vkw {

void copy_buffer(const vk_cmd_buffer& cmd, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;
    vkCmdCopyBuffer(cmd.handle(), src, dst, 1, &copy_region);
}

void copy_buffer_to_image(const vk_cmd_buffer& cmd, VkBuffer buffer, const vk_image& image) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { image.extent().width, image.extent().height, 1 };

    vkCmdCopyBufferToImage(cmd.handle(),
        buffer, image.handle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region
    );
}

void transition_image_layout(const vk_cmd_buffer& cmd, const vk_image& image, VkImageLayout layout_old, VkImageLayout layout_new) {
    VkPipelineStageFlags stage_src{}, stage_dst{};

    VkImageMemoryBarrier image_barrier{};
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier.oldLayout = layout_old;
    image_barrier.newLayout = layout_new;
    image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.image = image.handle();
    image_barrier.subresourceRange.baseMipLevel = 0;
    image_barrier.subresourceRange.levelCount = image.mip_levels();
    image_barrier.subresourceRange.baseArrayLayer = 0;
    image_barrier.subresourceRange.layerCount = 1;

    if (layout_new == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (image.format() == VK_FORMAT_D32_SFLOAT_S8_UINT || image.format() == VK_FORMAT_D24_UNORM_S8_UINT) { // if has stencil component
            image_barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else {
        image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    // TODO : generalize and un-if it?
    // 
    // from undefined to writable, used when: just created a sampled texture
    if (layout_old == VK_IMAGE_LAYOUT_UNDEFINED && layout_new == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        image_barrier.srcAccessMask = 0;
        image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        stage_dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
        // from dst to optimal, used when: written the texture data, make it usable in shader, text step in sampled image creation
    }
    else if (layout_old == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && layout_new == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        stage_src = VK_PIPELINE_STAGE_TRANSFER_BIT;
        stage_dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        // from undefined to stencil, used when: depth
    }
    else if (layout_old == VK_IMAGE_LAYOUT_UNDEFINED && layout_new == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        image_barrier.srcAccessMask = 0;
        image_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        stage_dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        // from undefined to optimal, used when: creating dummy images
    }
    else if (layout_old == VK_IMAGE_LAYOUT_UNDEFINED && layout_new == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        image_barrier.srcAccessMask = 0;
        image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        stage_src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        stage_dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        LOG_ERR("Desired image transfer not supported");
        dbg::panic();
    }

    vkCmdPipelineBarrier(cmd.handle(), stage_src, stage_dst, 0, 0, nullptr, 0, nullptr, 1, &image_barrier);
}

void generate_mip_maps(const vk_cmd_buffer& cmd, const vk_image& image) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image.handle();
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    i32_t mipWidth = image.extent().width, mipHeight = image.extent().height;
    for (auto i = 1u; i < image.mip_levels(); ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmd.handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier
        );

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        vkCmdBlitImage(cmd.handle(),
            image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR
        );

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd.handle(),
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier
        );

        if (mipWidth > 1) {
            mipWidth /= 2;
        }
        if (mipHeight > 1) {
            mipHeight /= 2;
        }
    }

    // this transitions to read only layout
    barrier.subresourceRange.baseMipLevel = image.mip_levels() - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd.handle(),
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier
    );
}

}