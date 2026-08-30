#pragma once

// --- Agents specification -------------------------------------------------
// Image views upload pass-declared creation usage through
// `parent_of<image>(this)` when bound. View subresource ranges stay with the
// view; access/layout transition tracking is intentionally deferred to
// command-recording work.
// --------------------------------------------------------------------------

// RESOURCE
VKTL_EXPORT_ namespace vktl::detail {

  template <> struct trait<image> {
    static constexpr VK_ VkObjectType object_type = VK_ VK_OBJECT_TYPE_IMAGE;
    static ::std::atomic<uint64_t> id;
    static auto acquire() noexcept {
      return id.fetch_add(1u, ::std::memory_order_relaxed);
    }

    using type = image;
    using view = image_view;
    using handle_type = VK_ VkImage;
    using create_info_type = VK_ VkImageCreateInfo;
    using create_flags_bits_type = VK_ VkImageCreateFlagBits;
    using usage_flags_type = VK_ VkImageUsageFlags;
    using view_type = VK_ VkImageView;
    using view_create_info_type = VK_ VkImageViewCreateInfo;

    static constexpr auto create = &VK_ vkCreateImage;
    static constexpr auto destroy = &VK_ vkDestroyImage;

    static constexpr auto bind_memory = &VK_ vkBindImageMemory;
#if defined(VK_KHR_bind_memory2)
    using bind_memory_info = VK_ VkBindImageMemoryInfoKHR;
    bind_memory_info memory_info(void *ptr, handle_type handle,
                                 VK_ VkDeviceMemory memory, size_t offset) {
      return {
          .sType = VK_ VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR,
          .pNext = ptr,
          .image = handle,
          .memory = memory,
          .memoryOffset = offset,
      };
    }
    static constexpr auto bind_memory_2 = &VK_ vkBindImageMemory2KHR;
#endif
  };
  // template<>
  // struct trait<allow_image_> : trait<image> {};
  template <> struct trait<VK_ VkImage> : trait<image> {};

  template <> struct trait<image_view> {
    static constexpr VK_ VkObjectType object_type =
        VK_ VK_OBJECT_TYPE_IMAGE_VIEW;

    using type = image_view;
    using host = image;
    using handle_type = VK_ VkImageView;
    using view_type = handle_type;
    using create_info_type = VK_ VkImageViewCreateInfo;
    using create_flags_bits_type = VK_ VkImageViewCreateFlagBits;
    using subresource_range = VK_ VkImageSubresourceRange;
    using layout = VK_ VkImageLayout;

    static constexpr auto create = &VK_ vkCreateImageView;
    static constexpr auto destroy = &VK_ vkDestroyImageView;
  };
  template <> struct trait<VK_ VkImageView> : trait<image_view> {};

  template <typename N>
  struct m<image, N> : basic_memory_resource<N, trait<image>> {
    using base = basic_memory_resource<N, trait<image>>;
    constexpr m(image image, auto &&...others) : base{forward_(others)...} {
      auto const is_1d = (image.height == ::std::uint32_t(-1));
      auto const is_2d = (!is_1d && image.depth == ::std::uint16_t(-1));
      auto const image_type = is_1d   ? VK_ VK_IMAGE_TYPE_1D
                              : is_2d ? VK_ VK_IMAGE_TYPE_2D
                                      : VK_ VK_IMAGE_TYPE_3D;
      auto const actual_height = is_1d ? 1u : image.height;
      auto const actual_depth = (is_1d || is_2d) ? 1u : uint32_t(image.depth);
      info.sType = VK_ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      info.format = VK_ VK_FORMAT_R8G8B8A8_UNORM;
      info.mipLevels = 1;
      info.arrayLayers = 1;
      info.samples = VK_ VK_SAMPLE_COUNT_1_BIT;
      info.tiling = VK_ VK_IMAGE_TILING_OPTIMAL;
      info.extent.height = actual_height;
      info.extent.depth = actual_depth;
      info.imageType = image_type;
    }

    ~m() { reset(); }

    constexpr void usage(uint64_t usages) noexcept {
      auto _ = locker_of(this);
      VKTL_ASSERT(base::is_null());
      info.usage = VK_ VkImageUsageFlags(usages);
    }
    constexpr VK_ VkImageUsageFlags usage() noexcept {
      auto _ = locker_of(this);
      return info.usage;
    }

    constexpr void append(VK_ VkFormat format) noexcept {
      VKTL_ASSERT(base::is_null());
      VKTL_ASSERT(format != VK_ VK_FORMAT_UNDEFINED);
      info.format = format;
    }

    // uint64_t id() const noexcept { return id_; }

  protected:
    void init() {
      if (base::is_null()) {
        N::init();
        this->generate(info, "[IMAGE] Create image failure.");
      }
    }

    void reset() noexcept {
      if (!base::is_null()) {
        N::reset();
        this->destroy();
      }
    }

  protected:
    VK_ VkImageCreateInfo info;

  private:
    access_list<default_image_access> access_;
  };

  template <typename N>
  struct m<image_view, N> : basic_frame_indexed_handle<N, trait<image_view>> {
    using base = basic_frame_indexed_handle<N, trait<image_view>>;

    constexpr m(image_view image_view, auto &&...others)
        : base{forward_(others)...},
          info{
            .sType = VK_ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .subresourceRange = {
              .layerCount = maximum,
              .levelCount = maximum,
            },
          } {}

    void upload_access() const noexcept {}

    auto subresource_range() const noexcept { return info.subresourceRange; }

    auto host(uint32_t frame) const noexcept {
      return parent_of<image>(this)->handle(frame);
    }
    auto host() const noexcept { return host(this->frame_index()); }
    auto layout() const noexcept { return VK_ VK_IMAGE_LAYOUT_GENERAL; }

    void upload_usage(uint64_t usages) {
      parent_of<image>(this)->usage(VK_ VkImageUsageFlags(usages));
    }

    void append(VK_ VkFormat format) noexcept {
      VKTL_ASSERT(base::is_null());
      VKTL_ASSERT(format == VK_ VK_FORMAT_UNDEFINED); // format can only set once.
      info.format = format;
      parent_of<image>(this)->append(format);
    }

  protected:
    void init() {
      if (base::is_null()) {
        N::init();
        info.image = handle_of<image>(this);
        base::generate(info, "[IMAGE_VIEW] Create image view failure.");
      }
    }
    void reset() {
      if (!base::is_null()) {
        base::reset();
        base::destroy();
      }
    }

  protected:
    VK_ VkImageViewCreateInfo info;
  };

  namespace format {
  constexpr uint32_t compress_image_bits(uint16_t bits) noexcept {
    switch (bits) {
    case uint16_t(invalid):
      return 0u;
    case 8u:
      return 1u;
    case 16u:
      return 2u;
    case 24u:
      return 3u;
    case 32u:
      return 4u;
    case 64u:
      return 5u;
    default:
      return 7u;
    }
  }

  constexpr uint32_t compress_image_type(uint16_t type) noexcept {
    using namespace image_bits_type;
    switch (type) {
    case undefined:
      return 0u;
    case unorm:
      return 1u;
    case uint:
      return 2u;
    case snorm:
      return 3u;
    case sint:
      return 4u;
    case sfloat:
      return 5u;
    default:
      return 7u;
    }
  }

  constexpr uint32_t
  pack_color_format(image_format::color const &value) noexcept {
    auto type = [](uint16_t bits, uint16_t kind) {
      return compress_image_type(bits == invalid ? image_bits_type::undefined
                                                 : kind);
    };
    return (uint32_t(value.order) << 24u) |
           (compress_image_bits(value.rbits) << 20u) |
           (compress_image_bits(value.gbits) << 17u) |
           (compress_image_bits(value.bbits) << 14u) |
           (compress_image_bits(value.abits) << 11u) |
           (type(value.rbits, value.rtype) << 8u) |
           (type(value.gbits, value.gtype) << 5u) |
           (type(value.bbits, value.btype) << 2u) |
           type(value.abits, value.atype);
  }

  constexpr uint32_t color_format_case(uint16_t order, uint16_t r, uint16_t g,
                                       uint16_t b, uint16_t a,
                                       uint16_t type) noexcept {
    return pack_color_format(image_format::color{
        .order = order,
        .rbits = r,
        .gbits = g,
        .bbits = b,
        .abits = a,
        .rtype = type,
        .gtype = type,
        .btype = type,
        .atype = type,
    });
  }

  constexpr VK_ VkFormat
  to_vkformat(image_format::color const &value) noexcept {
    using namespace image_bits_order;
    using namespace image_bits_type;
    constexpr uint16_t n = invalid;
    switch (pack_color_format(value)) {
    case color_format_case(rgba, 8, n, n, n, unorm):
      return VK_ VK_FORMAT_R8_UNORM;
    case color_format_case(rgba, 8, n, n, n, snorm):
      return VK_ VK_FORMAT_R8_SNORM;
    case color_format_case(rgba, 8, n, n, n, uint):
      return VK_ VK_FORMAT_R8_UINT;
    case color_format_case(rgba, 8, n, n, n, sint):
      return VK_ VK_FORMAT_R8_SINT;
    case color_format_case(rgba, 16, n, n, n, unorm):
      return VK_ VK_FORMAT_R16_UNORM;
    case color_format_case(rgba, 16, n, n, n, snorm):
      return VK_ VK_FORMAT_R16_SNORM;
    case color_format_case(rgba, 16, n, n, n, uint):
      return VK_ VK_FORMAT_R16_UINT;
    case color_format_case(rgba, 16, n, n, n, sint):
      return VK_ VK_FORMAT_R16_SINT;
    case color_format_case(rgba, 16, n, n, n, sfloat):
      return VK_ VK_FORMAT_R16_SFLOAT;
    case color_format_case(rgba, 32, n, n, n, uint):
      return VK_ VK_FORMAT_R32_UINT;
    case color_format_case(rgba, 32, n, n, n, sint):
      return VK_ VK_FORMAT_R32_SINT;
    case color_format_case(rgba, 32, n, n, n, sfloat):
      return VK_ VK_FORMAT_R32_SFLOAT;
    case color_format_case(rgba, 8, 8, n, n, unorm):
      return VK_ VK_FORMAT_R8G8_UNORM;
    case color_format_case(rgba, 8, 8, n, n, snorm):
      return VK_ VK_FORMAT_R8G8_SNORM;
    case color_format_case(rgba, 8, 8, n, n, uint):
      return VK_ VK_FORMAT_R8G8_UINT;
    case color_format_case(rgba, 8, 8, n, n, sint):
      return VK_ VK_FORMAT_R8G8_SINT;
    case color_format_case(rgba, 16, 16, n, n, unorm):
      return VK_ VK_FORMAT_R16G16_UNORM;
    case color_format_case(rgba, 16, 16, n, n, snorm):
      return VK_ VK_FORMAT_R16G16_SNORM;
    case color_format_case(rgba, 16, 16, n, n, uint):
      return VK_ VK_FORMAT_R16G16_UINT;
    case color_format_case(rgba, 16, 16, n, n, sint):
      return VK_ VK_FORMAT_R16G16_SINT;
    case color_format_case(rgba, 16, 16, n, n, sfloat):
      return VK_ VK_FORMAT_R16G16_SFLOAT;
    case color_format_case(rgba, 32, 32, n, n, uint):
      return VK_ VK_FORMAT_R32G32_UINT;
    case color_format_case(rgba, 32, 32, n, n, sint):
      return VK_ VK_FORMAT_R32G32_SINT;
    case color_format_case(rgba, 32, 32, n, n, sfloat):
      return VK_ VK_FORMAT_R32G32_SFLOAT;
    case color_format_case(rgba, 8, 8, 8, n, unorm):
      return VK_ VK_FORMAT_R8G8B8_UNORM;
    case color_format_case(rgba, 8, 8, 8, n, snorm):
      return VK_ VK_FORMAT_R8G8B8_SNORM;
    case color_format_case(rgba, 8, 8, 8, n, uint):
      return VK_ VK_FORMAT_R8G8B8_UINT;
    case color_format_case(rgba, 8, 8, 8, n, sint):
      return VK_ VK_FORMAT_R8G8B8_SINT;
    case color_format_case(rgba, 16, 16, 16, n, unorm):
      return VK_ VK_FORMAT_R16G16B16_UNORM;
    case color_format_case(rgba, 16, 16, 16, n, snorm):
      return VK_ VK_FORMAT_R16G16B16_SNORM;
    case color_format_case(rgba, 16, 16, 16, n, uint):
      return VK_ VK_FORMAT_R16G16B16_UINT;
    case color_format_case(rgba, 16, 16, 16, n, sint):
      return VK_ VK_FORMAT_R16G16B16_SINT;
    case color_format_case(rgba, 16, 16, 16, n, sfloat):
      return VK_ VK_FORMAT_R16G16B16_SFLOAT;
    case color_format_case(rgba, 8, 8, 8, 8, unorm):
      return VK_ VK_FORMAT_R8G8B8A8_UNORM;
    case color_format_case(rgba, 8, 8, 8, 8, snorm):
      return VK_ VK_FORMAT_R8G8B8A8_SNORM;
    case color_format_case(rgba, 8, 8, 8, 8, uint):
      return VK_ VK_FORMAT_R8G8B8A8_UINT;
    case color_format_case(rgba, 8, 8, 8, 8, sint):
      return VK_ VK_FORMAT_R8G8B8A8_SINT;
    case color_format_case(rgba, 16, 16, 16, 16, unorm):
      return VK_ VK_FORMAT_R16G16B16A16_UNORM;
    case color_format_case(rgba, 16, 16, 16, 16, snorm):
      return VK_ VK_FORMAT_R16G16B16A16_SNORM;
    case color_format_case(rgba, 16, 16, 16, 16, uint):
      return VK_ VK_FORMAT_R16G16B16A16_UINT;
    case color_format_case(rgba, 16, 16, 16, 16, sint):
      return VK_ VK_FORMAT_R16G16B16A16_SINT;
    case color_format_case(rgba, 16, 16, 16, 16, sfloat):
      return VK_ VK_FORMAT_R16G16B16A16_SFLOAT;
    case color_format_case(rgba, 32, 32, 32, 32, uint):
      return VK_ VK_FORMAT_R32G32B32A32_UINT;
    case color_format_case(rgba, 32, 32, 32, 32, sint):
      return VK_ VK_FORMAT_R32G32B32A32_SINT;
    case color_format_case(rgba, 32, 32, 32, 32, sfloat):
      return VK_ VK_FORMAT_R32G32B32A32_SFLOAT;
    case color_format_case(bgra, 8, 8, 8, 8, unorm):
      return VK_ VK_FORMAT_B8G8R8A8_UNORM;
    case color_format_case(bgra, 8, 8, 8, 8, snorm):
      return VK_ VK_FORMAT_B8G8R8A8_SNORM;
    case color_format_case(bgra, 8, 8, 8, 8, uint):
      return VK_ VK_FORMAT_B8G8R8A8_UINT;
    case color_format_case(bgra, 8, 8, 8, 8, sint):
      return VK_ VK_FORMAT_B8G8R8A8_SINT;
    default:
      return VK_ VK_FORMAT_UNDEFINED;
    }
  }

  constexpr VK_ VkFormat
  to_vkformat(image_format::depth const &value) noexcept {
    using namespace image_bits_type;
    if (value.dbits == 16u && value.dtype == unorm && value.sbits == invalid)
      return VK_ VK_FORMAT_D16_UNORM;
    if (value.dbits == 24u && value.dtype == unorm && value.sbits == invalid)
      return VK_ VK_FORMAT_X8_D24_UNORM_PACK32;
    if (value.dbits == 32u && value.dtype == sfloat && value.sbits == invalid)
      return VK_ VK_FORMAT_D32_SFLOAT;
    if (value.dbits == invalid && value.sbits == 8u && value.stype == uint)
      return VK_ VK_FORMAT_S8_UINT;
    if (value.dbits == 16u && value.dtype == unorm && value.sbits == 8u &&
        value.stype == uint)
      return VK_ VK_FORMAT_D16_UNORM_S8_UINT;
    if (value.dbits == 24u && value.dtype == unorm && value.sbits == 8u &&
        value.stype == uint)
      return VK_ VK_FORMAT_D24_UNORM_S8_UINT;
    if (value.dbits == 32u && value.dtype == sfloat && value.sbits == 8u &&
        value.stype == uint)
      return VK_ VK_FORMAT_D32_SFLOAT_S8_UINT;
    return VK_ VK_FORMAT_UNDEFINED;
  }
  } // namespace format

  template <> struct express<image_format::color> {
    static constexpr void invoke(image_format::color input, auto &base) {
      auto value = format::to_vkformat(input);
      VKTL_ASSERT(value != VK_ VK_FORMAT_UNDEFINED);
      base.append(value);
    }
  };

  template <> struct express<image_format::depth> {
    static constexpr void invoke(image_format::depth input, auto &base) {
      auto value = format::to_vkformat(input);
      VKTL_ASSERT(value != VK_ VK_FORMAT_UNDEFINED);
      base.append(value);
    }
  };
}
