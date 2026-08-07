#pragma once

VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct m<shared_single_thread_object, N> : N {
		m(auto, auto&&...others)
			: N{ forward_(others)... }
		{
		}

		uint32_t add_ref() noexcept {
			assert(refc_); // try add from on zero from's object.
			return ++refc_;
		}
		uint32_t release() noexcept {
			assert(refc_); // try release on zero from's object.
			return --refc_;
		}

	private:
		uint32_t refc_ = 1u;
	};

	template<typename N>
	struct m<shared_cross_thread_object, N> : N {
		m(auto, auto&&...others)
			: N{ forward_(others)... }
		{
		}

		uint32_t add_ref() noexcept {
			auto old = refc_.fetch_add(1u, ::std::memory_order_relaxed);
			assert(old); // try add from on zero from's object.
			return old + 1u;
		}
		uint32_t release() noexcept {
			auto old = refc_.fetch_sub(1u, ::std::memory_order_acq_rel);
			assert(old); // try sub from on zero from's object.
			return old - 1u;
		}

	private:
		::std::atomic_uint32_t refc_ = 1u;
	};

	using namespace extensions;

	template<typename...Ts, typename N>
	struct m<from<Ts...>, N> : N {
		static constexpr auto num_parents = sizeof...(Ts);
		using parents = ts<Ts...>;

		constexpr m(from<Ts...> const& value, auto&&...others)
			: N{ forward_(others)... } {
		}

		template<typename T>
		constexpr auto parent() const noexcept {
			return parent_direct<T>(this);
		}

		template<::std::size_t index>
		constexpr auto parent() const noexcept {
			return::std::get<index>(parents_);
		}
		constexpr auto parent() const noexcept {
			return::std::get<0u>(parents_);
		}

		template<typename T>
		constexpr auto all_parent() const noexcept {
			return all_parent_impl<T, 0u>(this);
		}

	protected:
		void init() {
			::std::apply([](auto...ptr) { ((ptr->init()), ...); }, parents_);
		}

		// void reset() {
		// 	::std::apply([](auto...ptr) { ((ptr->reset()), ...); }, parents_);
		// }

	private:
		template<typename T, ::std::size_t index>
		static constexpr auto all_parent_impl(auto pthis, auto...ptrs) {
			if constexpr (index < num_parents) {
				if constexpr (object_of<tuple_at_t<index, parents>, T>) {
					return all_parent_impl<T, index + 1u>(ptrs..., get<index>(pthis->parents_));
				}
				else {
					return all_parent_impl<T, index + 1u>(ptrs...);
				}
			}
			else {
				return::std::tuple(ptrs...);
			}
		}

		template<typename T, ::std::size_t index = 0u>
		static constexpr auto parent_direct(auto pthis) noexcept {
			if constexpr (index < num_parents) {
				if constexpr (object_of<tuple_at_t<index, parents>, T>) {
					return pthis->template parent<index>();
				}
				else {
					return parent_direct<T, index + 1>(pthis);
				}
			}
			else {
				return parent_recursive<T, 0u>(pthis);
			}
		}

		template<typename T, ::std::size_t index = 0u>
		static constexpr auto parent_recursive(auto pthis) noexcept {
			if constexpr (index < num_parents) {
				auto pparent = pthis->template parent<index>();
				if constexpr (requires { pparent->template parent<T>(); }) {
					auto presult = pparent->template parent<T>();
					if constexpr (!::std::is_null_pointer_v<decltype(presult)>) {
						return presult;
					}
					else {
						return parent_recursive<T, index + 1>(pthis);
					}
				}
				else {
					return parent_recursive<T, index + 1>(pthis);
				}
			}
			else {
				return nullptr;
			}
		}

	private:
		::std::tuple<Ts*...> parents_;
	};

	template<typename N, typename T>
	constexpr auto handle_from() noexcept { return N::template parent<T>()->handle(); }


	template<template<typename>typename VPtr>
	struct box : VPtr<box<VPtr>> {
		using base = VPtr<box<VPtr>>;

		template<typename T>
		constexpr box(::std::in_place_t, T& ptr)
			: ptr_{ &ptr } {
			base::template rebind<T>();
		}

		template<typename T>
		constexpr box(T& ptr)
			: ptr_{ &ptr } {
			rebind<T>();
		}

		template<typename T>
		constexpr box(T* ptr)
			: ptr_{ ptr } {
			rebind<T>();
		}

		// template<typename T>
		// constexpr box(share<T> ptr)
		// 	: ptr_{ ptr.get() } {
		// 	rebind<T>();
		// }

		constexpr box(box const& other)
			: base{ static_cast<base const&>(other) }
			, ptr_{ other.ptr_ }
			, release_{ other.release_ }
			, add_ref_{ other.add_ref_ } {
			assert(add_ref_ || !add_ref_ && !release_);
			if (add_ref_ && ptr_) {
				add_ref_(ptr_);
			}
		}
		constexpr box& operator=(box const& other) {
			if (&other != this) {
				reset();

				add_ref_ = other.add_ref_;
				release_ = other.release_;

				assert(add_ref_ || !add_ref_ && !release_);

				static_cast<base&>(*this) = other;
				if (ptr_ == other.ptr_ && add_ref_) {
					add_ref_(ptr_);
				}
			}
			return *this;
		}

		constexpr box(box&& other) noexcept
			: base{ static_cast<base&&>(other) }
			, ptr_{ ::std::exchange(other.ptr_, nullptr) }
			, add_ref_{ ::std::exchange(other.add_ref_, nullptr) }
			, release_{ ::std::exchange(other.release_, nullptr) }
		{
		}

		constexpr box& operator=(box&& other) noexcept {
			if (this != &other) {
				reset();
				static_cast<base&>(*this) = static_cast<base&&>(other);
				ptr_ = ::std::exchange(other.ptr_, nullptr);
				add_ref_ = ::std::exchange(other.add_ref_, nullptr);
				release_ = ::std::exchange(other.release_, nullptr);
			}
			return *this;
		}

		~box() { reset(); }

		template<typename T>
		void rebind() {
			base::template rebind<T>();
			if constexpr (requires (T & v) { v.add_ref(); v.release(); }) {
				add_ref_ = [](void* ptr) { static_cast<T*>(ptr)->add_ref(); };
				release_ = [](void* ptr) { static_cast<T*>(ptr)->release(); };
			}
			else {
				release_ = [](void* ptr) { delete static_cast<T*>(ptr); };
			}
		}

		bool shared() const noexcept { return add_ref_; }
		bool unique() const noexcept { return !add_ref_ && release_; }
		bool view() const noexcept { return !add_ref_ && !release_; }

		void reset() noexcept {
			if (release_) {
				::std::exchange(release_, nullptr)(::std::exchange(ptr_, nullptr));
			}
			add_ref_ = nullptr;
		}

		void* get() const noexcept { return ptr_; }

	private:
		void* ptr_ = nullptr;
		::std::uint32_t(*add_ref_)(void*) = nullptr;
		::std::uint32_t(*release_)(void*) = nullptr;
	};

	template<typename C>
	struct default_vptr {
	protected:
		constexpr auto self() const noexcept {
			return static_cast<C const*>(this)->ptr();
		}
	};

	struct default_buffer_access : range<VK_ VkDeviceSize> {
		using range_type = range<VK_ VkDeviceSize>;

		uint16_t index;
		VK_ VkBufferCreateFlags flags;
		VK_ VkBufferUsageFlags usage;
		VK_ VkPipelineStageFlags stage;
		VK_ VkAccessFlags access;
		VK_ VkDependencyFlags dependency;

		constexpr bool operator==(default_buffer_access const& other) const noexcept {
			return access == other.access;
		}

		constexpr auto& operator|=(default_buffer_access const& other) noexcept {
			if (&other != this) {
				flags |= other.flags; usage |= other.usage;
				stage |= other.stage;  access |= other.access;
				dependency |= other.dependency;
			}
			return *this;
		}
	};

	struct default_image_access : VK_ VkImageSubresourceRange {
		using range_type = VK_ VkImageSubresourceRange;
		// uint32_t width;
		// uint32_t height;
		// uint16_t depth;
		uint16_t index;
		VK_ VkImageCreateFlags flags;
		VK_ VkImageUsageFlags usage;
		VK_ VkImageLayout layout; // recommanded layout.
		VK_ VkPipelineStageFlags stage;
		VK_ VkAccessFlags access;
		VK_ VkDependencyFlags dependency;

		constexpr bool operator==(default_image_access const& other) const noexcept {
			return access == other.access;
		}

		constexpr auto& operator|=(default_image_access const& other) noexcept {
			if (&other != this) {
				if (layout == VK_ VK_IMAGE_LAYOUT_UNDEFINED) {
					layout = other.layout;
				}
				else if (other.layout == VK_ VK_IMAGE_LAYOUT_GENERAL || layout != other.layout) {
					layout = VK_ VK_IMAGE_LAYOUT_GENERAL;
				}

				flags |= other.flags; usage |= other.usage;
				stage |= other.stage; access |= other.access;
				dependency |= other.dependency;
			}
			return *this;
		}
	};


}