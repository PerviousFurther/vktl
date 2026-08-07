#pragma once

VKTL_EXPORT_ namespace vktl::detail {

	template<typename N>
	struct m<semaphore, N> : N {
		constexpr m(semaphore, auto&&...others)
			: N{ forward_(others)... } {
			info.sType = VK_ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		}

		~m() { reset(); }

		void init() {
			if (handle_) {
				N::init();
				VK_ vkCreateSemaphore(
					handle_from<N, device>(), &info,
					N::allocator(), &handle_) | popup{ "[SEMAPHORE] Create semaphore failure." };
			}
		}

		void reset() noexcept {
			if (handle_) {
				VK_ vkDestroySemaphore(
					handle_from<N, device>(), handle_,
					N::allocator());
			}
		}

		auto handle() const noexcept {
			return handle_.value;
		}

	protected:
		VK_ VkSemaphoreCreateInfo info;

	private:
		reset_if_copy<VK_ VkSemaphore> handle_;
	};

}
