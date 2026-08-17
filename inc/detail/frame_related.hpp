#pragma once

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

		locked<handle_type> handle(uint32_t index) const noexcept {
			return { handle_at(index), lock_of(this) };
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
		handle_type& handle_at(uint32_t index) const noexcept {
			if constexpr (base::have_frame_scope) {
				assert(parent_of<frame_scope>(this)->frame_count() > index);
				return handles_[index];
			}
			else {
				return handles_.value;
			}
		}

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
		mutable ::std::conditional_t<base::have_frame_scope,
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