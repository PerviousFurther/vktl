#pragma once

// RESOURCE
VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct m<image, N> : N {
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
			if (handle_) {
				N::init();
				VK_ vkCreateImage(
					handle_of<N, device>(), &info,
					N::allocator(), &handle_) | popup{ "[IMAGE] Create image failure." };
			}
		}

		void reset() noexcept {
			if (handle_) {
				VK_ vkDestroyImage(
					handle_of<N, device>(), handle_,
					N::allocator());
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
	struct m<image_view, N> : N {
		constexpr m(image_view image_view, auto&&...others)
			: N{ forward_(others)... }
		{
		}

		~m() { reset(); }

		void init() {
			if (handle_) {
				N::init();
				VK_ vkCreateImageView(handle_of<N, device>(), &info, N::allocator(), &handle_)
					| popup{ "[IMAGE_VIEW] Create image view failure." };
			}
		}

		void reset() {
			if (handle_) {
				VK_ vkDestroyImageView(handle_of<N, device>(), handle_, N::allocator());
			}
		}

		auto handle() const noexcept {
			return handle_.value;
		}

	protected:
		VK_ VkImageViewCreateInfo info;

	private:
		reset_if_copy<VK_ VkImageView> handle_;
	};


}
