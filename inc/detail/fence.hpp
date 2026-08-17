#pragma once

VKTL_EXPORT_ namespace vktl::vptr {
    struct fence : handle_owner<VkFence> {
		template<typename C>
		struct apply;

        // bool(*is_signaled_)(const void*) = nullptr;
        bool(*wait_)(const void*, uint64_t) = nullptr;
    };

	template<typename C>
	struct fence::apply : C {
		using base = C;

		template<typename T>
		void rebind() {
			vptr_ = { 
				.wait_ = [](const void* ptr, uint64_t timeout) -> bool {
					return static_cast<const T*>(ptr)->wait(timeout);
				}
			};
		}

		// bool is_signaled() const {
		//     return vptr_.is_signaled_(C::get_this());
		// }
		bool wait(uint64_t timeout = UINT64_MAX) const {
			return vptr_.wait_(C::get_this(), timeout);
		}

	private:
		fence vptr_;
	};
}

VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct m<fence, N> : N {
		constexpr m(fence fence, auto&&...others)
			: N{ forward_(others)... } {
			info.sType = VK_ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			info.flags = VK_ VK_FENCE_CREATE_SIGNALED_BIT; // not seperate first run, thus we just split it.
		}

		~m() { reset(); }

		void init() {
			if (handle_) {
				N::init();
				VK_ vkCreateFence(handle_of<N, device>(), &info,
					N::allocator(), &handle_) | popup{ "[FENCE] Create fence failure." };
			}
		}

		void reset() noexcept {
			if (handle_) {
				VK_ vkDestroyFence(handle_of<N, device>(), handle_, N::allocator());
			}
		}

		auto handle() const noexcept { return handle_.value; }

		void wait() const noexcept {
			
		}

	protected:
		VK_ VkFenceCreateInfo info;

	private:
		bool signal = false;
		reset_if_copy<VK_ VkFence> handle_;
	};
}