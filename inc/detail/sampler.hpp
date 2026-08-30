#pragma once

VKTL_EXPORT_ namespace vktl::detail {
  template <> struct trait<sampler> {
    using handle_type = sampler;
  };

  template <size_t index = 0u> struct sampler_bundles_ {
    static constexpr VK_ VkSamplerCreateInfo make_default_info() noexcept {
      return {
        .sType = VK_ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_ VK_FILTER_LINEAR,
        .minFilter = VK_ VK_FILTER_LINEAR,
        .mipmapMode = VK_ VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_ VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_ VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_ VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_ VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
        .borderColor = VK_ VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE};
    }

    constexpr VK_ VkSamplerCreateInfo operator[](size_t id) const noexcept
      requires(index == 0u)
    {

      VK_ VkSamplerCreateInfo info = make_default_info();

      switch (id) {
      case sampler_id::linear_clamp: {
        info.addressModeU = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        return info;
      }
      case sampler_id::linear_repeat: {
        return info;
      }
      case sampler_id::nearest_clamp: {
        info.magFilter = VK_ VK_FILTER_NEAREST;
        info.minFilter = VK_ VK_FILTER_NEAREST;
        info.mipmapMode = VK_ VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        return info;
      }
      case sampler_id::nearest_repeat: {
        info.magFilter = VK_ VK_FILTER_NEAREST;
        info.minFilter = VK_ VK_FILTER_NEAREST;
        info.mipmapMode = VK_ VK_SAMPLER_MIPMAP_MODE_NEAREST;
        return info;
      }
      case sampler_id::trilinear_repeat: {
        info.magFilter = VK_ VK_FILTER_LINEAR;
        info.minFilter = VK_ VK_FILTER_LINEAR;
        info.mipmapMode = VK_ VK_SAMPLER_MIPMAP_MODE_LINEAR;
        return info;
      }
      case sampler_id::anisotropic_16x_repeat: {
        info.anisotropyEnable = VK_TRUE;
        info.maxAnisotropy = 16.0f;
        return info;
      }
      case sampler_id::shadow_pcf: {
        info.magFilter = VK_ VK_FILTER_LINEAR;
        info.minFilter = VK_ VK_FILTER_LINEAR;
        info.addressModeU = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.addressModeV = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.addressModeW = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.borderColor = VK_ VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        info.compareEnable = VK_TRUE;
        info.compareOp = VK_ VK_COMPARE_OP_LESS_OR_EQUAL;
        return info;
      }
      case sampler_id::linear_border_black: {
        info.addressModeU = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.addressModeV = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.addressModeW = VK_ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.borderColor = VK_ VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        return info;
      }
      default:
        if constexpr (requires { sampler_bundles_<index + 1>()[id]; }) {
          return sampler_bundles_<index + 1>()[id];
        } else {
          VKTL_ASSERT(!"Unknown sampler id.");
          return info; // stop warning, since c++20 doesnt have unreachable().
        }
      }
    }
  };

  template <size_t index = 0u>
  constexpr sampler_bundles_<index> sampler_bundles{};

  template <typename N> struct m<sampler, N> : N {
    constexpr m(sampler s, auto &&...others)
        : N{forward_(others)...}, info{sampler_bundles<>[s.id]} {}

    VK_ VkSampler handle() const noexcept { return handle_.value; }

  protected:
    void init() {
      if (!handle_) {
        N::init();
        VK_ vkCreateSampler(handle_of<device>(this), &info, N::allocator(),
                            &handle_) |
            popup{"[SAMPLER] Create sampler failure."};
      }
    }

    void reset() {
      if (handle_) {
        VK_ vkDestroySampler(handle_of<device>(this), handle_, N::allocator());
        N::reset();
      }
    }

  protected:
    VK_ VkSamplerCreateInfo info;

  private:
    reset_if_copy<VK_ VkSampler> handle_;
  };
}