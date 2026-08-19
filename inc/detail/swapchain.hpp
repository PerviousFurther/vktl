#pragma once

// --- Agents specification -------------------------------------------------
// A swapchain is an independent frame host. It exposes one relocation-stable
// frame scope identity shared by child objects and per-frame revisions used by
// command invalidation. Successful recreation increments every affected frame;
// allocation-only consumers continue to use `frame_index_source`.
// --------------------------------------------------------------------------

#if !defined(VKTL_NO_WINDOW)

VKTL_EXPORT_ namespace vktl::detail {

	template<>
	struct is_queryable<swapchain, frame_scope> : ::std::true_type {};

	template<typename N>
	struct m<swapchain, N> : N {
		constexpr m(swapchain const& sw, auto&&...others)
			: N{ forward_(others)... }
			, info {
				.sType = VK_ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.minImageCount = sw.min_frame_count,
				.imageFormat = VK_ VK_FORMAT_MAX_ENUM,
				.imageColorSpace = VK_ VK_COLOR_SPACE_MAX_ENUM_KHR,
				.imageExtent = {
					.width = sw.width,
					.height = sw.height
				},
				.imageArrayLayers = 1,
				.imageUsage = VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // some may allow depth stencil, but this is normal usage.
				.imageSharingMode = VK_ VK_SHARING_MODE_EXCLUSIVE,
				.preTransform = VK_ VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
				.compositeAlpha = VK_ VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = VK_ VK_PRESENT_MODE_MAX_ENUM_KHR,
				.clipped = VK_TRUE,
			} 
		{
			frame_count_ = sw.min_frame_count;
			frame_index_ = 0u;
			frame_revisions_.resize(frame_count_, 1u);
		}

		~m() { reset(); }

		template<typename Fn = ::std::nullptr_t>
		void init(Fn&& fn = nullptr) {
			N::init();
			auto _ = locker_of(this);
			if (!handle_) {
				auto win = parent_of<window>(this);
				auto dv = parent_of<device>(this);

				auto hdv = dv->handle();
				auto phydv = dv->physical_device();

				info.surface = win->handle();

				VK_ VkSurfaceCapabilitiesKHR capabilities;
				vector<VK_ VkSurfaceFormatKHR> formats;
				vector<VK_ VkPresentModeKHR> present_modes;
				if constexpr (::std::invocable<Fn&, VK_ VkSwapchainCreateInfoKHR&, VK_ VkSurfaceCapabilities2EXT&>) {
					fn(info, capabilities);
					assert(info.minImageCount); // atleast image.
					assert(info.imageExtent.width && info.imageExtent.height); // atleast width and height.
				}

				if constexpr (::std::invocable<Fn&, VK_ VkSwapchainCreateInfoKHR&, vector<VK_ VkSurfaceFormatKHR>&>) {
					fn(info, formats);
					assert(info.imageFormat != VK_ VK_FORMAT_MAX_ENUM);
					assert(info.imageColorSpace != VK_ VK_COLOR_SPACE_MAX_ENUM_KHR);
				}
				else if (info.imageFormat == VK_ VK_FORMAT_MAX_ENUM || info.imageColorSpace == VK_ VK_COLOR_SPACE_MAX_ENUM_KHR) {
					invoke(formats, VK_ vkGetPhysicalDeviceSurfaceFormatsKHR, phydv, win->handle())
						| popup{"[SWAPCHAIN] No any surface format was supported."};
					if (info.imageFormat == VK_ VK_FORMAT_MAX_ENUM) {
						info.imageFormat = formats[0u].format;
					}
					if (info.imageColorSpace == VK_ VK_COLOR_SPACE_MAX_ENUM_KHR) {
						info.imageColorSpace = formats[0u].colorSpace;
					}
				}

				if constexpr (::std::invocable<Fn&, VK_ VkSwapchainCreateInfoKHR&, vector<VK_ VkPresentModeKHR>&>) {
					fn(info, present_modes);
					assert(info.imageColorSpace != VK_ VK_COLOR_SPACE_MAX_ENUM_KHR);
				}
				else if (info.presentMode == VK_ VK_PRESENT_MODE_MAX_ENUM_KHR) {
					invoke(present_modes, VK_ vkGetPhysicalDeviceSurfacePresentModesKHR, phydv, win->handle())
						| popup{"[SWAPCHAIN] No any present mode was supported."};
					info.presentMode = present_modes[0u];
				}
				
				if (info.imageExtent.width == invalid) {
					info.imageExtent.width = win->width();
				}
				if (info.imageExtent.height == invalid) {
					info.imageExtent.height = win->height();
				}
				VK_ vkCreateSwapchainKHR(dv->handle(), &info, N::allocator(), &handle_.value)
					| popup("[SWAPCHAIN] Create swapchain failure.");
				if (frame_revisions_.size() != frame_count_) {
					frame_revisions_.resize(frame_count_, 1u);
				}

				// invoke(images_, &VK_ vkGetSwapchainImagesKHR, hdv, handle_.value)
				// 	| popup("[SWAPCHAIN] Cannot get_by swapchain images.");
				ensure_frame_index();
			}
		}

		void reset() {
			auto _ = locker_of(this);
			if (handle_) {
				VK_ vkDestroySwapchainKHR(handle_of<device>(this),
					::std::exchange(handle_.value, VK_NULL_HANDLE), N::allocator());
				invalidate_frames();
				// images_.clear();
			}
		}

		constexpr auto handle() const noexcept { return handle_.value; }
		constexpr auto surface() const noexcept { return info.surface; }

		constexpr auto frame_count() const noexcept { return frame_count_; }
		// constexpr auto image_count() const noexcept { return 1u; }

		// locked<VK_ VkImage> image(uint32_t) const noexcept {
		// 	return { images_[ensure_frame_index()], lock_of(this) };
		// }
		// locked<VK_ VkImage> image(uint32_t) noexcept {
		// 	return { images_[ensure_frame_index()], lock_of(this) };
		// }

		constexpr auto frame_index() const noexcept {
			return ensure_frame_index();
		}

		constexpr frame_scope_id frame_scope_identity() const noexcept {
			return frame_scope_id_;
		}

		uint64_t frame_revision(uint32_t frame) const noexcept {
			assert(frame < frame_revisions_.size());
			return frame_revisions_[frame];
		}

		void fence(object_of<fence> auto& object) {
			fence_ = {object};
		}

	protected:
		VK_ VkSwapchainCreateInfoKHR info;

	private:
		uint32_t frame_count_ = invalid, frame_index_ = invalid;
		frame_scope_id frame_scope_id_ = allocate_frame_scope_id();
		vector<uint64_t> frame_revisions_;
		box<vptr::handle_owner<VK_ VkFence>> fence_;
		copyable_if_null<VK_ VkSwapchainKHR> handle_{ VK_NULL_HANDLE };

	private:
		void invalidate_frames() noexcept {
			for (auto& revision : frame_revisions_) {
				++revision;
				if (revision == 0u) ++revision;
			}
		}

		uint16_t ensure_frame_index() const {
			if (!fence_.empty()) {
				if (auto handle = fence_.handle()) VKTL_LIKELY {
					VK_ vkWaitForFences(handle_of<device>(this), 1u, &handle, false, maximum);
				}
			}
			return uint16_t(frame_index_);
		}
	};

}

#endif
