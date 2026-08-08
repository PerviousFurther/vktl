#pragma once


VKTL_EXPORT_ namespace vktl::detail {

	struct error {
		uint32_t code;
		const char* msg;
	};

	struct popup {
		template<typename T>
		struct closure {
			friend auto operator|(VK_ VkResult result, closure self) noexcept(false) {
				if (!self.value_->handle_error(result)) VKTL_UNLIKELY {
					throw error{ uint32_t(result), self.error_ };
				}
			}

			T* value_;
			const char* error_;
		};

		template<typename T>
		constexpr auto operator()(T& handler) {
			return closure<T>{ &handler };
		}

		VKTL_MAYBE_UNUSED
			friend void operator|(VK_ VkResult result, popup self) noexcept(false) {
			if (result != VK_ VK_SUCCESS) VKTL_UNLIKELY {
				throw error{ uint32_t(result), self.error_ };
			}
		}

		const char* error_;
	};

	// node.

	template<typename First, typename Next>
	struct m;

	// consume.

	template<typename State, typename Types>
	struct c;

	template<size_t index>
	struct i : ::std::integral_constant<size_t, index> {};

	template<typename T>
	struct express;

	template<typename T>
	concept can_express = ::std::default_initializable<express<T>>;

	template<typename State, typename Types>
	struct c;

	template<size_t index, typename Types>
		requires(index < tuple_size_v<Types>)
	struct c<i<index>, Types> : m<tuple_at_t<index, Types>, c<i<index + 1u>, Types>> {
		using base = m<tuple_at_t<index, Types>, c<i<index + 1u>, Types>>;

		template<typename T>
		friend struct express;

		template<typename...Args>
		constexpr c(Args&&...infos)
			requires(::std::constructible_from<base, Args&&...>)
		: base{ forward_(infos)... }
		{
		}

		template<typename Ignored, typename...Args>
		constexpr c(Ignored&& first, Args&&...infos)
			requires(!::std::constructible_from<base, Ignored&&, Args&&...>&& ::std::default_initializable<express<::std::remove_cvref_t<Ignored>>>)
		: c(forward_(infos)...) {
			express<::std::remove_cvref_t<Ignored>>::invoke(forward_(first), static_cast<base&>(*this));
		}
	};
	template<size_t index, typename Types>
		requires(index >= tuple_size_v<Types>)
	struct c<i<index>, Types> : Types {
		using base = Types;
		constexpr c(auto&&...infos)
			: base{ forward_(infos)... }
		{
		}
	};

	template<typename T>
	struct object;

	template<typename T, typename C>
	struct is_queryable : ::std::is_same<::std::remove_cvref_t<T>, ::std::remove_cvref_t<C>> {};

	struct empty {};
	struct always_one { operator uint32_t() const noexcept { return 1u; } };

	inline /* constinit */ struct lock_duck_ {
		static constexpr void lock() noexcept {}
		static constexpr bool try_lock() noexcept { return true; }
		static constexpr void unlock() noexcept {}
	} lock_duck {};

	inline constexpr struct call_duck_ {
		void operator()(auto&&...) {}
	} call_duck {};

	template<typename C, typename...Ts>
	struct b : C {
		constexpr b() = default;
		constexpr b(auto&&...) {}

		static constexpr always_one add_ref() noexcept { return {}; }
		static constexpr always_one release() noexcept { return {}; }

		template<typename>
		static constexpr auto parent() noexcept { return nullptr; }
		template<typename>
		static constexpr auto all_parent() noexcept { return::std::tuple(); }
		static constexpr auto parent() noexcept { return nullptr; }

		template<typename T>
		static constexpr bool query() noexcept { return ((is_queryable<Ts, T>::value) || ...); }

		static constexpr void init() noexcept {}
		static constexpr void reset() noexcept {}

		static constexpr lock_duck_& get_lock() noexcept { return lock_duck; }

	protected:
		using self = c<i<1u>, b<C, Ts...>>;

		static constexpr void relocate() noexcept {}
		static constexpr auto allocator() noexcept { return nullptr; }

		constexpr self& as_self() noexcept { return *as_this(); }
		constexpr self const& as_self() const noexcept { return *as_this(); }
		constexpr self* as_this() noexcept { return static_cast<self*>(this); }
		constexpr self const* as_this() const noexcept { return static_cast<self const*>(this); }
	};
	template<typename C, typename...Ts>
	struct tuple_like<b<C, Ts...>> : ::std::true_type {};

	template<typename...Args>
	struct ob_ : ob_<b<empty>, Args...> {
		using next = ob_<b<empty>, Args...>;

		static constexpr auto make_tuple(auto&&...values) {
			return next::make_tuple(::std::tuple(), ::std::tuple(), forward_(values)...);
		}
	};

	template<typename...Cs, typename C, typename First, typename...Args>
		requires(can_express<First>)
	struct ob_<b<C, Cs...>, First, Args...> : ob_<b<C, First, Cs...>, Args...> {
		using next = ob_<b<C, First, Cs...>, Args...>;

		static constexpr auto make_tuple(auto first, auto second, auto&&, auto&&...values) {
			return next::make_tuple(::std::move(first), ::std::move(second), forward_(values)...);
		}
	};
	template<typename...Cs, typename First, typename...Args>
		requires(!can_express<First>)
	struct ob_<b<Cs...>, First, Args...> : ob_<b<Cs...>, Args...> {
		using next = ob_<b<Cs...>, Args...>;

		static constexpr auto make_tuple(auto first, auto second, auto&& fir, auto&&...values) {
			return next::make_tuple(::std::move(first), 
				::std::tuple_cat(::std::forward_as_tuple(forward_(fir)), ::std::move(second)), 
				forward_(values)...);
		}
	};

	template<typename...Ts>
	struct from : ::std::tuple<Ts*...> {
		using base = ::std::tuple<Ts*...>;
		constexpr from(Ts&...value)
			: base{ &value... }
		{
		}

		constexpr from(from const&) = default;
		constexpr from& operator=(from const&) = default;

		constexpr from(from&&) noexcept = default;
		constexpr from& operator=(from&&) noexcept = default;
	};
	template<typename ...Ts>
	from(Ts&...) -> from<Ts...>;

	template<typename C>
	struct use_base {};

	template<typename T>
	struct express<use_base<T>> {
		static constexpr void invoke(auto&&...) noexcept {}
	};

	// already have base.
	template<typename C, typename...Cs, typename T, typename...Us, typename...Args>
	struct ob_<b<use_base<C>, from<Us...>, Cs...>, object<T>, Args...> : ob_<b<use_base<C>, from<Us..., object<T>>, Cs...>, Args...> {
		using next = ob_<b<use_base<C>, from<Us..., object<T>>, Cs...>, Args...>;

		static constexpr auto make_tuple(auto first, auto second, object<T>& fir, auto&&...args) {
			return next::make_tuple(::std::tuple_cat(::std::move(first), ::std::forward_as_tuple(fir)), ::std::move(second), forward_(args)...);
		}
		static constexpr auto make_tuple(auto, auto, object<T>&&, auto&&...) = delete; // not allow right value reference on object.
	};
	template<typename C, typename...Cs, typename T, typename...Args>
	struct ob_<b<use_base<C>, Cs...>, object<T>, Args...> : ob_<b<use_base<C>, from<object<T>>, Cs...>, Args...> {
		using next = ob_<b<use_base<C>, from<object<T>>, Cs...>, Args...>;

		static constexpr auto make_tuple(auto, auto second, object<T>& fir, auto&&...args) {
			return next::make_tuple(::std::forward_as_tuple(fir), ::std::move(second), forward_(args)...);
		}
		static constexpr auto make_tuple(auto, auto, object<T>&&, auto&&...) = delete; // not allow right value reference on object.
	};

	template<typename C, typename...Cs, typename T, typename...Args>
	struct ob_<b<use_base<C>, Cs...>, use_base<T>, Args...> : ob_<b<use_base<T>, Cs...>, Args...> {
		using next = ob_<b<use_base<T>, Cs...>, Args...>;
		static constexpr auto make_tuple(auto first, auto second, use_base<T>, auto&&...args) {
			return next::make_tuple(::std::move(first), ::std::move(second), forward_(args)...);
		}
	};

	template<typename...Args>
	struct s_;

	template<typename T>
	struct object : T::type {
		using object_tag = void;
		using base = typename T::type;

		template<typename...Args>
		constexpr object(Args&&...args)
			: object{ p_{}, T::make_tuple(static_cast<Args&&>(args)...) }
		{
			base::relocate();
		}

		constexpr object(object const& object) requires(::std::copy_constructible<base>)
			: base{ static_cast<T const&>(object) } {
			base::relocate();
		}
		constexpr object& operator=(object const& object) requires(::std::is_copy_assignable_v<base>) {
			static_cast<T>(*this) = static_cast<T const&>(object);
			base::relocate();
		}

		constexpr object(object&& object) requires(::std::move_constructible<base>)
			: base{ static_cast<T&&>(object) } {
			base::relocate();
		}
		constexpr object& operator=(object&& object) requires(::std::is_move_assignable_v<base>) {
			static_cast<T>(*this) = static_cast<T&&>(object);
			base::relocate();
		}

		void init() {
			T::relocate();
			T::init();
		}

		void reset() {
			T::reset();
		}

		template<typename O>
			requires(::std::is_trivial_v<O>)
		constexpr auto operator|(O other)& {
			return s_{ *this, other };
		}
		template<typename O>
			requires(::std::is_trivial_v<O>)
		friend constexpr auto operator|(O other, object& self) {
			return s_{ other, self };
		}

	private:
		struct p_ {}; // stop some shit language directly match private constructor and raise idiot error.
		template<typename Tuple, size_t...ids>
			requires(tuple_like_v<Tuple>)
		constexpr object(p_, ::std::index_sequence<ids...>, Tuple&& tuple)
			: base{ get<ids>(static_cast<Tuple&&>(tuple))... } {
		}
		template<typename Tuple>
			requires(tuple_like_v<Tuple>)
		constexpr object(p_ place_holder, Tuple&& tuple)
			: object{ place_holder, ::std::make_index_sequence<tuple_size_v<Tuple>>(), static_cast<Tuple&&>(tuple), }
		{
		}
	};
	template<typename...Args>
	object(Args&&...) -> object<ob_<::std::remove_cvref_t<Args>...>>;

	template<typename...Cs>
	struct ob_<b<Cs...>> {
		using type = c<i<1u>, b<Cs...>>;
		static constexpr auto make_tuple(auto first, auto second) {
			return::std::apply([&](auto&...ref) {
				return::std::apply([&](auto&&...value) {
					return::std::forward_as_tuple(forward_(value)..., from{ ref... });
					}, ::std::move(second));
				}, ::std::move(first));
		}
	};

	// the object contain a lock help operation thread safe. 
	inline constexpr struct lockable_ {} lockable;
	// cross thread shared use reference counter (not thread safe).
	inline constexpr struct shared_ {} shared;
	// cross thread shared use atomic reference counter.
	inline constexpr struct cross_thread_shared_ {} cross_thread_shared;

	template<typename T>
	struct is_host : ::std::false_type {};
	// template<typename T, typename E>
	// struct is_extend : ::std::false_type {};

	template<typename...Args>
	struct s_ : ::std::tuple<Args...> {
		using base = ::std::tuple<Args...>;
		using base::base;

		template<typename O>
			requires(::std::is_trivial_v<O>)
		constexpr auto operator|(O value) && noexcept { return s_{ ::std::tuple_cat(*this, ::std::tuple(::std::move(value))) }; }
		template<typename...Os>
		constexpr auto operator|(s_<Os...> value) && noexcept { return s_{ ::std::tuple_cat(*this, ::std::tuple(::std::move(value))) }; }
		constexpr operator object<ob_<Args...>>() && noexcept { return::std::apply([](auto&&...vals) { return object{ forward_(vals)... }; }, ::std::move(*this)); }
	};
	template<typename T, typename O>
		requires(::std::is_trivial_v<::std::remove_cvref_t<O>>)
	s_(object<T>&, O&&)->s_<object<T>&, ::std::remove_cvref_t<O>>;
	template<typename T, typename O>
		requires(::std::is_trivial_v<::std::remove_cvref_t<O>>)
	s_(O&&, object<T>&)->s_<::std::remove_cvref_t<O>, object<T>&>;
	template<typename T, typename O>
	s_(object<O>&, object<T>&) -> s_<object<O>&, object<T>&>;

	template<typename...Ts>
	struct tuple_like<s_<Ts...>> : ::std::true_type {};

	template<typename C, typename...Cs, typename...Ts, typename...Args>
	struct ob_<b<C, Cs...>, s_<Ts...>, Args...> : ob_<b<C, Cs...>, Ts..., Args...> {
		using next = ob_<b<C, Cs...>, Ts..., Args...>;

		static constexpr auto make_tuple(auto first, auto second, s_<Ts...>&& ia, auto&&...args) {
			return::std::apply(
				[&](auto&&...value) {
					return next::make_tuple(::std::move(first), ::std::move(second), forward_(value)..., forward_(args)...);
				}, static_cast<::std::tuple<Ts...>&&>(ia));
		}
		static constexpr auto make_tuple(auto, auto, s_<Ts...>&, auto&&...) = delete; // cannot express info adatper with left value reference.
	};

	template<typename T, typename O>
		requires(is_host<T>::value)
	constexpr auto operator|(T value, O other)
		noexcept {
		return s_<T, O>{::std::move(value), ::std::move(other)};
	}

	template<typename T, typename Q>
	concept object_of = T::template query<Q>();

	template<typename T>
	struct obtain_ {};

	template<size_t index, typename T>
	struct obtain_<c<i<index>, T>> { static constexpr auto value = index - 2u; };

	template<typename T, typename Tuple>
	constexpr auto&& obtain(Tuple&& tuples) requires(tuple_like_v<Tuple>) {
		return::std::get<obtain_<T>::value>(static_cast<Tuple&&>(tuples));
	}


}