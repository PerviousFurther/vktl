#pragma once

VKTL_EXPORT_ namespace vktl::vptr {
	
}

VKTL_EXPORT_ namespace vktl::detail {

	template<typename N>
	struct m<semaphore, N> : N {
		constexpr m(semaphore, auto&&...others)
			: N{ forward_(others)... } {
			info.sType = VK_ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		}

		~m() { reset(); }

		void init() {
			N::init();
			auto _ = locker_of(this);
			if (!handle_) {
				VK_ vkCreateSemaphore(
					handle_of<device>(this), &info,
					N::allocator(), &handle_.value) | popup{ "[SEMAPHORE] Create semaphore failure." };
			}
		}

		void reset() noexcept {
			auto _ = locker_of(this);
			if (handle_) {
				VK_ vkDestroySemaphore(
					handle_of<device>(this), ::std::exchange(handle_.value, VK_NULL_HANDLE),
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
