#pragma once

// --- Agents specification -------------------------------------------------
// Image views upload pass-declared creation usage through
// `parent_of<image>(this)` when bound. View subresource ranges stay with the
// view; access/layout transition tracking is intentionally deferred to
// command-recording work.
// --------------------------------------------------------------------------

// RESOURCE
VKTL_EXPORT_ namespace vktl::detail {

	template<>
	struct trait<image> {
		static constexpr VK_ VkObjectType object_type = VK_ VK_OBJECT_TYPE_IMAGE;

		using type = image;
		using view = image_view_;
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
		bind_memory_info memory_info(void* ptr, handle_type handle, VK_ VkDeviceMemory memory, size_t offset) {
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
	template<>
	struct trait<VK_ VkImage> : trait<image> {};

	template<>
	struct trait<image_view_> {
		static constexpr VK_ VkObjectType object_type = VK_ VK_OBJECT_TYPE_IMAGE_VIEW;

		using type = image_view_;
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
	template<>
	struct trait<VK_ VkImageView> : trait<image_view_> {};

	template<typename N>
	struct m<image, N> : basic_resource<N, trait<image>> {
		using base = basic_resource<N, trait<image>>;
		constexpr m(image image, auto&&...others)
			: base{ forward_(others)... } {
			auto const is_1d = (image.height == ::std::uint32_t(-1));
			auto const is_2d = (!is_1d && image.depth == ::std::uint16_t(-1));
			auto const image_type = is_1d
				? VK_ VK_IMAGE_TYPE_1D
				: is_2d
				? VK_ VK_IMAGE_TYPE_2D
				: VK_ VK_IMAGE_TYPE_3D;
			auto const actual_height = is_1d ? 1u : image.height;
			auto const actual_depth = (is_1d || is_2d) ? 1u : uint32_t(image.depth);
			info.sType = VK_ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.pNext = nullptr;
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

		void init() {
			N::init();
			auto _ = locker_of(this);
			if (base::is_null()) {
				this->generate(info, "[IMAGE] Create image failure.");
			}
		}

		void reset() noexcept {
			auto _ = locker_of(this);
			if (!base::is_null()) {
				this->destroy();
			}
		}

		void append_usage(VK_ VkImageUsageFlags usages) noexcept {
			auto _ = locker_of(this);
			assert(base::is_null());
			info.usage |= usages;
		}

		void append(VK_ VkFormat format) noexcept {
			assert(base::is_null());
			assert(format != VK_ VK_FORMAT_UNDEFINED);
			info.format = format;
		}

	protected:
		VK_ VkImageCreateInfo info;

	private:
		access_list<default_image_access> access_;
	};

	template<typename N>
	struct m<image_view_, N> : basic_frame_indexed_handle<N, trait<image_view_>> {
		using base = basic_frame_indexed_handle<N, trait<image_view_>>;

		constexpr m(image_view_ image_view_, auto&&...others)
			: base{ forward_(others)... }
			, info {
				.sType = VK_ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.subresourceRange = {
					.layerCount = maximum,
					.levelCount = maximum,
				},
			}
		{
		}

		~m() { reset(); }

		void init() {
			N::init();
			if (base::is_null()) {
				info.image = handle_of<image>(this);
				base::generate(info, "[IMAGE_VIEW] Create image view failure.");
			}
		}

		void reset() {
			if (!base::is_null()) base::destroy();
		}

		void upload_access() const noexcept {
			
		}

		auto subresource_range() const noexcept {
			return info.subresourceRange;
		}

		auto handle(uint32_t frame) const noexcept {
			auto value = parent_of<image>(this)->handle(frame);
			return value.value;
		}
		auto handle() const noexcept { return handle(this->frame_index()); }
		auto view(uint32_t frame) const noexcept {
			auto value = base::handle(frame);
			return value.value;
		}
		auto view() const noexcept { return view(this->frame_index()); }
		auto layout() const noexcept { return VK_ VK_IMAGE_LAYOUT_GENERAL; }

		void upload_usage(VK_ VkImageUsageFlags usages) {
			parent_of<image>(this)->append_usage(usages);
		}

		void append(VK_ VkFormat format) noexcept {
			assert(base::is_null());
			assert(format != VK_ VK_FORMAT_UNDEFINED);
			info.format = format;
		}

	protected:
		VK_ VkImageViewCreateInfo info;
	};


}
