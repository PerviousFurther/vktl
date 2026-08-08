#pragma once

VKTL_EXPORT_ namespace vktl::vptr {

	
	struct descriptor {
		template<typename C>
		struct apply : C {

		};
	

	};

	template<typename H>
	struct resource_view {
		using handle_type = H;

		template<typename C>
		struct apply : C {
			template<typename>
			friend struct apply;

			using handle_type = H;

			box<descriptor> descriptor() const {
				return vptr_.get_descriptor_(C::get_this());
			}

			template<typename T>
			void rebind() noexcept {
				C::template rebind<T>();
				vptr_.get_descriptor_ = [](void const* ptr) -> box<descriptor> {
					return static_cast<T const*>(ptr)->get_descriptor();
				};
			}

			template<typename O>
			void rebind(apply<O> const& other) noexcept {
				C::rebind(other);
				vptr_ = other.vptr_;
			}

		private:
			resource_view vptr_;
		};

		box<descriptor>(*get_descriptor_)(void const*) = nullptr;
	};
}

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
				::std::lock_guard _{ N::get_lock() };
				N::init();
				VK_ vkCreateBuffer(handle_of<N, device>(), &info, N::allocator(), &handle_)
					| popup{ "[BUFFER] Create buffer failure." };
			}
		}

		void reset() {
			if (handle_) {
				for (auto c : childs_) { c.reset(); }
				::std::lock_guard _{ N::get_lock() };
				childs_.clear();
				VK_ vkDestroyBuffer(handle_of<N, device>(), handle_, N::allocator());
			}
		}

		auto handle() const noexcept { return handle_.value; }

		template<typename T>
		void append_child(object<T>& object) {
			childs_.emplace_back(object);
		}

	protected:
		VK_ VkBufferCreateInfo info {
			.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		};

	private:
		reset_if_copy<VK_ VkBuffer> handle_;
		vector<box<vptr::resetable>> childs_;
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
			N::init();

			::std::lock_guard _{ N::get_lock() };
			if (handle_) {
				VK_ vkCreateBufferView(handle_of<device>(this), &info, N::allocator(), &handle_)
					| popup{ "[BUFFER_VIEW] Create buffer view failure." };
			}
		}

		void reset() {
			if (handle_) {
				for (auto child : childs_) {
					child.reset();
				}

				::std::lock_guard _{ N::get_lock() };
				VK_ vkDestroyBufferView(handle_of<device>(this),
					exchange(handle_, VK_NULL_HANDLE), N::allocator());
			}
		}

		auto handle() const noexcept { return handle_.value; }

		template<typename T>
		void append_child(object<T>& object) {
			childs_.emplace_back(object);
		}

	protected:
		VK_ VkBufferViewCreateInfo info{ 
			.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO 
		};

	private:
		reset_if_copy<VK_ VkBufferView> handle_;
		vector<box<vptr::resetable>> childs_;
	};


}