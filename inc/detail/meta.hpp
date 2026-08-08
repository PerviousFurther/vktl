#pragma once

VKTL_EXPORT_ namespace vktl::detail {
	template<typename T>
	using vector = ::std::vector<T>;
	template<typename T>
	using list = ::std::list<T>;

	template<typename Fn>
	struct defer {
		~defer() { fn_(); }
		Fn fn_;
	};

	template<typename T, size_t size>
	using array = ::std::array<T, size>;
	template<typename K, typename T>
	using umap = ::std::unordered_map<K, T>;

	template<typename T, typename U>
	concept similiar_to = ::std::same_as<::std::remove_cvref_t<T>, ::std::remove_cvref_t<U>>;

	template<typename T>
	struct tuple_like : ::std::false_type {};
	template<typename...Ts>
	struct tuple_like<::std::tuple<Ts...>> : ::std::true_type {};

	template<typename T>
	constexpr bool tuple_like_v = tuple_like<::std::remove_cvref_t<T>>::value;

	template<typename T>
	struct tuple_size {};
	template<template<typename...>typename Tp, typename...Ts>
		requires(tuple_like<Tp<Ts...>>::value)
	struct tuple_size<Tp<Ts...>> { static constexpr auto value{ sizeof...(Ts) }; };

	template<typename T>
		requires(tuple_like<T>::value)
	constexpr auto tuple_size_v = tuple_size<::std::remove_cvref_t<T>>::value;

	template<size_t index, typename T>
	struct tuple_at {};
	template<size_t index, template<typename...>typename Tp, typename...Ts>
		requires(tuple_like_v<Tp<Ts...>>&& index < sizeof...(Ts))
	struct tuple_at<index, Tp<Ts...>> : ::std::tuple_element<index, ::std::tuple<Ts...>> {};

	template<size_t index, typename T>
	using tuple_at_t = typename tuple_at<index, T>::type;

	template <typename T, typename...Args>
	consteval::std::size_t get_index() noexcept {
		constexpr bool matches[] = { ::std::same_as<T, Args>... };
		for (::std::size_t i = 0; i < sizeof...(Args); ++i) {
			if (matches[i]) { return i; }
		}
		return static_cast<::std::size_t>(-1);
	}

	template <typename T, typename Tuple>
	struct find_if_same;
	template <typename T, template<typename...>typename Tp, typename... Args>
		requires(tuple_like<Tp<Args...>>::value)
	struct find_if_same<T, Tp<Args...>>
		: ::std::integral_constant<::std::size_t, get_index<T, Args...>()> {
	};
	template <typename T, typename Tuple>
	constexpr size_t find_if_same_v = find_if_same<T, Tuple>::value;

	template<typename T>
	constexpr T align_up(T value, T alignment) noexcept {
		return (value + alignment - 1) & ~(alignment - 1);
	}
	template<typename T>
	constexpr T align_down(T value, T alignment) noexcept {
		return value & ~(alignment - 1);
	}
	template<typename T>
	constexpr T align_up_generic(T value, T alignment) noexcept {
		return ((value + alignment - 1) / alignment) * alignment;
	}
	template<typename T>
	constexpr T align_down_generic(T value, T alignment) noexcept {
		return (value / alignment) * alignment;
	}

	template<typename T, typename Spec, typename = void>
	struct is_specialization_of : ::std::false_type {};
	template<typename T, typename C>
	struct is_specialization_of<T, C, ::std::void_t<typename C::template apply<T>>> : C::template apply<T> {};

	template<template<typename...>typename Tp>
	struct tuple_specialization {
		template<typename T>
		struct apply : ::std::false_type {};
		template<typename...Ts>
		struct apply<Tp<Ts...>> : ::std::true_type {};
	};

	template<typename T, template<typename...>typename Tp>
	concept tuple_specialization_of = is_specialization_of<::std::remove_cvref_t<T>, tuple_specialization<Tp>>::value;

	template<typename...Ts>
	struct ts {
		template<template<typename...>typename Apply>
		using apply = Apply<Ts...>;
	};
	template<typename...Ts>
	struct tuple_like<ts<Ts...>> : ::std::true_type {};

	template<template<typename...>typename Tp>
	struct quote {
		template<typename...Ts>
		using apply = Tp<Ts...>;
	};

	template<template<typename...>typename Tp, typename...Ts>
	struct bind_back {
		template<typename...Us>
		using apply = Tp<Us..., Ts...>;
	};

	template<template<typename...>typename Tp, typename...Ts>
	struct bind_front {
		template<typename...Us>
		using apply = Tp<Ts..., Us...>;
	};

}
