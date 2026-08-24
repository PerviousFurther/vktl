#pragma once

VKTL_EXPORT_ namespace vktl::detail {

	template<typename N>
	struct m<event, N> : N {
		constexpr m(event, auto&&...others)
			: N{ forward_(others)... } {
		}

		~m() { reset(); }

		void init() {
			if (handle_) {
				N::init();
				VK_ vkCreateEvent(
					handle_of<device>(this), &info,
					N::allocator(), &handle_) | popup{ "[EVENT] Create event failure." };
			}
		}

		void reset() noexcept {
			if (handle_) {
				VK_ vkDestroyEvent(handle_of<device>(this), 
					handle_, N::allocator());
			}
		}

		auto handle() const noexcept {
			return handle_.value;
		}

	protected:
		VK_ VkEventCreateInfo info{ .sType = VK_ VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };

	private:
		reset_if_copy<VK_ VkEvent> handle_;
	};

	using namespace event_extensions;

}

VKTL_EXPORT_ namespace vktl::detail {

	template<typename N>
	struct m<semaphore, N> : N {
		constexpr m(semaphore, auto&&...others)
			: N{ forward_(others)... } {
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
		VK_ VkSemaphoreCreateInfo info{ .sType = VK_ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, };

	private:
		reset_if_copy<VK_ VkSemaphore> handle_;
	};

}

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
			vptr = {
				.wait_ = [](const void* ptr, uint64_t timeout) -> bool {
					return static_cast<const T*>(ptr)->wait(timeout);
				}
			};
		}

		// bool is_signaled() const {
		//     return vptr.is_signaled_(C::get_this());
		// }
		bool wait(uint64_t timeout = UINT64_MAX) const {
			return vptr.wait_(C::get_this(), timeout);
		}

	private:
		fence vptr;
	};
}

VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct m<fence, N> : N {
		constexpr m(fence fence, auto&&...others)
			: N{ forward_(others)... } {
			info.flags = VK_ VK_FENCE_CREATE_SIGNALED_BIT; // not seperate first run, thus we just split it.
		}

		~m() { reset(); }

		void init() {
			if (handle_) {
				N::init();
				VK_ vkCreateFence(handle_of<device>(this), &info,
					N::allocator(), &handle_) | popup{ "[FENCE] Create fence failure." };
			}
		}

		void reset() noexcept {
			if (handle_) {
				VK_ vkDestroyFence(handle_of<device>(this), handle_, N::allocator());
			}
		}

		auto handle() const noexcept { return handle_.value; }

	protected:
		VK_ VkFenceCreateInfo info{ .sType = VK_ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

	private:
		bool signal = false;
		reset_if_copy<VK_ VkFence> handle_;
	};
}