#pragma once

// Agents specification:
// Parent type searches are consteval index enumerations. Runtime traversal must
// dereference parent links only through the indexed parent<index>() overload.
//
// Interface style: reusable vptr capabilities and parent-link mixins connect
// independently composed objects without virtual inheritance.
// Implementation: each vptr stores explicit function pointers, while `from`
// owns only stable parent addresses and forwards initialization deliberately.

VKTL_EXPORT_ namespace vktl::vptr {
	using detail::box;

	struct initable {
		template<typename C>
		struct apply;

		vfn<void()> init_ = nullptr;
	};

	template<typename C>
	struct initable::apply : C {
		using base = C;

		template<typename>
		friend struct apply;

		template<typename T>
		void rebind() {
			vptr.init_ = [](void* ptr) { static_cast<T*>(ptr)->init(); };
		}

		void init() {
			vptr.init_(C::get_this());
		}

		initable vptr;
	};

	// -----------------------------------------------------------------------------
	// 2. resetable
	// -----------------------------------------------------------------------------
	struct resetable {
		template<typename C>
		struct apply;

		vfn<void()> reset_ = nullptr;
	};

	template<typename C>
	struct resetable::apply : C {
		using base = C;

		template<typename T>
		void rebind() {
			this->template rebind_next<T>(this);
			vptr.reset_ = [](void* ptr) { static_cast<T*>(ptr)->reset(); };
		}

		void reset() {
			vptr.reset_(C::get_this());
		}

		resetable vptr;
	};

	template<typename C>
	using reusable = resetable::apply<initable::apply<C>>;

	// -----------------------------------------------------------------------------
	// 3. handle_owner
	// -----------------------------------------------------------------------------
	template<typename Handle>
	struct handle_owner {
		using handle_type = Handle;

		template<typename C>
		struct apply;

		vfn<handle_type() const> handle_ = nullptr;
	};

	template<typename Handle>
	template<typename C>
	struct handle_owner<Handle>::apply : C {
		using base = C;

		template<typename T>
		void rebind() {
			vptr.handle_ = [](const void* ptr) -> handle_type {
				return static_cast<const T*>(ptr)->handle();
				};
		}

		handle_type handle() const {
			return vptr.handle_(C::get_this());
		}

		handle_owner vptr;
	};

	// -----------------------------------------------------------------------------
	// 4. element_of
	// -----------------------------------------------------------------------------
	template<typename E>
	struct element_of {
		template<typename C>
		struct apply;

		vfn<void(E*)> remove_element_ = nullptr;
	};

	template<typename E>
	template<typename C>
	struct element_of<E>::apply : C {
		using base = C;
		using element_type = E;

		template<typename T>
		void rebind() {
			vptr.remove_element_ =
				[](void* ptr, element_type* element) {
				static_cast<T*>(ptr)->remove_from_handle(element);
				};
		}

		void remove(element_type* element) {
			vptr.remove_element_(C::get_this(), element);
		}

		element_of vptr;
	};

	// -----------------------------------------------------------------------------
	// 5. unbindable
	// -----------------------------------------------------------------------------
	template<typename V = void> // V allow reference type.
	struct unbindable {
		using notifier_type = ::std::conditional_t<::std::is_void_v<V>,
			vfn<void()>,
			vfn<void(V)>>;

		template<typename C>
		struct apply;

		notifier_type notifier_ = nullptr;
	};

	template<typename V>
	template<typename C>
	struct unbindable<V>::apply : C {
		using base = C;

		template<typename T>
		void rebind() {
			if constexpr (::std::is_void_v<V>) {
				vptr.notifier_ = [](void* ptr) { static_cast<T*>(ptr)->unbind(); };
			}
			else {
				vptr.notifier_
					= [](void* ptr, V value) { static_cast<T*>(ptr)->unbind(::std::move(value)); };
			}
		}

		void unbind() {
			if constexpr (::std::is_void_v<V>) {
				vptr.notifier_(C::get_this());
			}
			else {
				vptr.notifier_(C::get_this(), V());
			}
		}

		void unbind(V value) requires(!::std::is_void_v<V>) {
			vptr.notifier_(C::get_this(), static_cast<V>(value));
		}

		unbindable vptr;
	};

	// -----------------------------------------------------------------------------
	// 6. bindable
	// -----------------------------------------------------------------------------
	template<typename V = void> // V allow reference type.
	struct bindable {
		using notifier_type = ::std::conditional_t<::std::is_void_v<V>, vfn<void()>, vfn<void(V)>>;

		template<typename C>
		struct apply;

		notifier_type notifier_ = nullptr;
	};

	template<typename V>
	template<typename C>
	struct bindable<V>::apply : C {
		using base = C;

		template<typename T>
		void rebind() {
			if constexpr (::std::is_void_v<V>) {
				vptr.notifier_ = [](void* ptr) { static_cast<T*>(ptr)->bind(); };
			}
			else {
				vptr.notifier_
					= [](void* ptr, V value) {
					static_cast<T*>(ptr)->bind(static_cast<V>(value)); };
			}
		}

		void bind() {
			if constexpr (::std::is_void_v<V>) {
				vptr.notifier_(C::get_this());
			}
			else {
				vptr.notifier_(C::get_this(), V());
			}
		}

		void bind(V value) requires(!::std::is_void_v<V>) {
			vptr.notifier_(C::get_this(), ::std::move(value));
		}

		bindable vptr;
	};

	// -----------------------------------------------------------------------------
	// 7. child_of
	// -----------------------------------------------------------------------------
	template<typename VPtr, typename... Ts>
	struct child_of {
		template<typename C>
		struct apply;

		vfn<box<VPtr>()> parent_;
	};

	template<typename VPtr, typename... Ts>
	template<typename C>
	struct child_of<VPtr, Ts...>::apply : C {
		using base = C;

		template<typename T>
		void rebind() {
			vptr = {
				.parent_ = [](void* ptr) -> box<VPtr> {
					return static_cast<T*>(ptr)->template parent<Ts...>();
				},
			};
		}

		auto parent() {

		}

	private:
		child_of vptr;
	};

}

VKTL_EXPORT_ namespace vktl::detail {

	// RI required interface.
	template<typename N, typename...RI>
	class basic_parent : N {
		using child_box = box<RI...>;

	protected:
		template<typename T>
		bool add_child(T& child) requires(::std::constructible_from<child_box, T&>) {
			void* pchild = ::std::addressof(child);
			auto it = childs_.find(pchild);
			if (it == childs_.end()) {
				childs_.emplace(pchild, child_box{ child });
				return true;
			}
			else {
				return false;
			}
		}

		bool remove_child(void* child) {
			auto it = childs_.find(child);
			if (it != childs_.end()) {
				childs_.erase(it);
				return true;
			}
			else {
				return false;
			}
		}

	private:
		::std::unordered_map<void*, child_box> childs_;
	};

	template<typename N>
	struct m<shared_, N> : N {
		m(auto, auto&&...others)
			: N{ forward_(others)... }
		{
		}

		// shared object usaully not on stack, copy or move is forbidden.

		m(m const&) = delete;
		m& operator=(m const&) = delete;
		m(m&&) = delete;
		m& operator=(m&&) = delete;
		

		uint32_t add_ref() noexcept {
			::std::lock_guard _{ N::get_lock() };
			assert(refc_); // try add from on zero from's object.
			return ++refc_;
			
		}
		uint32_t release() noexcept {
			::std::lock_guard _{ N::get_lock() };
			assert(refc_); // try release on zero from's object.
			return --refc_;
		}

	private:
		uint32_t refc_ = 1u;
	};

	// cross thread shared object usaully not on stack, copy or move is forbidden.
	template<typename N>
	struct m<cross_thread_shared_, N> : N {

		m(auto, auto&&...others)
			: N{ forward_(others)... }
		{
		}

		uint32_t add_ref() noexcept {
			auto old = refc_.fetch_add(1u, ::std::memory_order_relaxed);
			assert(old); // try add from on zero referenced object.
			return old + 1u;
		}
		uint32_t release() noexcept {
			auto old = refc_.fetch_sub(1u, ::std::memory_order_acq_rel);
			assert(old); // try sub from on zero referenced object.
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
			: N{ forward_(others)... }
			, parents_{ static_cast<typename from<Ts...>::base const&>(value) } {
		}

		template<typename...Qs>
		constexpr auto parent() const noexcept {
			constexpr auto direct_index = parent_direct<0u, Qs...>();
			if constexpr (direct_index < num_parents) {
				return parent<direct_index>();
			}
			else {
				constexpr auto recursive_index = parent_recursive<0u, Qs...>();
				if constexpr (recursive_index < num_parents) {
					return parent<recursive_index>()->template parent<Qs...>();
				}
				else {
					return nullptr;
				}
			}
		}

		template<size_t index>
		constexpr auto parent() const noexcept {
			return::std::get<index>(parents_);
		}
		constexpr auto parent() const noexcept {
			return::std::get<0u>(parents_);
		}

		template<typename T>
		constexpr auto all_parent() const noexcept {
			return [this]<size_t...indices>(::std::index_sequence<indices...>) {
				return::std::tuple(parent<indices>()...);
			}(all_parent_impl<0u, T>());
		}

	protected:
		void init() { ::std::apply([](auto...ptr) { ((ptr->init()), ...); }, parents_); }

		// void reset() {
		// 	::std::apply([](auto...ptr) { ((ptr->reset()), ...); }, parents_);
		// }

	private:
		template<size_t index, typename...Qs, size_t...indices>
		static consteval auto all_parent_impl(::std::index_sequence<indices...> = {}) {
			if constexpr (index < num_parents) {
				if constexpr (object_of<tuple_at_t<index, parents>, Qs...>) {
					return all_parent_impl<index + 1u, Qs...>(
						::std::index_sequence<indices..., index>{});
				}
				else {
					return all_parent_impl<index + 1u, Qs...>(
						::std::index_sequence<indices...>{});
				}
			}
			else {
				return::std::index_sequence<indices...>{};
			}
		}

		template<size_t index = 0u, typename...Qs>
		static consteval size_t parent_direct() noexcept {
			if constexpr (index < num_parents) {
				if constexpr (object_of<tuple_at_t<index, parents>, Qs...>) {
					return index;
				}
				else {
					return parent_direct<index + 1, Qs...>();
				}
			}
			else {
				return num_parents;
			}
		}

		template<size_t index = 0u, typename...Qs>
		static consteval size_t parent_recursive() noexcept {
			if constexpr (index < num_parents) {
				if constexpr (!::std::is_null_pointer_v<parent_t<tuple_at_t<index, parents>, Qs...>>) {
					return index;
				}
				else {
					return parent_recursive<index + 1, Qs...>();
				}
			}
			else {
				return num_parents;
			}
		}

	private:
		::std::tuple<Ts*...> parents_;
	};

	template<typename N>
	struct m<lockable_, N> : N {
		using mutex_type = ::std::mutex;

		m(lockable_, auto&&...others) : N{forward_(others)...} {}

		auto& get_lock() const noexcept { return lock_; }

	private:
		mutable mutex_type lock_;
	};

	template<typename T>
	constexpr auto is_lockable = object_of<T, lockable_>;

	template<typename T>
	struct locked : handle_<T> {
		using base = handle_<T>;

		// these for handles.
		locked(T*& value, uint32_t index, lock_duck_ const& lock) 
			: locked{ nullptr, index, value }
		{}
		locked(T*& value, uint32_t index, ::std::mutex& lock) 
			: locked{ this->lock(&lock), index, value } 
		{}
		locked(T& value, uint32_t index, ::std::mutex* lock)
			: locked{ this->lock(lock), index, value }
		{}

		// these for handle.
		locked(T& value, lock_duck_ const& lock)
			: base{ value }
			, plock_{ nullptr }
		{}
		locked(T& value, ::std::mutex& lock)
			: locked{this->lock(&lock), value}
		{}
		locked(T& value, ::std::mutex* lock)
			: locked{ this->lock(&lock), value }
		{}

		locked(locked const&) = delete;
		locked& operator=(locked const&) = delete;

		~locked() { if (plock_) { plock_->unlock(); } }

	private:
		::std::mutex* lock(::std::mutex* ptr) {
			if (ptr) {
				ptr->lock();
			}
			return ptr;
		}

		locked(::std::mutex* lock, T& value) 
			: base{value}
			, plock_{lock} 
		{}
		locked(::std::mutex* lock, uint32_t index, T*& value) 
			: base{value[index]}
			, plock_{lock}
		{}

		::std::mutex* plock_;
	};

	using namespace extensions;

	template<typename N>
	struct m<allocate_from, N> : N {
	private:
		using allocator_type = box<vptr::handle_allocator>;

	public:
		constexpr m(allocate_from const& from, auto&&...others)
			: N{forward_(others)...}
			, allocator_{ from.allocator }
			, callbacks_{
				.pUserData = &allocator_,
				.pfnAllocation = [](void* p, size_t size, size_t alignment, VkSystemAllocationScope) -> void* {
					return static_cast<allocator_type*>(p)->allocate(size, alignment);
				},
				.pfnReallocation = allocator_.reallocable() ? [](void* p, void* ori, size_t size, size_t alignment, VkSystemAllocationScope) -> void* {
					return static_cast<allocator_type*>(p)->reallocate(ori, size, alignment);
				} : nullptr,
				.pfnFree = [](void* p, void* ptr) -> void {
					static_cast<allocator_type*>(p)->free(ptr);
				},
				.pfnInternalAllocation = nullptr,
				.pfnInternalFree = nullptr,
			} 
		{}

		void relocate() noexcept { callbacks_.pUserData = &allocator_; }

		constexpr VK_ VkAllocationCallbacks const* allocator() const { return &callbacks_; }

	protected:
		constexpr auto& allocator_impl() noexcept { return allocator_; }
		

	private:
		allocator_type allocator_;
		VK_ VkAllocationCallbacks callbacks_;
	};

}

