#pragma once

VKTL_EXPORT_ namespace vktl::detail {

	template<typename N>
	struct m<buffer, N> : N {
		using base = N;
		constexpr m(buffer buffer, auto&&...others)
			: N{ forward_(others)... } {
			info.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		}

		~m() { reset(); }

		void init() {
			if (!handle_) {
				N::init();
				VK_ vkCreateBuffer(handle_from<N, device>(), &info, N::allocator(), &handle_)
					| popup{ "[BUFFER] Create buffer failure." };
			}
		}

		void reset() {
			if (handle_) {
				VK_ vkDestroyBuffer(handle_from<N, device>(), handle_, N::allocator());
			}
		}

	protected:
		VK_ VkBufferCreateInfo info;

	private:
		reset_if_copy<VK_ VkBuffer> handle_;
		access_list<default_buffer_access> access_;
	};


	template<typename N>
	struct m<buffer_view, N> : N {
		constexpr m(buffer_view buffer_view, auto&&...others)
			: N{ forward_(others)... }
		{
		}

		~m() { reset(); }

		void init() {
			if (handle_) {
				N::init();
				VK_ vkCreateBufferView(handle_from<N, device>(), &info, N::allocator(), &handle_)
					| popup{ "[BUFFER_VIEW] Create buffer view failure." };
			}
		}

		void reset() {
			if (handle_) {
				VK_ vkDestroyBufferView(handle_from<N, device>(), handle_, N::allocator());
			}
		}

		auto handle() const noexcept {
			return handle_.value;
		}

	protected:
		VK_ VkBufferViewCreateInfo info;

	private:
		reset_if_copy<VK_ VkBufferView> handle_;
	};


}