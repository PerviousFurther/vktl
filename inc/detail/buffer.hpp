#pragma once

// --- Agents specification -------------------------------------------------
// Buffer views upload pass-declared creation usage through
// `parent_of<buffer>(this)` when bound. The buffer exposes this focused
// operation publicly; access-state and barrier insertion remain separate
// future work.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::vptr {}

VKTL_EXPORT_ namespace vktl::detail {

  template <> struct trait<buffer> {
    static constexpr VK_ VkObjectType object_type = VK_ VK_OBJECT_TYPE_BUFFER;
    static ::std::atomic<uint64_t> id;
    static auto acquire() noexcept {
      return id.fetch_add(1u, ::std::memory_order_relaxed);
    }

    using type = buffer;
    using view = buffer_view;
    using handle_type = VK_ VkBuffer;
    using create_info_type = VK_ VkBufferCreateInfo;
    using create_flags_bits_type = VK_ VkBufferCreateFlagBits;
    using usage_flags_type = VK_ VkBufferUsageFlags;
    using view_type = VK_ VkBufferView;
    using view_create_info_type = VK_ VkBufferViewCreateInfo;

    static constexpr auto create = &VK_ vkCreateBuffer;
    static constexpr auto destroy = &VK_ vkDestroyBuffer;

    static constexpr auto bind_memory = &VK_ vkBindBufferMemory;

#if defined(VK_KHR_bind_memory2)
    using bind_memory_info = VK_ VkBindBufferMemoryInfoKHR;
    bind_memory_info memory_info(void *ptr, handle_type handle,
                                 VK_ VkDeviceMemory memory, size_t offset) {
      return {
          .sType = VK_ VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR,
          .pNext = ptr,
          .buffer = handle,
          .memory = memory,
          .memoryOffset = offset,
      };
    }
    static constexpr auto bind_memory_2 = &VK_ vkBindBufferMemory2KHR;
#endif
  };
  // template<>
  // struct trait<allow_buffer_> : trait<buffer> {};
  template <> struct trait<VK_ VkBuffer> : trait<buffer> {};

  template <> struct trait<buffer_view> {
    static constexpr VK_ VkObjectType object_type =
        VK_ VK_OBJECT_TYPE_BUFFER_VIEW;

    using type = buffer_view;
    using host = buffer;
    using handle_type = VK_ VkBufferView;
    using view_type = handle_type;
    using create_info_type = VK_ VkBufferViewCreateInfo;
    using create_flags_bits_type = VK_ VkBufferViewCreateFlags;
    using subresource_range = range<VK_ VkDeviceSize>;
    using layout = void;

    static constexpr auto create = &VK_ vkCreateBufferView;
    static constexpr auto destroy = &VK_ vkDestroyBufferView;
  };
  template <> struct trait<VK_ VkBufferView> : trait<buffer_view> {};

  template <typename N>
  struct m<buffer, N> : basic_memory_resource<N, trait<buffer>> {
    using base = basic_memory_resource<N, trait<buffer>>;

    constexpr m(buffer buffer, auto &&...others) : base{forward_(others)...} {
      info.size = buffer.size;
    }

    ~m() { reset(); }

    constexpr size_t size() const noexcept { return info.size; }

    constexpr void usage(uint64_t usages) noexcept {
      auto _ = locker_of(this);
      VKTL_ASSERT(base::is_null());
      info.usage = VK_ VkBufferUsageFlags(usages);
    }

    constexpr VK_ VkBufferUsageFlags usage() const noexcept {
      return info.usage;
    }

    // VK_ VkMemoryBarrier append_access(default_buffer_access const& access) {
    // 	VKTL_ASSERT(false); // not implmented yet.
    // }

    // uint64_t id() const noexcept { return id_; }

  protected:
    auto init() {
      auto locker = N::init();
      if (base::is_null()) {
        this->generate(info, "[BUFFER] Create buffer failure.");
      }
      return locker;
    }

    auto reset() {
      auto locker = N::reset();
      if (!base::is_null()) {
        this->destroy();
      }
      return locker;
    }

  protected:
    VK_ VkBufferCreateInfo info{
        .sType = VK_ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    };

  private:
    access_list<default_buffer_access> access_;
  };

  template <typename N>
  struct m<buffer_view, N>
      : basic_frame_indexed_handle<N, trait<buffer_view>> {
    using base = basic_frame_indexed_handle<N, trait<buffer_view>>;

    constexpr m(buffer_view buffer_view, auto &&...others)
        : base{forward_(others)...} {
      info.range = parent_of<buffer>(this)->size();
    }

    ~m() { reset(); }

    constexpr void usage(uint64_t usages) noexcept {
      parent_of<buffer>(this)->usage(VK_ VkBufferUsageFlags(usages));
    }

    void upload_access() {
      VKTL_ASSERT(false); // TODO;
    }

    uint32_t frame_count() const noexcept {
      return parent_of<buffer>(this)->frame_count();
    }

    auto host(uint32_t frame_index) const noexcept {
      return parent_of<buffer>(this)->handle(frame_index);
    }
    auto host() const noexcept { return host(this->frame_index()); }

    auto view(uint32_t frame_index) const noexcept {
      return base::handle_at(frame_index);
    }
    auto view() const noexcept { return view(this->frame_index()); }

    // auto layout() const noexcept { return nullptr; }
    auto subresource_range() const noexcept {
      return range<VK_ VkDeviceSize>{info.offset, info.range};
    }

    void append(VK_ VkFormat format) noexcept {
      VKTL_ASSERT(!handle_);
      VKTL_ASSERT(format != VK_ VK_FORMAT_UNDEFINED);
      info.format = format;
    }

  protected:
    void init() {
      N::init();
      VKTL_ASSERT(info.range); // not allow no size buffer view.
      if (this->is_null() && info.format != VK_ VK_FORMAT_UNDEFINED) {
        this->generate(info, "[BUFFER VIEW] Create buffer view failure.");
      }
    }

    void reset() {
      N::init();
      if (!this->is_null()) {
        this->destroy();
      }
    }

  private:
    using base::handle;

  protected:
    VK_ VkBufferViewCreateInfo info{
        .sType = VK_ VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
    };

  private:
    reset_if_copy<descriptor_handle> descriptor_{nullptr};
    reset_if_copy<VK_ VkBufferView> handle_{VK_NULL_HANDLE};
  };
}
