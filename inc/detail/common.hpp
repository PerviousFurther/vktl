#pragma once

VKTL_EXPORT_ namespace vktl::vptr {
	using detail::box;
	using detail::type_box;

	template<typename C>
	struct initable : C {
		template<typename>
		friend struct initable;
	protected:
		using base = C;

		template<typename T>
		void rebind() {
			base::template rebind<T>();
			init_ = [](void* ptr) { static_cast<T*>(ptr_)->init(); };
		}

		template<typename T>
		void rebind(initable<T> const& other) {
			base::rebind(other);
			init_ = other.init_;
		}

	public:
		void init() { init_(C::get_this()); }

	private:
		void(*init_)(void*);
	};

	template<typename C>
	struct resetable : C {
		template<typename>
		friend struct resetable;

	protected:
		using base = C;

		template<typename T>
		void rebind() {
			base::template rebind<T>();
			reset_ = [](void* ptr) { static_cast<T*>(ptr)->reset(); };
		}

		template<typename T>
		void rebind(resetable<T> const& other) {
			base::rebind(other);
			reset_ = other.reset_;
		}

	public:
		void reset() {
			if (reset_) { reset_(C::get_this()); }
		}

	private:
		void(*reset_)(void*) = nullptr;
	};

	template<typename C>
	using reusable = resetable<initable<C>>;

	template<typename C, typename H>
	struct handle_owner : C {
		using handle_type = H;

	protected:
		using base = C;

		template<typename T>
		void rebind() {
			C::template rebind<T>();
			handle_ = [](const void* ptr) -> handle_type {
				return static_cast<const T*>(ptr)->handle();
			};
		}

	public:
		handle_type handle() const {
			return handle_(C::get_this());
		}

	private:
		handle_type(*handle_)(void const*) = nullptr;
	};
}

VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct m<shared_, N> : N {
		m(auto, auto&&...others)
			: N{ forward_(others)... }
		{
		}

		uint32_t add_ref() noexcept {
			assert(refc_); // try add from on zero from's object.
			if constexpr (object_of<N, lockable_>) {
				::std::lock_guard _{N::get_lock()};
				return ++refc_;
			}
			else {
				return ++refc_;
			}
			
		}
		uint32_t release() noexcept {
			assert(refc_); // try release on zero from's object.
			if constexpr (object_of<N, lockable_>) {
				::std::lock_guard _{ N::get_lock() };
				return ++refc_;
			}
			else {
				return --refc_;
			}
		}

	private:
		uint32_t refc_ = 1u;
	};

	template<typename N>
	struct m<cross_thread_shared_, N> : N {
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
		void init() { ::std::apply([](auto...ptr) { ((ptr->init()), ...); }, parents_); }

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

	template<typename N>
	struct m<lockable_, N> : N {
		using mutex_type = ::std::mutex;

		m(lockable_, auto&&...others) : N{} {}

	private:
		mutable::std::mutex lock_;
	};

	template<typename T>
	constexpr auto is_lockable = object_of<T, lockable_>;



	using namespace extensions;

	template<typename N>
	struct m<allocate_from, N> : N {
	private:
		using allocator_type = box<default_handle_allocator_vptr>;

	public:
		constexpr m(allocate_from const& from, auto&&...others)
			: N{forward_(others)...}
			, allocator_{ from.allocator }
			, callbacks_{
				.pUserData = &allocator_,
				.pfnAllocation = [](void* p, size_t size, size_t alignment, VkSystemAllocationScope) -> void* {
					return static_cast<allocator_type*>(p)->allocate(size, alignment);
				},
				.pfnReallocation = alloator_.reallocable() ? [](void* p, void* ori, size_t size, size_t alignment, VkSystemAllocationScope) -> void* {
					return static_cast<allocator_type*>(p)->reallocate(ori, size, alignment);
				} : nullptr,
				.pfnFree = [](void* p, void* ptr) -> void {
					static_cast<allocator_type*>(p)->free(ptr);
				},
				.pfnInternalAllocation = nullptr,
				.pfnInternalFree = nullptr,
			} {
			
		}

		void relocate() noexcept { callbacks_.pUserData = allocator_; }

		constexpr VK_ VkAllocationCallbacks const* allocator() const { return &callback_; }

	private:
		allocator_type allocator_;
		VK_ VkAllocationCallbacks callbacks_;
	};
}

