#pragma once

VKTL_EXPORT_ namespace vktl::vptr {
	using detail::box;

	struct initable {
		template<typename C>
		struct apply : C {
			template<typename>
			friend struct apply;

		protected:
			using base = C;

			template<typename T>
			void rebind() {
				base::template rebind<T>();
				vptr_.init_ = [](void* ptr) { static_cast<T*>(ptr)->init(); };
			}

			template<typename T>
			void rebind(initable<T> const& other) {
				base::rebind(other);
				vptr_ = other.vptr_;
			}

		public:
			void init() { vptr_.init_(C::get_this()); }

		private:
			initable vptr_;
		};

		void(*init_)(void*);
	};

	struct resetable {
		template<typename C>
		struct apply : C {
			template<typename>
			friend struct apply;

		protected:
			using base = C;

			template<typename T>
			void rebind() {
				base::template rebind<T>();
				vptr_.reset_ = [](void* ptr) { static_cast<T*>(ptr)->reset(); };
			}

			template<typename O>
			void rebind(apply<O> const& other) {
				base::rebind(other);
				vptr_ = other.vptr_;
			}

		public:
			void reset() {
				vptr_.reset_(C::get_this());
			}

		private:
			resetable vptr_;
		};

		void(*reset_)(void*) = nullptr;
	};

	template<typename C>
	using reusable = resetable::apply<initable::apply<C>>;

	template<typename Handle>
	struct handle_owner {
		template<typename C>
		struct apply : C {
			template<typename>
			friend struct apply;

			using handle_type = Handle;

		protected:
			using base = C;

			template<typename T>
			void rebind() {
				base::template rebind<T>();
				vptr_.handle_ = [](const void* ptr) -> handle_type {
					return static_cast<const T*>(ptr)->handle();
					};
			}

			template<typename O>
			void rebind(apply<O> const& other) {
				base::rebind(other);
				vptr_ = other.vptr_;
			}

		public:
			handle_type handle() const {
				return vptr_.handle_(C::get_this());
			}

		private:
			handle_owner vptr_;
		};

		handle_type(*handle_)(const void*) = nullptr;
	};

	template<typename E>
	struct element_of {
		template<typename C>
		struct apply : C {
			template<typename>
			friend struct apply;

			using element_type = E;

		protected:
			using base = C;

			template<typename T>
			void rebind() {
				base::template rebind<T>();
				vptr_.remove_element_ = [](void* ptr, element_type* element) {
					static_cast<T*>(ptr)->remove_from_handle(element);
					};
			}

			template<typename O>
			void rebind(apply<O> const& other) {
				base::rebind(other);
				vptr_ = other.vptr_;
			}

		public:
			void remove(element_type* element) {
				vptr_.remove_element_(C::get_this(), element);
			}

		private:
			element_of vptr_;
		};

		void(*remove_element_)(void*, E*) = nullptr;
	};

	template<typename V = void>
	struct unbind_notifier {
		using notifier_type = ::std::conditional_t<::std::is_void_v<V>,
			void(*)(void*),
			void(*)(void*, V)>;

		template<typename C>
		struct apply : C {
			template<typename>
			friend struct apply;

		protected:
			using base = C;

			template<typename T>
			void rebind() {
				base::template rebind<T>();
				if constexpr (::std::is_void_v<V>) {
					vptr_.notifier_ = [](void* ptr) {
						static_cast<T*>(ptr)->on_unbind();
						};
				}
				else {
					vptr_.notifier_ = [](void* ptr, V value) {
						static_cast<T*>(ptr)->on_unbind(::std::move(value));
						};
				}
			}

			template<typename O>
			void rebind(apply<O> const& other) {
				base::rebind(other);
				vptr_ = other.vptr_;
			}

		public:
			void unbind() {
				if constexpr (::std::is_void_v<V>) {
					vptr_.notifier_(C::get_this());
				}
				else {
					vptr_.notifier_(C::get_this(), V());
				}
			}

			void unbind(V value) requires(!::std::is_void_v<V>) {
				vptr_.notifier_(C::get_this(), ::std::move(value));
			}

		private:
			unbind_notifier vptr_;
		};

		notifier_type notifier_ = nullptr;
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

	template<typename T>
	struct locked : handle_<T> {
		using base = handle_<T>;

		locked(T value, lock_duck_& lock)
			: base{ value }
			, plock_{ nullptr }
		{}

		locked(T value, ::std::mutex& lock)
			: base{value}
			, plock_{&lock}
		{ plock_->lock(); }

		locked(T value, ::std::mutex* lock)
			: base{value}
			, plock_{lock}
		{ if(plock_){ plock_->lock(); } }

		locked(locked const&) = delete;
		locked& operator=(locked const&) = delete;

		~locked() { if (plock_) { plock_->unlock(); } }

	private:
		::std::mutex* plock_;
	};

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

