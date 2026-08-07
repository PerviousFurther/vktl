#pragma once

VKTL_EXPORT_ namespace vktl::detail {

	template<typename N>
	struct m<event, N> : N {
		constexpr m(event, auto&&...others)
			: N{ forward_(others)... } {
			info.sType = VK_ VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
		}

		~m() { reset(); }

		void init() {
			if (handle_) {
				N::init();
				VK_ vkCreateEvent(
					handle_from<N, device>(), &info,
					N::allocator(), &handle_) | popup{ "[EVENT] Create event failure." };
			}
		}

		void reset() noexcept {
			if (handle_) {
				VK_ vkDestroyEvent(
					handle_from<N, device>(), handle_,
					N::allocator());
			}
		}

		auto handle() const noexcept {
			return handle_.value;
		}

	protected:
		VK_ VkEventCreateInfo info;

	private:
		reset_if_copy<VK_ VkEvent> handle_;
	};

	using namespace event_extensions;

}