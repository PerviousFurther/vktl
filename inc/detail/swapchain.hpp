#pragma once

#if !defined(VKTL_NO_WINDOW)

VKTL_EXPORT_ namespace vktl::detail {

	struct frame_scope; // tag for frame related objects.

	template<typename N>
	struct basic_frame_related : N {
		static constexpr auto have_frame_scope = parent_have<N, frame_scope>;

		basic_frame_related(auto&&...others)
			: N{ forward_(others)... }
		{
		}

		constexpr uint32_t frame_count() const noexcept {
			if constexpr (have_frame_scope) {
				return parent_of<frame_scope>(this)->frame_count();
			}
			else {
				return 1u;
			}
		}

		constexpr uint32_t frame_index() const noexcept {
			if constexpr (have_frame_scope) {
				return parent_of<frame_scope>(this)->frame_index();
			}
			else {
				return 1u;
			}
		}
	};


	// helper classes.

	template<typename N, typename Trait>
	struct basic_frame_related_handle : basic_frame_related<N> {
		using base = basic_frame_related<N>;
		using handle_type = typename Trait::handle_type;

		basic_frame_related_handle(auto&&...others)
			: base{ forward_(others)... } {
			construct_handles();
		}

		basic_frame_related_handle(basic_frame_related_handle const&) {
			construct_handles();
		}

		basic_frame_related_handle& operator=(basic_frame_related_handle const& other) {
			assert(is_null());
			if constexpr (!base::have_frame_scope) {
				handles_ = other.handles_;
			}
		}

		basic_frame_related_handle(basic_frame_related_handle&& other) {
			if constexpr (base::have_frame_scope) {
				handles_ = ::std::exchange(other.handles_, nullptr);
			}
			else {
				handles_ = ::std::exchange(other.handles_, VK_NULL_HANDLE);
			}
		}

		basic_frame_related_handle& operator=(basic_frame_related_handle&& other) {
			assert(is_null());
			if constexpr (base::have_frame_scope) {
				if constexpr (::std::is_trivially_copyable_v<handle_type>) {
					::std::memcpy(handles_, other.handles_, sizeof(handle_type) * this->frame_count());
					::std::memset(other.handles_, sizeof(handle_type) * this->frame_count()); // should?
				}
				else {
					for (auto its = other.handles_, itd = handles_; itd != handles_ + this->frame_count(); (void)++itd, ++its) {
						*its = ::std::move(*its);
					}
				}
			}
			else {
				handles_ = other.handles_;
			}
		}

		~basic_frame_related_handle() {
			if constexpr (base::have_frame_scope) {
				delete[] handles_;
			}
		}

		locked<handle_type> handle() const noexcept {
			return handle(base::frame_index());
		}

		locked<handle_type> handle(uint32_t index = 0u) const noexcept {
			if (base::have_frame_scope) {
				assert(parent_of<frame_scope>(this)->frame_count() >= index);
				return { handles_, index, lock_of(this) };
			}
			else {
				return { handles_, lock_of(this) };
			}
		}

		constexpr bool is_null() const noexcept {
			if constexpr (base::have_frame_scope) {
				for (auto c : ::std::span(handles_, base::frame_count())) {
					if (c != VK_NULL_HANDLE) {
						return false;
					}
				}
				return true;
			}
			else {
				return handles_ == VK_NULL_HANDLE;
			}
		}

	protected:
		void generate(auto const& info, const char* error) requires(requires { Trait::create; }) {
			if constexpr (base::have_frame_scope) {
				uint32_t frame_count = base::frame_count();
				for (auto i = 0u; i < frame_count; i++) try {
					Trait::create(handle_of<device>(this), &info, N::allocator(), handles_ + i)
						| popup{ error };
				}
				catch (...) {
					this->destory();
					throw;
				}
			} 
			else {
				Trait::create(handle_of<device>(this), &info, N::allocator(), handles_)
					| popup{ error };
			}
		}
		void destroy() requires(requires { Trait::destroy; }) {
			if constexpr (base::have_frame_scope) {
				uint32_t frame_count = base::frame_count();
				for (auto i = 0u; i < frame_count; i++) if (handles_[i]) {
					Trait::destroy(handle_of<device>(this), handles_[i], N::allocator());
				}	
			}
			else {
				Trait::destroy(handle_of<device>(this), handles_, N::allocator());
			}
		}

	private:
		void construct_handles() {
			if constexpr (base::have_frame_scope) {
				handles_ = new handle_type[base::frame_count()];
				for (auto& handle : span(handles_, base::frame_count())) {
					handle = VK_NULL_HANDLE;
				}
			}
			else {
				handles_.value = VK_NULL_HANDLE;
			}
		}

	private:
		::std::conditional_t<base::have_frame_scope,
			handle_type*, reset_if_copy<handle_type>> handles_;
	};

}

VKTL_EXPORT_ namespace vktl::vptr {

	struct frame_related {
		template<typename C>
		struct apply;
		
		vfn<uint32_t() const> frame_index_;
		vfn<uint32_t() const> frame_count_;
	};

	template<typename C>
	struct frame_related::apply : C {
		using base = C;

		template<typename T>
		void rebind() noexcept {
			vptr_ = {
				.frame_index_ = [](void const* ptr) noexcept {
					return static_cast<T const*>(ptr)->frame_index();
				},
				.frame_count_ = [](void const* ptr) noexcept {
					return static_cast<T const*>(ptr)->frame_count();
				},
			};
		}

		uint32_t frame_index() const noexcept {
			return vptr_.frame_index_(C::get_this());
		}
		uint32_t frame_count() const noexcept {
			return vptr_.frame_count_(C::get_this());
		}

		frame_related vptr_;
	};
}

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
		{}

		~m() { reset(); }

		template<typename Fn = ::std::nullptr_t>
		void init(Fn&& fn = nullptr) {
			N::init();
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

				// invoke(images_, &VK_ vkGetSwapchainImagesKHR, hdv, handle_.value)
				// 	| popup("[SWAPCHAIN] Cannot get_by swapchain images.");
				ensure_frame_index();
			}
		}

		void reset() {
			if (handle_) {
				VK_ vkDestroySwapchainKHR(handle_of<device>(this), handle_, N::allocator());
				// images_.clear();
			}
		}

		constexpr auto handle() const noexcept { return handle_.value; }

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

		void fence(object_of<fence> auto& object) {
			fence_ = {object};
		}

	protected:
		VK_ VkSwapchainCreateInfoKHR info;

	private:
		uint32_t frame_count_ = invalid, frame_index_ = invalid;
		box<vptr::handle_owner<VK_ VkFence>> fence_;
		copyable_if_null<VK_ VkSwapchainKHR> handle_{ VK_NULL_HANDLE };

	private:
		uint16_t ensure_frame_index() {
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
