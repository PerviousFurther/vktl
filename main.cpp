

#if !defined(IDKNW_H__)

#if defined(IDKNW_INLINE_INCLUDE)
#	define IDKNW_INLINE_VAR inline
#	define IDKNW_ENABLE_INLINE 1
#else 
#	define IDKNW_INLINE_VAR
#	define IDKNW_ENABLE_INLINE 0
#endif // inline include.

#if defined(IDKNW_ENABLE_INTERFACE)
#	define IDKNW_ENABLED_TYPE_EARSE 1
#else
#	define IDKNW_ENABLED_TYPE_EARSE 0
#endif

#if defined(IDKNW_ENABLE_DEBUG)
#	define IDKNW_ENABLED_DEBUG 1
#else
#	define IDKNW_ENABLED_DEBUG 0
#endif

#include <cstdint>
#include <span>
namespace unk
{
	using uint8 = ::std::uint8_t;
	using uint16 = ::std::uint16_t;
	using uint32 = ::std::uint32_t;
	using uint64 = ::std::uint64_t;
	using size = uint64;

	struct error_t
	{
		uint32 code;
	};

	struct version
	{
		uint8 major;
		uint8 minor;
		uint8 patch;
	};
	struct identifier
	{
		uint8 major;
		uint8 minor;
		uint8 patch;
		uint32 id;
		operator version const& () const noexcept
		{
			return reinterpret_cast<version const&>(*this);
		}
	};
	extern const version engine_version;
	extern const identifier vulkan_1_0;
	extern const identifier vulkan_1_1;
	extern const identifier vulkan_1_2;
	extern const identifier vulkan_1_3;
	extern const identifier vulkan_1_4;

	// Common fs for objects.
	namespace feature
	{
		// Notify allocator.
		// This is for every object.
		// To indicate how vulkan allocate memory 
		// for handle's.
		struct allocation
		{

		};
		// device group extension.
		struct device_group
		{

		};
	}

	// instance fs. 
	namespace feature
	{
		// enable debug utils and something.
		struct debug
		{
			static const identifier uid;
		};
		// enable validation layers.
		struct validation
		{
			static const identifier uid;
		};
		// enable detail_query fs.
		struct detail_query
		{
			static const identifier uid;
		};
	} // namespace fs

	// instance object info.
	struct instance
	{
		identifier api_ver;
	};
	namespace interfaces
	{
		struct instance
		{
		#if IDKNW_ENABLED_TYPE_EARSE
			static instance* create(unk::instance const&, void const* const*, identifier const*);
		#endif
		};
	}
	
	namespace feature
	{
		// physical device properties.
		struct properties 
		{
		
		};
		// physical memory properties.
		struct memory 
		{

		};
		// physical device fs.
		struct features 
		{

		};
		struct queue_family_props 
		{

		};
	}
	struct physical_device
	{
	
	};

	namespace feature
	{
		// for `queue` and `*resources*`.
		struct sparse_binding 
		{

		};
		struct graphics 
		{
			uint16 queue_count = 1u;
		};
		struct compute 
		{
			uint16 queue_count = 1u;
		};
		struct transfer
		{
			uint16 queue_count = 1u;
		};
		struct priorities 
		{
			uint32 count = 0u;
			float value[32]{};
		};
		struct select_queue_family
		{
			uint16 index = 0u;
		};
	}
	struct device 
	{
		
	};

	namespace feature 
	{

	}
	struct queue 
	{

	};
} // namespace unk


// TEMPORARY:

#define ENABLE_IDKWN_IMPL

#if defined(ENABLE_IDKWN_IMPL)

// WARNING: 
//  you need to remember ISO c++'s ODR rule when define these kind of stuff.
//  That means you might need to compile the implmentation yourself when setting them.
#if !defined(IDKNW_DEFAULT_DEVICE_QUEUE_COUNT)
#define IDKNW_MAX_DEVICE_QUEUE_COUNT 8
#endif

#include <vulkan/vulkan.h>
#include <new>
#include <concepts>
#include <tuple>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace unk
{
	// Alias.

	template<typename...Ts>
	using tuple = ::std::tuple<Ts...>;

	inline constexpr struct null_t
	{
		using const_iterator_type = void;
		using iterator_type = void;
		using value_type = void;
		using type = void;

		static constexpr auto size() noexcept { return 0; }
		static constexpr auto data() noexcept { return nullptr; }
		static constexpr auto begin() noexcept { return nullptr; }
		static constexpr auto end() noexcept { return nullptr; }
		void operator&() {}
	} null;

	struct empty_base {};

	template<template<typename...>typename T>
	struct template_type 
	{
		template<typename...Ts>
		using apply = T<Ts...>;
	};
	// full struct template type
	template<template<typename...>typename T>
	inline constexpr template_type<T> stmp{};

	template<size index, typename...Ts>
	using type_at = ::std::tuple_element_t<index, tuple<Ts...>>;
	template<size index, typename T, typename...Ts>
	using type_at_or 
		= typename::std::conditional
		< (sizeof...(Ts) > index)
		, ::std::tuple_element<index, tuple<Ts...>>
		, ::std::type_identity<T>>::type::type;

	template<typename T, ::std::size_t size>
	using array = ::std::array<T, size>;
	using str = ::std::string;
	using strvw = ::std::string_view;

	template<size...ids>
	using indices = std::index_sequence<ids...>;
	template<template<typename>typename Tp, typename...Ts>
	inline constexpr::std::index_sequence_for<Ts...> tuple_ids(Tp<Ts...> const&) { return {}; }

	namespace impl
	{
		template<typename T, typename C>
		struct transparent_hashable_impl : ::std::false_type {};
		template<>
		struct transparent_hashable_impl<str, strvw> : ::std::true_type {};

		template<typename T, typename C>
		concept transparent_hashable
			= (transparent_hashable_impl<::std::remove_cvref_t<T>, ::std::remove_cvref_t<C>>::value
				|| transparent_hashable_impl<::std::remove_cvref_t<C>, ::std::remove_cvref_t<T>>::value);

		template<typename T>
		struct hash
		{
			using hash_type = ::std::hash<T>;
			using is_transparent = void;

			template<typename O>
			constexpr::std::size_t operator()(O&& value)
				noexcept(::std::is_nothrow_invocable_v<::std::hash<::std::remove_cvref_t<O>>, O&&>)
				requires(transparent_hashable<T, O&&> // [URGENT]:TODO: Need more concrete. 
				&& ::std::invocable<::std::hash<::std::remove_cvref_t<O>>, O&&>)
			{
				return::std::hash<::std::remove_cvref_t<O>>()(static_cast<O&&>(value));
			}
		};


	} // namespace unk::impl.

	template<typename K, typename T>
	using umap = ::std::unordered_map<K, T, impl::hash<K>, ::std::equal_to<>>;
	template<typename T>
	using uset = ::std::unordered_set<T, impl::hash<T>, ::std::equal_to<>>;
	template<typename T>
	using vector = ::std::vector<T>;

	template<typename T>
	struct type_type : ::std::type_identity<T> {};
	template<typename T>
	inline constexpr type_type<T> type{};

	//
	// constants.
	//

	IDKNW_INLINE_VAR const version engine_version{ 0, 0, 1 };
	IDKNW_INLINE_VAR const identifier vulkan_1_0{ 0, 0, 0, VK_API_VERSION_1_0 };
	IDKNW_INLINE_VAR const identifier vulkan_1_1{ 0, 0, 0, VK_API_VERSION_1_1 };
	IDKNW_INLINE_VAR const identifier vulkan_1_2{ 0, 0, 0, VK_API_VERSION_1_2 };
	IDKNW_INLINE_VAR const identifier vulkan_1_3{ 0, 0, 0, VK_API_VERSION_1_3 };
	IDKNW_INLINE_VAR const identifier vulkan_1_4{ 0, 0, 0, VK_API_VERSION_1_4 };

	template<typename C>
	struct uid_of 
	{
		static constexpr identifier value{};
	};
	template<identifier const& uid>
	struct type_of
	{
		using type = void;
	};



	namespace feature
	{
		template<typename T>
		struct trait;
	}

	namespace err
	{
		template<typename T>
		struct trait;

		struct propagate_t
		{
			friend void operator|( VkResult result, propagate_t)
			{
				if (result !=  VK_SUCCESS) 
					throw error_t{ static_cast<uint32>(result) };
			}
		};
	}
	inline constexpr err::propagate_t propagate{};

	template<typename...>
	struct type_list {};

	// template<typename T, typename R = type_list<>>
	// struct reverse;
	// template< template<typename...>typename TL
	// 	, template<typename...>typename TR
	// 	, typename T, typename...Features, typename...Rs>
	// struct reverse<TL<T, Features...>, TR<Rs...>> : reverse<TL<Features...>, TR<T, Rs...>> {};
	// template< template<typename...>typename TL
	// 	, template<typename...>typename TR
	// 	, typename...Rs>
	// struct reverse<TL<>, TR<Rs...>> { using type = TR<Rs...>; };

	// 
	// CreateInfoChain on stack helper.
	//
	namespace impl 
	{
		// T is the type need to query.
		// Ts... is the types that T might contains.
		template<typename T, typename...Ts>
		struct is_contains
			: ::std::bool_constant<(... || (::std::is_same_v<T, Ts>))> 
		{};

		template<template<typename...>typename Tp, typename...Ts>
		struct is_not 
		{
			template<typename T>
			struct apply : ::std::bool_constant<!Tp<T, Ts...>::value> {};
		};
		template<template<typename...>typename Tp, typename...Ts>
		struct bind_back
		{
			template<typename T>
			struct apply : ::std::bool_constant<!Tp<T, Ts...>::value> {};
		};
		template<typename T, template<typename>typename Req>
		struct where_satisfy
		{
			static constexpr auto value{Req<T>::value};
		};
		// where require Req<...>::value
		template<typename T, template<typename>typename Req>
		concept where = Req<T>::value;
		template<typename T, typename Req>
		struct when_satisfy 
		{
			static constexpr auto value{Req::template apply<T>::value};
		};
		// when require Req::apply
		template<typename T, typename Req>
		concept when = when_satisfy<T, Req>::value;

		template<template<typename>typename Tp, typename...Ts>
		struct find_where 
		{
			template<::std::size_t, typename...>
			struct impl 
			{
				using type = null_t;
				static constexpr auto value{false};
				static constexpr size_t index_value = sizeof...(Ts);
			};
			template<::std::size_t index, where<Tp> T, typename...Tts>
			struct impl<index, T, Tts...>
			{
				using type = T;
				static constexpr auto value{true};
				static constexpr auto index_value{index};
			};
			template<::std::size_t index, when<is_not<Tp>> T, typename...Tts>
			struct impl<index, T, Tts...> : impl<index + 1, Tts...>
			{};

			using result = impl<0, Ts...>;
			static constexpr auto value{ result::value };
			static constexpr auto index_value{ result::index_value };
			using type = typename result::type;
		};

		// more .
		template<typename T, typename...Ts>
		concept range_of 
			= bool(::std::ranges::range<T>)
			&& ((::std::same_as<Ts, ::std::ranges::range_value_t<T>>) || ...);

		// T is feature type.
		template<typename T>
		struct info_trait
		{
			template<typename U, typename C>
			static constexpr auto get(U&& src, C& bs)
			{
				if constexpr (requires{ { feature::trait<T>::create_info(src, bs) } -> when<is_not<::std::is_same, void>>; })
					return feature::trait<T>::create_info(src, bs);
				else if constexpr (requires{ typename T::type; } || ::std::same_as<::std::remove_cvref_t<U>, ::std::remove_cvref_t<C>>)
					return static_cast<U&&>(src);
				else
					return null;
			}

			template<typename U, typename P>
			static constexpr void set(U& st, P* pnext) noexcept
			{
				if constexpr (requires { st.set(pnext); })
					return st.set(pnext);
				else if constexpr (requires { st.pNext; })
					st.pNext = pnext;
				else if constexpr (requires { *st = pnext; })
					*st = pnext;
				else
					static_assert(::std::same_as<U, null_t>);
			}

			template<typename U, typename P>
			static constexpr void set(U& st, P& pnext) // might add something else.
			{
				if constexpr (requires { pnext.sType; })
					info_trait::set(st, &pnext);
				else if constexpr (::std::ranges::range<U> && ::std::ranges::range<P>)
					for (auto i{0u}; i < ::std::ranges::size(st); i++)
						info_trait::set(st[i], pnext[i]);
			}
		};

		// Ts...: should use *desc_type*.
		template<typename...Ts>
		struct infos 
		{ 
			using base = void;
			void set(auto&) {}

			infos() = default;
			// We designed infos as a disposable info chain.
			// So any movement or redirection is meaningless.
			infos(infos&&) = delete; 
			infos& operator=(infos&&) = delete;
		};
		template<typename T, typename...Ts>
		struct infos<T, Ts...> : infos<Ts...>
		{
			static constexpr bool is_base = sizeof...(Ts) == 0; // TODO: too weak.
			using trait = info_trait<T>;
			using next = infos<Ts...>;
			using base = decltype([]() {
				if constexpr (is_base)
					return type<infos>;
				else
					return type<typename next::base>;
				}())::type;
			using type = decltype([]() {
				if constexpr (is_base)
					return type<decltype(trait::get(::std::declval<T&&>(), ::std::declval<T&>()))>;
				else
					return type<decltype(trait::get(::std::declval<T&&>(), ::std::declval<typename next::base&>()))>;
				}())::type;

			template<typename...Rs>
			constexpr infos(Rs&&...rs) 
				requires(::std::constructible_from<next, Rs&&...> 
					&&::std::constructible_from<type, T&&>
					&&::std::default_initializable<T>)
				: next{rs...}
				, info{T{}}
			{
				next::set(this->info);
			}

			template<typename U, typename...Rs>
			constexpr infos(U&& f, Rs&&...rs)
				: next{ static_cast<Rs&&>(rs)... }
				, info{ trait::get(static_cast<U&&>(f), base::get()) }
			{
				next::set(this->info);
			}

			constexpr auto& get() noexcept 
				requires(is_base)
			{
				return info;
			}

			template<typename F>
			constexpr void set(F const& fronter) noexcept
			{
				info_trait<T>::set(info, fronter);
			}

			template<::std::size_t index = 0>
			constexpr void detach() noexcept
			{
				if constexpr (index == 0) {
					// should not detach when null_t.
					static_assert(!::std::same_as<null_t, type>); 
					this->set(nullptr);
				} else 
					next::template detach<index - 1>();
			}

			// Partial:
			// for ranges version.

			template<typename U, typename...Rs>
			constexpr void resize(size size, U&& cinfo, Rs&&...rest)
				requires(::std::ranges::range<type>)
			{
				next::resize(size, static_cast<Rs&&>(rest)...);
				info.resize(size);
				trait::get(static_cast<U&&>(cinfo), info); // design use for or .size().
				next::set(info);
			}

			template<::std::size_t index>
			constexpr auto const &as() const noexcept 
			{
				if constexpr (index == 0)
					return *this;
				else {
					static_assert(sizeof...(Ts) > 0); // Out of range.
					return next::template as<index - 1>();
				}
			}

			constexpr auto data() const noexcept 
				requires(is_base && ::std::ranges::range<type>)
			{ return::std::ranges::data(info); }
			constexpr auto begin() const noexcept 
				requires(is_base && ::std::ranges::range<type>) 
			{ return::std::ranges::begin(info); }
			constexpr auto end() const noexcept 
				requires(is_base&& ::std::ranges::range<type>)
			{ return::std::ranges::end(info); }

			// end.

			constexpr operator type const& () const noexcept
			{
				return info;
			}
			constexpr operator type& () noexcept
			{
				return info;
			}
			constexpr operator type const* () const noexcept 
				requires(is_base)
			{
				return &info;
			}
			constexpr operator type* () noexcept
				requires(is_base)
			{
				return &info;
			}

			type info;
		};
		template<typename...Ts>
		infos(Ts&&...) -> infos<::std::remove_cvref_t<Ts>...>;
	} // namesapce unk::fs

	//
	// FeatureInheritance helper 
	// FeatureChain helper.
	//

	// To register a fs, specialize `consume_feature` 
	// and place the fs at the front 
	// of the template parameter pack.
	namespace feature
	{
		template<typename Self, template<typename>typename...Rs>
		struct compose
		{
			using self = Self;
		};
		template<typename Self, template<typename>typename T, template<typename>typename...Ts>
		struct compose<Self, T, Ts...> : T<compose<Self, Ts...>>
			// if error is apply is not a template of some fs.
			// add `apply` in trait<your fs>. It is required.
		{
			using self = Self;
			using next = compose<Self, Ts...>; // next node.
			using base = T<compose<Self, Ts...>>; // current node type.
			using base::base;
		};

		// 
		// Feature utils.
		//

		// Redirect.
		
		template<typename T>
		struct rd { using query_redirect = T; };
		template<typename T>
		struct trait<rd<T>>
		{
			template<typename C>
			using apply = T;
		};
		template<typename T>
		inline constexpr rd<T> set_interface{};

		// Child of.
		template<typename P>
		struct co { using query_redirect = P; };
		template<typename P>
		struct trait<co<P>>
		{
			template<typename C>
			struct apply : C
			{
				using C::C;
				using parent_type = P;
			};
		};
		template<typename T>
		inline constexpr co<T> set_parent{};

		template<typename T>
		struct query_trait { using type = T; };
		template<typename T>
			requires(requires { typename T::query_redirect; })
		struct query_trait<T> { using type = typename T::query_redirect; };

		// All object actually can be use this to initialize.
		// See more infomation in document.
		template<typename...Ts>
		struct object : compose<object<Ts...>, trait<Ts>::template apply...>
		{
			using next = compose<object<Ts...>, trait<Ts>::template apply...>;
			using next::next;

			// template<typename T, typename...Rs>
			// constexpr object(T&& desc, Rs&&...rests)
			// 	requires(::std::constructible_from<next, Rs&&..., T&&>)
			// 	: next{static_cast<Rs&&>(rests)..., static_cast<T&&>(desc)}
			// {}

			// Stub for meta programming.
			constexpr auto allocator() noexcept
			{
				if constexpr (requires (next & next) { next.allocator(); })
					return next::allocator();
				else
					return nullptr;
			}

			// for internal use only. DO NOT USE IT IN YOUR CODE.
			static constexpr object& ass(auto* pthis) noexcept
			{
				return *static_cast<object*>(pthis);
			}
			// for internal use only. DO NOT USE IT IN YOUR CODE.
			static constexpr object const& ass(const auto* pthis) noexcept
			{
				return *static_cast<object const*>(pthis);
			}

			template<typename T>
			static constexpr bool query(type_type<T> = type<T>) noexcept
			{
				if constexpr (impl::is_contains<T, Ts...>::value)
					return true;
				// TODO: Transfer query.
				// else if constexpr (requires{})
				// 	return 
				else
					return false;
			}
		};
		template<typename T, typename...Ts>
		object(T&&, Ts&&...) -> object<::std::remove_cvref_t<Ts>..., ::std::remove_cvref_t<T>>;

		template<typename, template<typename>typename...>
		struct compose_insert { using type = empty_base; };
		template<typename Self, template<typename>typename...Ts, template<typename>typename...As>
		struct compose_insert<compose<Self, Ts...>, As...> : type_type<compose<Self, As..., Ts...>> {};
		// Insert template.
		template<typename C, template<typename>typename...Ts>
		using compose_insert_t = typename compose_insert<C, Ts...>::type;
		// Insert type, but require the type have apply.
		template<typename C, typename...Ts>
		using compose_insert_apply_t = typename compose_insert<C, Ts::template apply...>::type;

		template<typename T>
		using parent_t = typename T::parent_type;
	}
	using feature::object;
	
	namespace impl 
	{
		template<typename T, typename...Ts>
		struct check_mutal : ::std::true_type {};
		template<typename F, typename...Ts, typename...Fs>
		struct check_mutal<type_list<Fs...>, F, Ts...>
			: ::std::conditional_t
				<((::std::same_as<F, Ts>) || ...) // if have same.
				, ::std::false_type // evaluate false.
				, check_mutal<type_list<Fs...>, Ts...>> // evaluate next.
		{};

		template<typename...Ts>
		using is_type_mutal = check_mutal<type_list<Ts...>, Ts...>;
	}

	template<typename...Ts>
	struct pack : tuple<Ts...>
	{
		using next = tuple<Ts...>;
		using next::next;

		static_assert(impl::is_type_mutal<Ts...>::value
			, "pack's `Ts...` must different with each other.");

		template<template<typename>typename Tp, size index = 0u, typename...Vs>
		constexpr auto&& get(tuple<Vs...> value) const noexcept
		{
			if constexpr (index != sizeof...(Ts)) 
				if constexpr (Tp<type_at<index, Ts...>>::value)
					return this->template get<Tp, index + 1u>
						(::std::forward_as_tuple(::std::get<Vs>(::std::move(value))..., ::std::get<index>(*this)));
				else 
					return this->template get<Tp, index + 1u>(::std::move(value));
			else 
				return::std::move(value);
		}

		template<template<typename>typename Tp>
		constexpr auto&& get_first() const noexcept 
		{
			constexpr auto index = impl::find_where<Tp, Ts...>::index_value;
			if constexpr (index != sizeof...(Ts))
				return::std::get<index>(static_cast<next const&>(*this));
		}

		template<typename T>
		constexpr operator T const& () const noexcept 
			requires(requires(next const& self) { ::std::get<T>(self); })
		{
			return::std::get<T>(static_cast<next const&>(*this));
		}
	};
	template<typename...Ts>
	pack(Ts&&...) -> pack<::std::remove_cvref_t<Ts>...>;

	template<typename T>
	struct pack_if_single : type_type<pack<T>> {};
	template<typename...Ts>
	struct pack_if_single<pack<Ts...>> : type_type<pack<Ts...>> {};

	// template parameter version.
	// For some objects and info's querying fs.
	//  T: The object or info type.
	//  C: The fs type.
	// TODO: query cpo?
	template<typename T, typename C>
	inline constexpr bool query() noexcept
	{
		if constexpr (requires { T::query(type<C>); })
			return T::query(type<C>);
		else
			return false;
	}
	// function parameter version.
	// For some objects and info's querying fs.
	//  T: The object or info type.
	//  C: The fs type.
	// TODO: query cpo?
	template<typename C, typename T>
	inline constexpr bool query(T const&) noexcept
	{
		return query<T, C>();
	}

	namespace impl
	{
		// merge as array.
		// WARNING:
		//  This function assume that all elements both 
		//  in `left` and `right` were initialized.
		//  No dynamic size checking is performed.
		template<typename K, typename C>
		inline constexpr auto merge_as_arr(K&& left, C&& right)
		{
			if constexpr (::std::is_null_pointer_v<::std::remove_cvref_t<K>>) {
				if constexpr (::std::ranges::range<::std::remove_cvref_t<C>>) {
					array<::std::ranges::range_value_t<C>, ::std::ranges::size(right)> arr;
					::std::ranges::copy(static_cast<C&&>(right), arr.begin());
					return arr;
				} else
					return array<::std::remove_cvref_t<C>, 1>{ static_cast<C&&>(right) };
			} else if constexpr (requires { ::std::ranges::size(::std::declval<C&&>()); }) {
				array<::std::ranges::range_value_t<K>, ::std::ranges::size(left) + ::std::ranges::size(right)> arr;
				auto [_, out] {::std::ranges::copy(static_cast<K&&>(left), arr.begin())};
				::std::ranges::copy(static_cast<C&&>(right), out);
				return arr;
			} else {
				array<::std::ranges::range_value_t<K>, ::std::ranges::size(left) + 1> arr;
				auto [_, out] {::std::ranges::copy(static_cast<K&&>(left), arr.begin())};
				*out = right;
				return arr;
			}
		}
		template<template<typename>typename Trait>
		struct enum_as_arr_t
		{
			template<typename A>
			static constexpr auto enumerate(A&& arr)
			{
				if constexpr (::std::is_null_pointer_v<::std::remove_cvref_t<A>>)
					return null;
				else
					return static_cast<A&&>(arr);
			}
			template<typename A, typename T, typename...Rs>
			static constexpr auto enumerate(A&& arr, T&& info, Rs&&...rst)
			{
				auto&& acc = Trait<::std::remove_cvref_t<T>>::get(static_cast<T&&>(info));
				if constexpr (::std::is_null_pointer_v<::std::remove_cvref_t<decltype(acc)>>)
					return enum_as_arr_t::enumerate(static_cast<A&&>(arr), static_cast<Rs&&>(rst)...);
				else
					return enum_as_arr_t::enumerate(merge_as_arr(static_cast<A&&>(arr), static_cast<decltype(acc)>(acc)), static_cast<Rs&&>(rst)...);
			}
			template<typename...Rs>
			constexpr auto operator()(Rs&&...val) const noexcept
			{
				return enum_as_arr_t::enumerate(nullptr, static_cast<Rs&&>(val)...);
			}
		};
	}
	template<template<typename>typename Trait>
	inline constexpr impl::enum_as_arr_t<Trait> enum_as_arr = {};

	// double count invocation.
	namespace impl
	{
		template< ::std::ranges::contiguous_range R, typename...Args
				, ::std::invocable<Args&&..., uint32*, ::std::ranges::range_value_t<R>*> T>
		inline auto invoke(R& result, T fn, Args&&...args)
		{
			// Count from inside.
			uint32 count;
			if constexpr (::std::is_void_v<::std::invoke_result<T, Args&&..., uint32*, ::std::ranges::range_value_t<R>*>>) {
				fn(static_cast<Args&&>(args)..., &count, nullptr);
				result.resize(count);
				fn(static_cast<Args&&>(args)..., &count, result.data());
			} else {
				 VkResult value{  VK_SUCCESS };
				if ((value = fn(static_cast<Args&&>(args)..., &count, nullptr)) !=  VK_SUCCESS)
					return value;
				result.resize(count);
				if ((value = fn(static_cast<Args&&>(args)..., &count, result.data())) !=  VK_SUCCESS)
					return value;
				return value;
			}
		}
	} // namespace unk::impl

	// 
	// common fs part.
	//

	namespace feature
	{
		template<typename C, typename Props>
		struct is_feature_testable 
			: ::std::bool_constant
				<requires(C const& v, Props const &props, trait<C> c)
					{ c.raise_if_failed(v, props); }> {};
		template<typename C, typename Props>
		struct is_feature_pass
			: ::std::bool_constant
				<requires(Props const &props, trait<C> c)
					{ c.always_pass(props); }> {};
		inline constexpr struct 
		{
			template<typename T, typename F, typename C = decltype((null))>
			constexpr auto operator()(F const& feature, T const& feature_props, C&& context_or_def_value = null) const
			{
				static_assert(is_feature_testable<F, T>::value
					|| is_feature_pass<F, T>::value
					, "Since we recommand feature test, "
					  "add `always_pass` in your `trait` specialization, if you don't wanna check.");
				if constexpr (is_feature_testable<F, T>::value) {
					if constexpr (::std::convertible_to<decltype(trait<F>{}.raise_if_failed(feature, feature_props)), bool> ) {
						if (!trait<F>{}.raise_if_failed(feature, feature_props)) throw ; // TODO: 
					} else if constexpr (::std::ranges::range<decltype(trait<F>{}.raise_if_failed(feature, feature_props))>) {
						for (auto c : trait<F>{}.raise_if_failed(feature, feature_props)) if (!c) throw;
					} else 
						trait<F>{}.raise_if_failed(feature, feature_props);
				} return static_cast<C&&>(context_or_def_value);
			}
		} raise_if_failed{};
		

		// Allocator for vulkan allocating memory for handle's.

		template<>
		struct trait<allocation>
		{
			template<typename C>
			struct apply : C
			{
				template<typename T, typename...Rs>
				constexpr apply(T&& host, allocation const& info, Rs&&...rs)
					: C{static_cast<T&&>(host), static_cast<Rs&&>(rs)...}
					, allocator_{}
				{
				}

				constexpr VkAllocationCallbacks const*
					allocator() const noexcept { return &allocator_; }

			private:
				 VkAllocationCallbacks allocator_;
			};
		};

	}

	//
	// instance part.
	//

	namespace feature
	{
	#define UNK_WRAPPER_NAME_TRAIT(name)																			\
	template<typename T>																							\
	struct name { static constexpr auto get(auto&) { return nullptr; } };											\
	template<typename T>																							\
		requires(requires { trait<T>::name; })																		\
	struct name<T> { static constexpr auto& get(auto&) { return trait<T>::name; } };								\
	template<typename T>																							\
		requires(requires { trait<T>::name(::std::declval<T const&>()); })											\
	struct name<T> { static constexpr auto& get(auto &handle) { return trait<T>::name(handle); } }
		UNK_WRAPPER_NAME_TRAIT(instance_layer_name);
		UNK_WRAPPER_NAME_TRAIT(instance_extension_name);
		UNK_WRAPPER_NAME_TRAIT(device_extension_name);
		UNK_WRAPPER_NAME_TRAIT(device_layer_name);
	#undef UNK_WRAPPER_NAME_TRAIT

		template<>
		struct trait<debug>
		{
			template<typename C>
			using apply = C; // "`feature::validation` is a fs for `instance` only."

			static constexpr auto instance_extension_name{ VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

			static constexpr auto create_info(debug const& info, VkInstanceCreateInfo&) noexcept
			{
				return VkDebugUtilsMessengerCreateInfoEXT
				{
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				};
			}
		};
		template<>
		struct trait<validation>
		{
			template<typename C>
			using apply = C; // "`feature::validation` is a fs for `instance` only."

			static constexpr auto instance_layer_name{ "VK_LAYER_KHRONOS_validation" };
		};

		template<>
		struct trait<detail_query>
		{
			template<typename C>
			using apply = C;

			static constexpr const char* instance_extension_name[]
			{
				VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
			};
		};

		template<>
		struct trait<device_group>
		{
			template<typename C>
			struct apply : C
			{
				using C::C;

				static_assert(::std::is_void_v<C>?0:0
					, "`device_group` is currently unavailable.");
			};
			static constexpr auto device_extension_name{ VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME };
		};

		template<>
		struct trait<instance>
		{
			template<typename C>
			struct apply : C
			{
				using next = C;
				using self = typename C::self;
				using handle_type = VkInstance;

				template<typename...Ts>
				apply(instance const& info, Ts const&...infos)
				{
					impl::infos app_info
					{
						infos..., VkApplicationInfo
						{
							.sType         = VK_STRUCTURE_TYPE_APPLICATION_INFO,
							.pEngineName   = "__VK_@MaoQwe__",
							.engineVersion = VK_MAKE_VERSION(engine_version.major, engine_version.minor, engine_version.patch),
							.apiVersion    = info.api_ver.id,
						}
					};
					auto lay = enum_as_arr<instance_layer_name>(infos...);
					auto ext = enum_as_arr<instance_extension_name>(infos...);
					impl::infos cinfo
					{
						infos..., VkInstanceCreateInfo
						{
							.sType				     = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
							.pApplicationInfo        = app_info,
							.enabledLayerCount       = uint32(::std::size(lay)),
							.ppEnabledLayerNames     = ::std::data(lay),
							.enabledExtensionCount   = uint32(::std::size(ext)),
							.ppEnabledExtensionNames = ::std::data(ext),
						}
					};
					vkCreateInstance(cinfo, self::ass(this).allocator(), &handle_) | propagate;
					impl::invoke(phys_, vkEnumeratePhysicalDevices, handle_) | propagate;
				}

				apply(apply&&) noexcept = delete;
				apply& operator=(apply&&) noexcept = delete;

				~apply()
				{
					vkDestroyInstance(handle_, self::ass(this).allocator());
				}

				// template parameter version.
				// info:
				//  get attribute from physical device.
				//  remember, index must be valid.
				template<typename IPhyDv = empty_base, typename...Qs>
				auto get_phydv(uint32 index, Qs const&...infos) noexcept
				{
					return object<Qs..., physical_device, co<self>, rd<IPhyDv>>{phys_[index], infos...};
				}
				auto num_phydv() noexcept
				{
					return uint32(phys_.size());
				}
				constexpr operator handle_type() const noexcept
				{
					return handle_;
				}
			private:
				handle_type handle_;
				vector<VkPhysicalDevice> phys_;
			};
		};
	} // end instance.

	//
	// Physical device part.
	//
	namespace feature
	{
		using impl::infos;

		struct empty_get 
		{
			template<typename C>
			struct apply : C
			{
			protected:
				static constexpr auto get_next(size = 0) noexcept { return nullptr; }
			};
		};

		// `Ts...` must be the `type` with `apply`.
		// `C` must be `type_list<...>`, elements in `type_list` use for extra paramters and can be reference.
		template<typename B, typename C = type_list<>, typename...Ts>
		struct apply_pack;
		template<typename B, typename...Cs, typename...Ts>
		struct apply_pack<B, type_list<Cs...>, Ts...> : compose_insert_apply_t<B, Ts...>
		{
			using C = compose_insert_apply_t<B, Ts...>;

			template<typename K, typename F, typename...Rs>
			apply_pack(K&& host, Cs...value, F&& first, Rs&&...rest)
				requires(!requires{ ::std::get<0>(static_cast<F&&>(first)); })
				: C{ static_cast<K&&>(host), static_cast<Cs>(value)...
					, static_cast<F&&>(first), static_cast<Rs&&>(rest)... }
			{
			}

			template<typename K, typename F, typename...Rs>
			apply_pack(K&& host, Cs...value, F&& first, Rs&&...rest)
				requires(requires{ ::std::get<0>(static_cast<F&&>(first)); })
				: apply_pack{ tuple_ids(first), static_cast<K&&>(host)
					, static_cast<Cs>(value)..., static_cast<F&&>(first), static_cast<Rs&&>(rest)... }
			{
			}

			template<typename K, typename F, typename...Rs, size...ids>
			apply_pack(indices<ids...>, K&& host, Cs...value, F&& first, Rs&&...rest)
				: C{ static_cast<K&&>(host), static_cast<Cs>(value)...
					, ::std::get<ids>(static_cast<F&&>(first))..., static_cast<Rs&&>(rest)...}
			{
			}
		};


		// for pure getter?
		template<typename Host, typename...Ts>
			requires (requires{ type<typename trait<Host>::phydv_attr>, ((type<typename trait<Ts>::phydv_attr>), ...); })
		struct trait<pack<Host, Ts...>>
		{
			using phydv_attr = void;
			template<typename B>
			using apply = apply_pack<B, type_list<>, trait<Host>, trait<Ts>..., empty_get>;
		};

		// physical device properties.
		template<>
		struct trait<properties> 
		{
			using phydv_attr = void;

			template<typename C>
			struct apply : C
			{
				using type 
					= ::std::conditional_t
					< C::enable_2
					, VkPhysicalDeviceProperties2KHR
					, VkPhysicalDeviceProperties>;

				apply(auto handle, properties const&, auto&...rs)
					: C{handle, rs...}
				{
					if constexpr (C::enable_2) {
						properites = {
							VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
							C::get_next()
						}; vkGetPhysicalDeviceProperties2KHR(C::handle, &properites);
					} else 
						vkGetPhysicalDeviceProperties(C::handle, &properites);
				}

				type properites;
			};
		};

		template<>
		struct trait<memory>
		{
			using phydv_attr = void;

			template<typename C>
			struct apply : C
			{
				using type 
					= ::std::conditional_t
					< C::enable_2
					, VkPhysicalDeviceMemoryProperties2KHR
					, VkPhysicalDeviceMemoryProperties>;

				apply(auto handle, memory const&, auto const&...rs)
					: C{handle, rs...}
				{
					if constexpr (C::enable_2) {
						memory_properties = {
							VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR,
							C::get_next(),
						}; vkGetPhysicalDeviceMemoryProperties2KHR(C::handle, &memory_properties);
					} else 
						vkGetPhysicalDeviceMemoryProperties(C::handle, &memory_properties);
				}

				type memory_properties;
			};
		};

		template<>
		struct trait<features>
		{
			using phydv_attr = void;

			template<typename C>
			struct apply : C
			{
				using type
					= ::std::conditional_t
					< C::enable_2
					, VkPhysicalDeviceFeatures2KHR
					, VkPhysicalDeviceFeatures>;

				apply(auto handle, features const&, auto const&...rs)
					: C{handle, rs...}
				{
					if constexpr (C::enable_2) {
						features = {
							VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR,
							C::get_next()
						}; vkGetPhysicalDeviceFeatures2KHR(C::handle, &features);
					}
					else
						vkGetPhysicalDeviceFeatures(C::handle, &features);
				}

				type features;
			};
		};
		template<>
		struct trait<queue_family_props>
		{
			using phydv_attr = void;

			template<typename C>
			struct apply : C 
			{
				using type
					= ::std::conditional_t
					< C::enable_2
					, vector<VkQueueFamilyProperties2>
					, vector<VkQueueFamilyProperties>>;

				apply(auto handle, queue_family_props const&, auto&...rs)
					: C{handle, rs...}
				{
					uint32 count;
					vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, nullptr);
					if constexpr (C::enable_2) {
						queue_family_props.reserve(count);
						for (auto i{ 0u }; i < count; i++) 
							queue_family_props.emplace_back(VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, C::get_pnext(i));
;						vkGetPhysicalDeviceQueueFamilyProperties2(handle, &count, queue_family_props.data());
					} else {
						queue_family_props.resize(count);
						vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, queue_family_props.data());
					}
				}

				type queue_family_props;
			};
		};



		template<typename T>
		struct is_queue_device
			: ::std::bool_constant
			< ::std::same_as<T, graphics>
			|| ::std::same_as<T, compute>
			|| ::std::same_as<T, transfer>> 
		{};

		template<template<typename>typename Cond, typename T>
		struct pack_if_and_single
			: ::std::conditional_t<Cond<T>::value, pack_if_single<T>, type_type<T>> 
		{};

		template<>
		struct trait<physical_device>
		{
			using handle_type = VkPhysicalDevice;

			template<typename C>
			struct apply : C
			{
				using self = typename C::self;

				apply(handle_type handle)
					: C{}
					, handle{handle}
				{}

				template<typename...Fs>
				auto create_device(device const &info, Fs const&...features) const
				{
					return object<typename pack_if_and_single<is_queue_device, Fs>::type...
						, device, co<self>, rd<device>>{self::ass(this), features..., info};
				}

				operator handle_type() const noexcept { return handle; }

			protected:
				static constexpr auto enable_2 = query<parent_t<C>, feature::detail_query>(); 

			protected:
				handle_type handle;
			};
		};
	}

	// 
	// device part.
	//

	namespace feature 
	{
		using queue_info_vec = vector<VkDeviceQueueCreateInfo>;

		template<typename...Ts>
		concept contains_graphics
			= impl::is_contains<graphics, Ts...>::value;
		template<typename...Ts>
		concept contains_compute
			= impl::is_contains<compute, Ts...>::value;
		template<typename...Ts>
		concept contains_transfer
			= impl::is_contains<transfer, Ts...>::value;
		template<typename...Ts>
		concept contains_queue 
			=  contains_compute<Ts...> 
			|| contains_transfer<Ts...> 
			|| contains_graphics<Ts...>;


		struct queue_get_base_index // use for pack<*contain queue device*>.
		{
			template<typename C>
			struct apply : C // consume the uint32 from *queue_pack*.
			{
				template<typename Phy, typename...Rs>
				constexpr apply(Phy&& phy, uint32, uint32&&, Rs&&...rs)
					: C{static_cast<Phy&&>(phy), static_cast<Rs&&>(rs)...}
				{}
			};
		};
		struct default_queue_family 
		{
			static constexpr auto family_index{ 0u };
			template<typename C>
			struct apply : C 
			{
				using C::C;
				static constexpr auto family_index{default_queue_family::family_index};
			};
		};
		template<typename...Features>
			requires(contains_queue<Features...>)
		struct trait<pack<Features...>>
		{
			using pack = pack<Features...>;
			static constexpr auto ids = ::std::index_sequence_for<Features...>();
			static constexpr auto num_extension = sizeof...(Features) - 1u;

			template<size_t...ids>
			static auto raise_if_failed_impl(indices<ids...>, pack const& pack, auto& props) noexcept
			{
				return array{ feature::raise_if_failed(get<ids>(pack), props, true)... }; // require to return bool.
			}
			template<typename T>
			static auto raise_if_failed(pack const& pack, T& props) noexcept
			{
				if constexpr (impl::range_of<T, VkQueueFamilyProperties, VkQueueFamilyProperties2>) {
					auto& index = get_family_index(pack);
					return index < ::std::ranges::size(props) ?
						false : raise_if_failed_impl(ids, pack, props[index]);
				} else
					return raise_if_failed_impl(ids, pack, props);
			}

			static constexpr bool customize_family_index{ impl::is_contains<select_queue_family, Features...>::value };

			template<typename B>
			struct apply : ::std::conditional_t<customize_family_index
				, apply_pack<B, type_list<uint32, uint32&&>, trait<Features>..., queue_get_base_index>
				, apply_pack<B, type_list<uint32, uint32&&>, trait<Features>..., default_queue_family, queue_get_base_index>>
			{
				using C = ::std::conditional_t<customize_family_index
					, apply_pack<B, type_list<uint32, uint32&&>, trait<Features>..., queue_get_base_index>
					, apply_pack<B, type_list<uint32, uint32&&>, trait<Features>..., default_queue_family, queue_get_base_index>>;
				using self = typename C::self;

				// have customized family index.

				template<typename PhyDv, typename T, typename...Rs>
				constexpr apply(PhyDv&& parent, T&& pack, Rs&&...rest)
					requires(customize_family_index)
					: apply{::std::get<select_queue_family>(pack).family_index
						, static_cast<PhyDv&&>(parent)
						, static_cast<T&&>(pack), static_cast<Rs&&>(rest)...}
				{}

				// no customized family index.

				template<typename PhyDv, typename T, typename...Rs>
				constexpr apply(PhyDv&& parent, T&& pack, Rs&&...rest)
					requires(!customize_family_index)
					: apply{default_queue_family::family_index
						, static_cast<PhyDv&&>(parent)
						, static_cast<T&&>(pack), static_cast<Rs&&>(rest)...}
				{}

				// generic apply.

				template<typename PhyDv, typename T, typename...Rs>
				constexpr apply(uint32 family_index, PhyDv&& parent, T&& pack, Rs&&...rest)
					: C{static_cast<PhyDv&&>(parent)
					  , family_index, 0u, static_cast<T&&>(pack), static_cast<Rs&&>(rest)... }
				{
					if constexpr (query<PhyDv, queue_family_props>())
						raise_if_failed(pack, parent.queue_family_props);
				}

				template<typename Q = empty_base, typename...Ts>
				auto graphics_queue(uint32 index, Ts const&...features)
					requires(contains_graphics<Features...>)
				{
					return get_queue_<Q>(type<graphics>, index, features...);
				}
				template<typename Q = empty_base, typename...Ts>
				auto compute_queue(uint32 index, Ts const&...features)
					requires(contains_compute<Features...>)
				{
					return get_queue_<Q>(type<compute>, index, features...);
				}
				template<typename Q = empty_base, typename...Ts>
				auto transfer_queue(uint32 index, Ts const&...features)
					requires(contains_transfer<Features...>)
				{
					return get_queue_<Q>(type<transfer>, index, features...);
				}

			private:
				using C::get_queue;
				template<typename Q, typename T, typename...Ts>
				auto get_queue_(type_type<T> t, uint32 index, Ts const&...features)
				{
					auto gidx = uint32(get_queue(t, index));
					VkQueue hqueue; vkGetDeviceQueue(*this, C::family_index, gidx, &hqueue);
					return object<Ts..., queue, co<self>, rd<Q>>{hqueue, features...};
				}

			};
			
			static constexpr float defualt_priorities[32]{};

			template<::std::size_t...ids>
			static constexpr auto enumerate_and_return(indices<ids...>, pack const& info, auto& next)
			{
				return tuple{trait<Features>::create_info(get<ids>(info), next)...};
			}
			static constexpr auto create_info(pack const& pack, queue_info_vec& value)
			{
				return enumerate_and_return(ids, pack
				, value.emplace_back(VkDeviceQueueCreateInfo{
					.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.pQueuePriorities = defualt_priorities,
				}));
			}
		};
		template<impl::where<is_queue_device> T>
		struct trait<T>
		{
			template<typename C>
			struct apply : C
			{
				using self = typename C::self;

				template<typename PhyDv, typename...Rs>
				constexpr apply(PhyDv&& parent, uint32 family_index, uint32&& base_index, T const& info, Rs const&...rs)
					: C{parent, family_index, ::std::move(base_index), rs..., info}
					, base_index_{base_index}
					, queue_count_{info.queue_count}
				{ base_index += info.queue_count; }

			protected:
				constexpr auto get_queue(type_type<T>, size index)
				{
					if (index >= queue_count_) throw; // TODO: 
					return base_index_ + index;
				}

			private:
				uint32 base_index_;
				uint32 queue_count_;
			};

			static constexpr auto create_info(auto&, null_t) {}

			static consteval auto required_flags() noexcept
			{
				if constexpr (contains_compute<T>)
					return VK_QUEUE_COMPUTE_BIT;
				else if constexpr (contains_graphics<T>)
					return VK_QUEUE_GRAPHICS_BIT;
				else if constexpr (contains_transfer<T>)
					return VK_QUEUE_TRANSFER_BIT;
				else
					static_assert(std::is_void_v<T>?0:0); // unreachable.
			}

			static auto raise_if_failed(T const& info, VkQueueFamilyProperties& props) noexcept
			{
				return info.queue_count < props.queueCount 
					|| (props.queueFlags & required_flags()) != required_flags() 
					? false : ((void)(props.queueCount -= info.queue_count), true);
			}
			static auto raise_if_failed(T const& info, VkQueueFamilyProperties2& props) noexcept
			{
				return raise_if_failed(info, props.queueFamilyProperties);
			}
			void always_pass(VkPhysicalDeviceFeatures const&) {}
			void always_pass(VkPhysicalDeviceFeatures2 const&) {}
		};
		template<>
		struct trait<sparse_binding> 
		{
			template<typename C>
			using apply = C;

			static bool raise_if_failed(sparse_binding const& pack, VkPhysicalDeviceFeatures const& features) noexcept
			{
				return features.sparseBinding != 0;
			}
			static bool raise_if_failed(sparse_binding const& pack, VkPhysicalDeviceFeatures2 const& features) noexcept
			{
				return raise_if_failed(pack, features.features);
			}

			static auto raise_if_failed(sparse_binding const& info, VkQueueFamilyProperties const& props) noexcept
			{
				return (props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0u;
			}
			static auto raise_if_failed(sparse_binding const& info, VkQueueFamilyProperties2 const& props) noexcept
			{
				return raise_if_failed(info, props.queueFamilyProperties);
			}
		};
		template<>
		struct trait<select_queue_family> 
		{
			template<typename C>
			struct apply : C
			{
				template<typename PhyDv, typename...Rs>
				apply(PhyDv&& dv, uint32 family_index, uint32&& queue_count, Rs&&...rs)
					: C{static_cast<PhyDv>(dv), family_index, queue_count, static_cast<Rs&&>(rs)...}
					, family_index{family_index}
				{}

				uint32 family_index;
			};

			void always_pass(auto&) {}
		};
		template<>
		struct trait<device>
		{
			template<typename C>
			struct apply : C
			{
				using self = typename C::self;
				using handle_type = VkDevice;

				template<typename PhyDv, typename...Fs>
				apply(PhyDv const& parent, device const& info, Fs&&...fs)
					requires(query<PhyDv, physical_device>()) // Device must be the child of `physical_deivce`.
				{
					if constexpr (query<PhyDv, feature::features>()) 
						(..., raise_if_failed(fs, parent.features));
					auto lay = enum_as_arr<device_layer_name>(fs...);
					auto ext = enum_as_arr<device_extension_name>(fs...);
					impl::infos qinfo{fs..., queue_info_vec{}};
					impl::infos cinfo
					{
						fs..., VkDeviceCreateInfo
						{
							.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
							.queueCreateInfoCount    = uint32(::std::size(qinfo.base::get())),
							.pQueueCreateInfos       = ::std::data(qinfo.base::get()),
							.enabledLayerCount       = uint32(::std::size(lay)),
							.ppEnabledLayerNames     = ::std::data(lay),
							.enabledExtensionCount   = uint32(::std::size(ext)),
							.ppEnabledExtensionNames = ::std::data(ext),
						}
					};
					vkCreateDevice(parent, cinfo, self::ass(this).allocator(), &handle) | propagate;
				}

				apply(apply&&) = delete;
				apply& operator=(apply&&) = delete;

				~apply() { vkDestroyDevice(handle, self::ass(this).allocator()); }

				constexpr operator handle_type() const noexcept { return handle; }

			protected:
				handle_type handle;
			};
		};
	}

	namespace feature 
	{
		template<>
		struct trait<queue>
		{
			using handle_type = VkQueue;
			template<typename C>
			struct apply : C 
			{
				template<typename...Rs>
				apply(handle_type handle, Rs&&...rs)
					: queue_{handle}
				{

				}

				apply(apply&&) noexcept = delete;
				apply& operator=(apply&&) noexcept = delete;

			private:
				VkQueue queue_;
			};
		};
	}
} // namespace unk.
#endif // ENABLE_IDKWN_IMPL

// undef macros.

#undef IDKNW_INLINE_VAR
#undef IDKNW_ENABLED_TYPE_EARSE
#undef IDKNW_ENABLED_DEBUG

#endif // IDKNW_H__

int main()
{
	namespace unf = unk::feature;

	unk::object ins{ unk::instance{}, unf::debug{}, unf::validation{}, };
	unk::object phydv = ins.get_phydv(0
		, unf::memory{}
		, unf::properties{}
		, unf::features{}
		, unf::queue_family_props{});
	unk::object dv = phydv.create_device({
		}, unf::graphics{ 
			.queue_count = 1u, 
		});
	unk::object queue = dv.graphics_queue(0);
	queue.allocator();
	return 0;
}