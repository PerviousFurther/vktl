#pragma once

// RESOURCE
VKTL_EXPORT_ namespace vktl::detail {

	template<>
	struct trait<image> {
		static constexpr VK_ VkObjectType object_type = VK_ VK_OBJECT_TYPE_IMAGE;

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
	struct trait<image_view> {
		static constexpr VK_ VkObjectType object_type = VK_ VK_OBJECT_TYPE_IMAGE_VIEW;

		using type = image_view;
		using host = image;
		using handle_type = VK_ VkImageView;
		using create_info_type = VK_ VkImageViewCreateInfo;
		using create_flags_bits_type = VK_ VkImageViewCreateFlagBits;
		using subresource_range = VK_ VkImageSubresourceRange;
		using layout = VK_ VkImageLayout;

		static constexpr auto create = &VK_ vkCreateBufferView;
		static constexpr auto destroy = &VK_ vkDestroyBufferView;
	};
	template<>
	struct trait<VK_ VkImageView> : trait<image_view> {};

	template<typename N>
	struct m<image, N> : basic_resource<N, trait<image>> {
		constexpr m(image image, auto&&...others)
			: N{ forward_(others)... } {
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
			if (!N::is_null()) {
				this->generate(info);
			}
		}

		void reset() noexcept {
			auto _ = locker_of(this);
			if (N::is_null()) {
				VK_ vkDestroyImage(handle_of<device>(this), 
					handle_, N::allocator());
			}
		}

		auto handle() const noexcept {
			return handle_.value;
		}

	protected:
		VK_ VkImageCreateInfo info;

	private:
		reset_if_copy<VK_ VkImage> handle_;
		access_list<default_image_access> access_;
	};

	template<typename N>
	struct m<image_view, N> : basic_frame_related_handle<N, trait<image_view>> {
		using base = basic_frame_related_handle<N, trait<image_view>>;

		constexpr m(image_view image_view, auto&&...others)
			: N{ forward_(others)... }
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
			if (handle_) {
				base::generate();
				VK_ vkCreateImageView(handle_of<device>(this), 
					&info, N::allocator(), &handle_) | popup{ "[IMAGE_VIEW] Create image view failure." };
			}
		}

		void reset() {
			if (handle_) {
				VK_ vkDestroyImageView(handle_of<device>(this), handle_, N::allocator());
			}
		}

		auto handle() const noexcept {
			return handle_.value;
		}

		void upload_access() const noexcept {
			
		}

		auto subresource_range() const noexcept {
			return info.subresourceRange;
		}

	protected:
		VK_ VkImageViewCreateInfo info;
		
	private:
		reset_if_copy<VK_ VkImageView> handle_{ VK_NULL_HANDLE };
	};


}
