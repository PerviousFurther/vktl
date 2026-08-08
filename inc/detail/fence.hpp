#pragma once

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

	protected:
		VK_ VkFenceCreateInfo info;

	private:
		reset_if_copy<VK_ VkFence> handle_;
	};
}