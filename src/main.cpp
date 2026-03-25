
#include <string>
#include <span>
namespace hh
{
    namespace extension_common {
        // Set allocator and allocation messager.
        struct allocation {

        };
    }

    namespace extension_instance {
        // Instance debug utilities. Such as Debug Messagner.
        struct debug_utils {

        };
        // Instance will hold physical device handle inside instance.
        struct phydevice {

        };
        // Instance will hold physical device group inside instance.
        struct pdvgroups {

        };
        // Surface.
        struct surface {};
    }
    struct instance {

    };
    // use by `extension_instance::phydevice`
    namespace physical_device_properties {


    }
    // use by `extension_instance::surface`
    namespace surface_properties {

    }

    namespace extension_device {
        // This extension can be used by multiple time.
        struct queue_family {
            ::std::uint32_t families;
            ::std::uint32_t queue_count;
            ::std::span<float/*, queue_count*/> priorities; // size should be equal `queue_count`.
        };
        struct queue_families {
            ::std::span<queue_family> families;
        };
    }
    struct device {
        ::std::uint32_t index;
    };

    namespace extension_queue {

    }
    struct queue {
        ::std::uint32_t index;
    };

    // Record the error code and messages.
    struct errcode {
        ::std::uint32_t code;
        ::std::string_view msg;
    };
}

#include <vulkan/vulkan.h>
#include <concepts>
#include <tuple>
#include <ranges>
#include <vector>
#include <cassert>

namespace hh::meta {
    template<typename...Ts>
    struct type_t;
    template<typename...Ts>
    constexpr type_t<Ts...> type;
    template<typename...Ts>
    struct type_t {
        template<typename...Fs>
        constexpr type_t(Fs&&...) {}
        constexpr type_t() {}

        static constexpr auto multi = true;
        template<template<typename...>typename Tp>
        static constexpr auto apply() noexcept { return type<Tp<Ts...>>; }
        template<typename U>
            requires(::std::invocable<U&&, type_t<Ts>...>)
        static constexpr auto apply(U&& v) noexcept { return static_cast<U&&>(v)(type<Ts>...); }
    };
    template<typename T>
    struct type_t<T> {
        static constexpr auto multi = false;

        template<typename Fs>
        constexpr type_t(Fs&&) {}
        constexpr type_t() {}

        using type = T;
        template<template<typename...>typename Tp>
        static constexpr auto apply() noexcept { return type_t<Tp<T>>(); }
        template<typename U>
            requires(::std::invocable<U&&, type_t<T>>)
        static constexpr auto apply(U&& v) noexcept { return static_cast<U&&>(v)(type_t<T>()); }
    };
    template<typename...Ts>
    type_t(Ts&&...) -> type_t<::std::remove_cvref_t<Ts>...>;

    template<typename T = ::std::size_t>
    inline constexpr auto invaild_index = T(-1);

    template<typename T, template<typename...>typename Tp>
    struct is_specialization_of : ::std::false_type {};
    template<typename...Ts, template<typename...>typename Tp>
    struct is_specialization_of<Tp<Ts...>, Tp> : ::std::true_type {};
    template<typename T, template<typename...>typename Tp>
    concept specialization_of = is_specialization_of<T, Tp>::value;

    template<typename T, typename C>
    struct is_same_template : ::std::false_type {};
    template<typename...Ts, typename...St, template<typename...>typename Tp>
    struct is_same_template<Tp<Ts...>, Tp<St...>> : ::std::true_type {};
    // TODO: more template judgement.
    template<typename T, typename C>
    concept same_template = is_same_template<T, C>::value;

    template<typename T, auto tag>
    struct is_tag_of : std::is_same<T, decltype(tag)> {};
    template<typename T, auto tag>
    concept tag_of = is_tag_of<T, tag>::value;

    template<::std::size_t index, typename...Ts>
    struct types_at {};
    template<::std::size_t index, typename T, typename...Ts>
    struct types_at<index, T, Ts...>
        : ::std::conditional_t<index == 0, ::std::type_identity<T>, types_at<index - 1, Ts...>> {
    };
    template<::std::size_t index, typename...Ts>
    using types_at_t = typename types_at<index, Ts...>::type;

    template<::std::size_t index, typename T>
    struct tuple_at {};
    template<::std::size_t index, typename...Ts, template<typename...>typename Tp>
    struct tuple_at<index, Tp<Ts...>> : types_at<index, Ts...> {};
    template<::std::size_t index, typename T>
    using tuple_at_t = typename tuple_at<index, T>::type;

    template<typename Src, template<typename...>typename Dst, typename...Extra>
    struct tuple_apply;
    template<template<typename...>typename SrcTp, typename...Ts, template<typename...>typename Dst, typename...Extra>
    struct tuple_apply<SrcTp<Ts...>, Dst, Extra...> { using type = Dst<Extra..., Ts...>; }; // Extra push front and let template know.
    template<typename Src, template<typename...>typename Dst, typename...Extra>
    using tuple_apply_t = typename tuple_apply<Src, Dst, Extra...>::type;
    // Extra will push at front.
    template<typename Src, typename T, typename...Extra>
    using tuple_apply_type_t = typename tuple_apply<Src, T::template apply, Extra...>::type;


    template<template<typename...>typename Tp, typename...Ts>
    struct bind {
        // the [extra...] parameters in bind<[Requirements], [extra...]>, will place at [requirements]'s back.
        template<typename...Rs>
        using back = Tp<Rs..., Ts...>;
        // the [extra...] parameter in bind<[Requirements], [extra...]>, will place at [requirements]'s front.
        template<typename...Rs>
        using front = Tp<Ts..., Rs...>;
        // transform each of the extra parameters...
        template<template<typename>typename Trs>
        using transform = bind<Tp, typename Trs<Ts>::type...>;
    };

    template<::std::size_t...ids>
    struct indices : ::std::index_sequence<ids...> { static constexpr auto value = indices<ids...>(); };
    template<::std::size_t idx>
    struct indices<idx> { static constexpr auto value = idx; };

    template<::std::size_t idx, typename T>
    constexpr auto find_first_result = []<typename TagTp = ::std::nullptr_t>(TagTp = nullptr) constexpr {
        if constexpr (::std::is_null_pointer_v<TagTp>)
            return idx;
        else
            return type<T>;
    };
    template<typename Ids, typename Ts>
    constexpr auto find_all_result = []<typename T = ::std::nullptr_t>(T = nullptr) constexpr {
        if constexpr (::std::is_null_pointer_v<T>)
            return Ids();
        else
            return Ts();
    };
    template<typename...Ts>
    struct find {
        // find first type_t satisfied the requirement.
        // @return: type_t is `find_result`
        template< template<typename>typename Req
            , ::std::size_t from = 0>
        static consteval auto first() noexcept {
            if constexpr (sizeof...(Ts) > from) {
                if constexpr (Req<types_at_t<from, Ts...>>::value)
                    return find_first_result<from, types_at_t<from, Ts...>>;
                else
                    return find::template first<Req, from + 1>();
            }
            else
                return find_first_result<std::size_t(-1), void>;
        }
        template< template<typename>typename Req
            , ::std::size_t from = 0>
        using first_t = decltype(find::template first<Req, from>());

        // find all types satisfied the requirement.
        // @tparam Pds: Padding types.
        template< template<typename>typename Req>
        static consteval auto all() noexcept { return all_impl_<Req, 0>(::std::index_sequence<>()); }

        template< template<typename>typename Req, ::std::size_t from = 0>
        using all_t = decltype(find::template all<Req, from>());

    private:
        template< template<typename>typename Req
            , ::std::size_t from = 0
            , typename...Pds, ::std::size_t...ids>
        static consteval auto all_impl_(::std::index_sequence<ids...>) noexcept {
            if constexpr (sizeof...(Ts) < from)
                return find_all_result<::std::index_sequence<ids...>, type_t<Pds...>>;
            else if constexpr (Req<types_at_t<from, Ts...>>::value)
                return find::template all_impl_<Req, from + 1, Pds..., types_at_t<from, Ts...>>(::std::index_sequence<ids..., from>());
            else
                return find::template all_impl_<Req, from + 1, Pds...>(::std::index_sequence<ids...>());
        }
    };
    template<template<typename>typename Req, typename...Pds>
    struct find_all {
        template<typename...Ts, template<typename...>typename Tp>
        static consteval auto invoke(Tp<Ts...> const&) noexcept { return find<Ts...>::template all<Req, 0, Pds...>(); }
        template<typename...Ts, template<typename...>typename Tp>
        consteval auto operator()(Tp<Ts...> const& v) const noexcept { return find_all::invoke(v); }
        template<typename...Ts>
        using apply = typename find<Ts...>::template all<Req, 0, Pds...>;
    };
    // Usage:
    // find_first<[requirement]>()(type<Ts...>)() -> index of the result
    // find_first<[requirement]>()(type<Ts...>)(0) -> type<[result type]>
    // find_first<[requirement]>::template apply<Ts...> -> type of find_first_result
    template<template<typename>typename Req>
    struct find_first {
        template<typename...Ts, template<typename...>typename Tp>
        static consteval auto invoke(Tp<Ts...> const&) noexcept { return find<Ts...>::template first<Req, 0>(); }
        template<typename...Ts, template<typename...>typename Tp>
        consteval auto operator()(Tp<Ts...> const& v) const noexcept { return find_first::invoke(v); }
        template<typename...Ts>
        using apply = typename find<Ts...>::template first_t<Req, 0>;
    };

    // Tp is the requirement.
    template<template<typename>typename Tp, typename...Ts>
    struct types_have : ::std::bool_constant<((Tp<Ts>::value) || ...)> {};
    // Tp is the requirement.
    template<template<typename>typename Tp, typename T>
    struct tuple_have;
    // Tp is the requirement.
    template<template<typename>typename Tp, typename...Ts, template<typename...>typename Tts>
    struct tuple_have<Tp, Tts<Ts...>> : types_have<Tp, Ts...> {};

    template<typename T>
    struct tuple_size {};
    template<typename...Ts, template<typename...>typename Tp>
    struct tuple_size<Tp<Ts...>> { static constexpr auto value = sizeof...(Ts); };
    template<typename T>
    constexpr auto tuple_size_v = tuple_size<::std::remove_cvref_t<T>>::value;

    template<typename T>
    struct tuple_empty {};
    template<template<typename...>typename Tp, typename...Ts>
    struct tuple_empty<Tp<Ts...>> { using type = Tp<>; };
    template<typename T>
    using tuple_empty_t = typename tuple_empty<::std::remove_cvref_t<T>>::type;

    template<typename...Ts>
    struct tuple_concat;
    template<template<typename...Ts>typename Tpd, typename...Fs,
        template<typename...Ts>typename Tps, typename...Ts, typename...Rs>
    struct tuple_concat<Tpd<Fs...>, Tps<Ts...>, Rs...> : tuple_concat<Tpd<Fs..., Ts...>, Rs...> {};
    template<template<typename...Ts>typename Tpd, typename...Fs>
    struct tuple_concat<Tpd<Fs...>> { using type = Tpd<Fs...>; };
    template<typename...Ts>
    using tuple_concat_t = typename tuple_concat<::std::remove_cvref_t<Ts>...>::type;

    template<typename T, typename R, ::std::size_t...ids>
    struct tuple_drop;
    template<template<typename...>typename Dst, typename...Rs, template<typename...>typename Src, typename...Ts, ::std::size_t index>
    struct tuple_drop<Dst<Rs...>, Src<Ts...>, index> { using type = Dst<Rs..., Ts...>; };
    template<template<typename...>typename Dst, template<typename...>typename Src, typename...Rs, typename T, typename...Ts, ::std::size_t index, ::std::size_t idx, ::std::size_t...ids>
    struct tuple_drop<Dst<Rs...>, Src<T, Ts...>, index, idx, ids...> : tuple_drop<Dst<Rs..., T>, Src<Ts...>, index + 1, idx, ids...> {};
    template<template<typename...>typename Dst, template<typename...>typename Src, typename...Rs, typename T, typename...Ts, ::std::size_t index, ::std::size_t...ids>
    struct tuple_drop<Dst<Rs...>, Src<T, Ts...>, index, index, ids...> : tuple_drop<Dst<Rs...>, Src<Ts...>, index + 1, ids...> {};
    template<typename R, ::std::size_t...ids>
    using tuple_drop_t = typename tuple_drop<tuple_empty_t<R>, ::std::remove_cvref_t<R>, 0, ids...>::type;

    template<typename T, typename R, ::std::size_t index = invaild_index<>, typename S = type_t<>>
    struct tuple_insert {};
    template<template<typename...>typename Dp, template<typename...>typename Sp, typename T, typename...Ts, typename...Rs, ::std::size_t index, typename...TTs>
    struct tuple_insert<Dp<T, Ts...>, Sp<Rs...>, index, type_t<TTs...>> : tuple_insert<Dp<Ts...>, Sp<Rs...>, index - 1u, type_t<TTs..., T>> {};
    template<template<typename...>typename Dp, template<typename...>typename Sp, typename...Rs, ::std::size_t index, typename...TTs>
    struct tuple_insert<Dp<>, Sp<Rs...>, index, type_t<TTs...>> {
        using type = Dp<TTs..., Rs...>;
        static constexpr auto overflow{ true };
    };
    template<template<typename...>typename Dp, template<typename...>typename Sp, typename...Ts, typename...Rs, typename...TTs>
    struct tuple_insert<Dp<Ts...>, Sp<Rs...>, 0, type_t<TTs...>> {
        using type = Dp<TTs..., Rs..., Ts...>;
        static constexpr auto overflow{ false };
    };
    template<typename T, typename...Ts>
    using tuple_append_t = typename tuple_insert<::std::remove_cvref_t<T>, type_t<Ts...>>::type;




    template<typename T>
    struct index { static constexpr auto value = ::std::size_t(T::value); };
    template<::std::size_t idx>
    struct index_type : index<::std::integral_constant<::std::size_t, idx>> {};
    template<typename T>
    struct query { using type = T; };
    // struct drop {};
    // struct top {};
    struct consume {};
    // struct all {};
    struct length {};
    struct get_all {};
    struct debug_get {};
    struct quiet {};


    template<typename T>
    struct static_error {
        using error_t = T;
        template<typename...Ts>
        constexpr static_error(Ts&&...) {}
        // Stub from error. Check your usage of some of the type.
        template<typename = void>
        T propergate_error() { static_assert(T::value, "Some constraints wasnt satisfied."); return {}; }
    };

    template<auto f, auto...fs>
    constexpr auto compose = []<typename Tp, typename...Tps>(Tp tp, Tps...args) constexpr {
        return type<decltype(f(tp)), decltype(fs(args))...>;
    };

    template<typename T>
    struct trait;

    template<typename T>
    struct end_of_object {
        using all_objects = decltype(T()(type<get_all>));
        template<typename...Ts> constexpr end_of_object(Ts&&...) {}
        static constexpr auto allocator() { return nullptr; }
    };
    struct empty_base {};
    // Make descriptor as public state.
    template<::std::size_t index>
    struct desciptor_trans_ {
        template<typename Unhandled, typename Handled, typename...Ts>
        using apply = type_t<tuple_drop_t<Unhandled, index>, tuple_append_t<Handled, tuple_at_t<index, Unhandled>>, Ts...>;
    };
    template<::std::size_t idx, ::std::size_t r_value, typename R, template<typename...>typename Tp>
    constexpr auto decide_next_(Tp<>) noexcept {
        return type<index_type<r_value>, R>;
    }
    // idx is the enumeration index.
    // r_value is result of the index. (corresponding `R`'s index)
    template<::std::size_t idx, ::std::size_t r_value, typename R, typename T, typename...Ts, template<typename...>typename Tp>
    constexpr auto decide_next_(Tp<T, Ts...>) noexcept {
        if constexpr (requires { trait<T>::index; }) {
            constexpr auto this_val = trait<T>::index;
            if constexpr (trait<R>::index > this_val)
                return decide_next_<idx + 1, idx, T>(Tp<Ts...>()); // changed.
            else
                return decide_next_<idx + 1, r_value, R>(Tp<Ts...>()); // remain unchanged.
        }
        else
            return decide_next_<idx + 1, r_value, R>(Tp<Ts...>()); // remain unchanged.
    }
    // Remember, if you need to call descriptor, you should use `type<...>`, not directly express data in it.
    template<typename TsTp, template<typename>typename Top = end_of_object>
    constexpr auto descriptor = []<typename TagTp>(TagTp value) constexpr {
        if constexpr (requires { tuple_size<TagTp>::value; }) {
            using tag = tuple_at_t<0, TagTp>;
            using unhandled = tuple_at_t<0, TsTp>;
            using handled = tuple_at_t<1, TsTp>;
            if constexpr (::std::same_as<tag, consume>) {
                if constexpr (tuple_size_v<unhandled> == 0)
                    return type<Top<decltype(descriptor<TsTp, Top>)>>;
                else {
                    using rT = decltype(decide_next_<0, 0, tuple_at_t<0, unhandled>>(unhandled()));
                    using nT = tuple_at_t<1, rT>;
                    constexpr auto index = tuple_at_t<0, rT>::value;
                    if constexpr (requires { trait<nT>(); }) // the type was registered.
                        return type<typename trait<nT>::template apply<descriptor<tuple_apply_type_t<TsTp, desciptor_trans_<index>>, Top>>>;
                    else // the type wasn't registered. Might because the type is expressed for other object's creation.
                        return descriptor<tuple_apply_type_t<TsTp, desciptor_trans_<index>>, Top>(value);
                }
            }
            else if constexpr (::std::same_as<tag, length>) // get length.
                return tuple_size_v<TsTp>;
            else if constexpr (specialization_of<tag, query>) // query type is inside descriptor. 
                return find_first<bind<::std::is_same, typename tag::type>::template back>()
                (tuple_concat_t<unhandled, handled>())() != invaild_index<>;
            else if constexpr (specialization_of<tag, index>)
                return type<tuple_at_t<tag::value, TsTp>>;
            else if constexpr (::std::same_as<tag, get_all>)
                return tuple_concat_t<unhandled, handled>();
            else if constexpr (::std::same_as<tag, debug_get>) {
                /*using rT = decltype(decide_next_<0, 0, tuple_at_t<0, unhandled>>(unhandled()));
                using nT = tuple_at_t<1, rT>;
                constexpr auto index = tuple_at_t<0, rT>::value;
                return descriptor<tuple_apply_type_t<TsTp, desciptor_trans_<index>>, Top>;*/
            }
            else if constexpr (!::std::same_as<tag, quiet>) // unrecognized tag.
                return static_error<::std::is_invocable<void, TagTp>>();
        }
        else if constexpr (requires { value(type<TsTp>); })
            return value(type<TsTp>);
        else
            return static_error<::std::is_invocable<void, TagTp>>();
    };

    template<typename T>
    constexpr decltype(auto) atleast_tuple(T&& value) {
        if constexpr (requires { tuple_size<::std::remove_cvref_t<T>>::value; })
            return static_cast<T&&>(value);
        else
            return::std::tuple(static_cast<T&&>(value));
    }
    template<typename T>
    constexpr decltype(auto) atleast_array(T&& value) {
        if constexpr (::std::ranges::range<T>)
            return static_cast<T&&>(value);
        else
            return::std::array{ static_cast<T&&>(value) };
    }
    template<typename T>
    constexpr auto as_type() noexcept {
        return decltype(type_t(::std::declval<T>()))();
    }

    struct empty_range {
        static constexpr void* data() { return nullptr; }
        static constexpr auto size() { return 0; }
        static constexpr void* begin() { return nullptr; }
        static constexpr void* cbegin() { return nullptr; }
        static constexpr void* rbegin() { return nullptr; }
        static constexpr void* cend() { return nullptr; }
        static constexpr void* rend() { return nullptr; }
        static constexpr void* end() { return nullptr; }
    };

#define VKTL_DEVICE_LAYER_NAME_ device_layer
#define VKTL_DEVICE_EXT_NAME_ device_extension
#define VKTL_INSTANCE_LAYER_NAME_ instance_layer
#define VKTL_INSTANCE_EXT_NAME_ instance_extension
    template<bool is_device, bool is_layer, typename T>
    constexpr decltype(auto) extract_match_() {
        if constexpr (is_device && is_layer && requires { trait<T>::VKTL_DEVICE_LAYER_NAME_; })
            return atleast_array(trait<T>::VKTL_DEVICE_LAYER_NAME_);
        else if constexpr (is_device && !is_layer && requires { trait<T>::VKTL_DEVICE_EXT_NAME_; })
            return atleast_array(trait<T>::VKTL_DEVICE_EXT_NAME_);
        else if constexpr (!is_device && is_layer && requires { trait<T>::VKTL_INSTANCE_LAYER_NAME_; })
            return atleast_array(trait<T>::VKTL_INSTANCE_LAYER_NAME_);
        else if constexpr (!is_device && !is_layer && requires { trait<T>::VKTL_INSTANCE_EXT_NAME_; })
            return atleast_array(trait<T>::VKTL_INSTANCE_EXT_NAME_);
        else
            return empty_range{};
    }

    inline constexpr auto streq_(const char* left, const char* right) {
        auto itl{ left }, itr{ right };
        for (; *itl && *itr; itl++, itr++)
            if (*itl != *itr) return false;
        return *itl == *itr;
    }
    template<bool is_device, bool is_layer, ::std::size_t size>
    constexpr auto extract_impl_(::std::array<const char*, size> value) {
        return value;
    }
    template<::std::size_t index = 0, typename T, typename C, ::std::size_t...xids, ::std::size_t...ids>
    constexpr auto extract_unique_(T& need_to_append, C& source, ::std::index_sequence<xids...> sids, ::std::index_sequence<ids...> rst) {
        if constexpr (index < ::std::ranges::ssize(need_to_append)) {
            if constexpr (((streq_(need_to_append[index], source[xids])) && ...))
                return extract_unique_<index + 1>(need_to_append, source, sids, ::std::index_sequence<ids..., index>());
            else
                return extract_unique_<index + 1>(need_to_append, source, sids, ::std::index_sequence<ids...>());
        }
        else
            return::std::array{ static_cast<T&&>(need_to_append)[ids]... };
    }
    template<bool is_device, bool is_layer, typename T, typename...Ts, typename Rg>
    constexpr auto extract_impl_(Rg value) {
        constexpr auto arr = extract_match_<is_device, is_layer, T>();
        constexpr auto size = ::std::ssize(value);
        constexpr auto nsize = ::std::ranges::ssize(arr) + size;
        ::std::array<const char*, nsize> result{};
        ::std::ranges::copy(::std::move(value), result.begin());
        if constexpr (::std::ranges::ssize(arr) > 0)
            ::std::ranges::copy(extract_unique_<0>(arr, value,
                ::std::make_index_sequence<size>(), ::std::index_sequence<>()), result.begin() + size);
        return extract_impl_<is_device, is_layer, Ts...>(::std::move(result));
    }
    template<bool is_device, bool is_layer, template<typename...>typename Tp, typename...Ts>
    constexpr auto extract_(Tp<Ts...> const&) {
        return extract_impl_<is_device, is_layer, Ts...>(empty_range{});
    }

    template<::std::size_t size>
    constexpr auto enabled_(::std::array<const char*, size> const& arr, const char* val) {
        for (auto c : arr) if (::std::string_view{ c } == ::std::string_view{ val }) return true;
        return false;
    }

    template<typename T>
    struct exclude_from_vktpl {
        using vktuple_ignore = void;
        static constexpr auto value = ::std::is_null_pointer_v<T> || requires { typename T::vktuple_ignore; } || requires { T::vktuple_ignore; };
    };
    template<typename T, typename C>
    constexpr auto make_vktuple_ivk_(C&& hstVk, T&& val) noexcept {
        if constexpr (requires{ trait<::std::remove_cvref_t<T>>::create_info(::std::declval<C&&>(), ::std::declval<T&&>()); })
            return trait<::std::remove_cvref_t<T>>::create_info(static_cast<C&&>(hstVk), static_cast<T&&>(val));
    }
    template<typename C, typename T, typename Tup>
    constexpr decltype(auto) make_vktuple_sgl_(C&& hstVk, T&& val, Tup&& tuples) noexcept {
        if constexpr (!::std::is_void_v<decltype(make_vktuple_ivk_(::std::declval<C&&>(), ::std::declval<T&&>()))>)
            return::std::tuple_cat(atleast_tuple(make_vktuple_ivk_(static_cast<C&&>(hstVk), static_cast<T&&>(val))), static_cast<Tup&&>(tuples));
        else
            return static_cast<Tup&&>(tuples);
    }
    template<::std::size_t index = 0, typename T, typename Tup, typename Rst = ::std::tuple<>>
    constexpr auto make_vktuple_(T&& val, Tup vals, Rst&& tups = ::std::tuple<>()) noexcept {
        if constexpr (index < tuple_size_v<Tup>)
            return make_vktuple_<index + 1>(static_cast<T&&>(val), static_cast<Tup&&>(vals),
                make_vktuple_sgl_(val, get<index>(static_cast<Tup&&>(vals)), static_cast<Rst&&>(tups)));
        // else if constexpr (exclude_from_vktpl<::std::remove_cvref_t<T>>::value)
        //     return static_cast<Rst&&>(tups);
        else
            return::std::tuple_cat(atleast_tuple(static_cast<T&&>(val)), static_cast<Rst&&>(tups));
    }
    template<::std::size_t index = 0, typename T = void>
    constexpr auto make_vktuple_next_(T& tuple) {
        if constexpr (index < tuple_size_v<T> -1) {
            if constexpr (::std::ranges::range<tuple_at_t<index, T>>) {
                auto& dst = get<index>(tuple);
                auto& src = get<index + 1>(tuple);
                assert(::std::ranges::size(dst) == ::std::ranges::size(src)
                    && "`vktuple` cannot connect `pNext` from different size.");
                auto itd{ ::std::ranges::begin(dst) };
                auto its{ ::std::ranges::begin(src) };
                for (; itd != ::std::ranges::end(dst); itd++, its++)
                    itd->pNext = &*itd;
            }
            else
                get<index>(tuple).pNext = &get<index + 1>(tuple);
            return make_vktuple_next_<index + 1>(tuple);
        }
    }
    template<typename T>
    struct vktuple : T {
        template<typename...Ext>
        constexpr vktuple(Ext&&...exts)
            : T{ make_vktuple_<0>(static_cast<Ext&&>(exts)...) }
        {
            make_vktuple_next_(static_cast<T&>(*this));
        }

        constexpr operator tuple_at_t<0, T> const* ()
            const noexcept {
            return &get<0>(static_cast<T const&>(*this));
        }
        constexpr operator tuple_at_t<0, T>* ()
            noexcept {
            return &get<0>(static_cast<T&>(*this));
        }
    };
    template<typename T, typename Tp>
    vktuple(T&&, Tp&&) -> vktuple<decltype(
        make_vktuple_<0>(::std::declval<T&&>(), ::std::declval<Tp&&>()))>;

    template<auto ivk, typename...Ts> // avoid some compiler's ice.
    constexpr auto type_from_(Ts&&...v) noexcept {
        if constexpr (requires{ ivk(static_cast<Ts&&>(v)...); }) {
            static_assert(!::std::is_void_v<decltype(ivk(static_cast<Ts&&>(v)...))>,
                "Descriptor should not return void, return error_t<[reason]> when enter unexcepted batch instead.");
            return ivk(static_cast<Ts&&>(v)...);
        }
        else
            static_assert(requires{ ivk(static_cast<Ts&&>(v)...); }, "Descriptor cannot invoke specified parameter.");
    }
    template<auto d, typename...Ts>
    using type_from = decltype(type_from_<d>(::std::declval<Ts>()...))::type;
    template<auto d>
    using from_default = type_from<d, type_t<consume>>;

    template<auto desc, typename...Ext>
    struct object : type_from<desc, Ext...> {
        template<typename...Ts>
        constexpr object(Ts&&...values)
            : type_from<desc, Ext...>{ ::std::tuple(static_cast<Ts&&>(values)...) }
        {}
    };
    template<typename...Ts>
    object(Ts&&...) -> object<
        descriptor<type_t<type_t<::std::remove_cvref_t<Ts>...>, type_t<>>>, type_t<consume>>;
    template<typename Dst, typename T>
    constexpr decltype(auto) get_from(T&& tuple) {
        constexpr auto index = find_first<bind<::std::is_same, Dst>::template back>::invoke(as_type<T>())();
        return get<index>(static_cast<T&&>(tuple));
    }

    static constexpr auto extension_stage = 0x00000000u; // this stage of object, can extend the main entries' function.
    static constexpr auto host_stage = 0xff000000u;      // this stage of object, is the main entry of the whole object.
    static constexpr auto require_stage = 0xff00ffffu;   // this stage of object, can be observed by host stage object.
}
namespace hh::meta {

    inline constexpr struct propagate_suger {
        static void trigger(::VkResult result, ::std::string_view msg) {
            if (result != ::VK_SUCCESS)
                throw errcode{ ::std::uint32_t(result), msg };
        }
        struct package {
            ~package() { trigger(code, "Inner error."); }
            void operator|(::std::string_view msg) { trigger(code, msg); }

            ::VkResult code;
        };
        friend package operator|(::VkResult result, propagate_suger) {
            return { result };
        }
    } propagate;

    // sugar invoker for multiple call function. (function need to call get size then get data)
    inline constexpr struct invoke_sugar {
        template<typename T, typename F, typename...Args>
        ::VkResult operator()(T& value, F fn, Args...args) const noexcept {
            ::std::uint32_t count;
            ::VkResult code = fn(args..., &count, nullptr);
            if (code == VK_SUCCESS) {
                value.resize(count);
                fn(static_cast<Args&&>(args)..., &count, value.data());
            }
            return code;
        }
    } invoke;



    // PAGE: Common extesnions.


    template<> struct trait<extension_common::allocation> {
        static constexpr auto index = require_stage + 0x1;
        template<auto d>
        struct apply : from_default<d> {
            template<typename T>
            constexpr apply(T const& vals)
                : from_default<d>{ vals }
            {}

            constexpr::VkAllocationCallbacks allocator() noexcept {
                return {};
            }
        };
    };

    template<typename T, typename D = empty_base>
    struct child_of : D {};
    template<typename T, typename D> struct trait<child_of<T, D>> {
        static constexpr auto index = require_stage + 0xffff;
        template<auto d>
        struct apply : from_default<d> {
            using parent_type = T;
            using from_default<d>::from_default;
        };
    };


    // PAGE: Instance.


    template<> struct trait<instance> {
        static constexpr auto index = host_stage + 0x1u;
        using handle_type = ::VkInstance;
        struct from_instance {
            handle_type handle;
        };
        template<auto d>
        struct apply : from_default<d> {
            template<typename T>
            apply(T const& tps) : from_default<d>{ tps } {
                vktuple appdesc{ ::VkApplicationInfo {
                    .sType = ::VK_STRUCTURE_TYPE_APPLICATION_INFO,
                    // .pApplicationName = 
                    // .applicationVersion = 0u,
                    .pEngineName = "__VK_@QWE@__",
                    .engineVersion = 0u,
                    .apiVersion = VK_API_VERSION_1_0,
                }, tps };

                constexpr auto layers = extract_<false, true>(tps);
                constexpr auto extensions = extract_<false, false>(tps);
                vktuple desc{ ::VkInstanceCreateInfo {
                    .sType = ::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                    .pApplicationInfo = appdesc,
                    .enabledLayerCount = ::std::uint32_t(::std::ranges::size(layers)),
                    .ppEnabledLayerNames = ::std::ranges::data(layers),
                    .enabledExtensionCount = ::std::uint32_t(::std::ranges::size(extensions)),
                    .ppEnabledExtensionNames = ::std::ranges::data(extensions),
                }, tps };
                ::vkCreateInstance(desc, this->allocator(), &instance_) | propagate | "Create device failure.";
            }

            ~apply() { ::vkDestroyInstance(instance_, this->allocator()); }

            template<typename...Ts>
            auto create(Ts&&...desc) { return object{ static_cast<Ts&&>(desc)... }; }

            operator handle_type() const noexcept { return instance_; }

        protected:
            handle_type instance_;
        };
    };
    template<> struct trait<extension_instance::phydevice> {
        static constexpr auto index = extension_stage + 0xff;
        using handle_type = ::VkPhysicalDevice;

        struct device_details {
            handle_type phydv;
        };

        template<auto d>
        struct apply : from_default<d> {
            using base = from_default<d>;
            static_assert(!d(type<query<extension_instance::pdvgroups>>), "`phydevice` cannot use with `pdvgroups`.");
            template<typename Tp>
            constexpr apply(Tp const& tps) : from_default<d>{ tps } {
                invoke(phydvs_, ::vkEnumeratePhysicalDevices, this->instance_) | propagate | "Enumerate physical device failure.";
            }

            ::std::size_t num_phydevice() const noexcept { return phydvs_.size(); }

            // Get properties of physcial devices.
            // Each of the object from `Ts...` should from `namespace physical_device_properties`.
            // Only enabled `VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME` then props will be used.
            template<typename...Ts>
            auto properties_phydv(::std::size_t index, Ts&&...props) {
                if constexpr (enabled_(this->extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
                    vktuple result{ ::VkPhysicalDeviceProperties2KHR {
                        .sType = ::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
                    }, static_cast<Ts&&>(props)... };
                    ::vkGetPhysicalDeviceProperties2KHR(phydvs_[index], result);
                    return result;
                }
                else {
                    ::VkPhysicalDeviceProperties result;
                    ::vkGetPhysicalDeviceProperties(phydvs_[index], &result);
                    return result;
                }
            }

            template<typename...Ts>
            auto device(device device_info, Ts&&...exts) {
                assert(device_info.index < phydvs_.size() && "Get device out of range.");
                return object{ static_cast<Ts&&>(exts)..., ::std::move(device_info),
                    child_of<typename apply::all_objects, device_details>{phydvs_[device_info.index]} };
            }

        protected:
            ::std::vector<handle_type> phydvs_;
        };
    };
    template<> struct trait<extension_instance::pdvgroups> {
        using type = extension_instance::pdvgroups;
        static constexpr auto index = extension_stage + 0xff;
        static constexpr auto VKTL_INSTANCE_EXT_NAME_ = VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME;

        using handle_type = ::VkPhysicalDeviceGroupProperties;
    };
    template<> struct trait<extension_instance::surface> {
        // using type = extension_instance::surface;
        static constexpr auto index = extension_stage + 0x4;
        static constexpr auto VKTL_INSTANCE_EXT_NAME_ = VK_KHR_SURFACE_EXTENSION_NAME;
        template<auto d>
        struct apply : from_default<d> {

            template<typename Tps>
            constexpr apply(Tps const& tps) : from_default<d>{ tps } {
                static_assert(d(type<query<extension_instance::phydevice>>) || d(type<query<extension_instance::pdvgroups>>),
                    "Instance extension `surface` require instance extension `pdvgroups` or `phydevice`.");
            }

            // Get properties of physcial devices of surface.
            template<typename...Ts>
            auto properties_surface(Ts&&...exts) {
                // if constexpr (enabled_(this->extesnions, VK_KHR_GET_DISPLAY_PROPERTIES_2_EXTENSION_NAME))

                // vkGetPhysicalDeviceSurfaceFormatsKHR;
            }
        };
    };
    template<> struct trait<extension_instance::debug_utils> {
        static constexpr auto index = extension_stage + 0x1;
        static constexpr auto VKTL_INSTANCE_EXT_NAME_ = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        static constexpr auto create_info(::VkInstanceCreateInfo const&, extension_instance::debug_utils const&) {
            return::VkDebugUtilsMessengerCreateInfoEXT{
                .sType = ::VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            };
        }
        template<auto d>
        using apply = from_default<d>;
    };



    // PAGE: Device

    using device_queue_create_infos = ::std::vector<::VkDeviceQueueCreateInfo>;
    template<> struct trait<device> {
        static constexpr auto index = host_stage + 0x2;
        using handle_type = ::VkDevice;
        template<auto d>
        struct apply : from_default<d> {
            template<typename Tps>
            apply(Tps const& tps) : from_default<d>{ tps } {
                vktuple phyf{ ::VkPhysicalDeviceFeatures {}, tps };
                constexpr auto layers = extract_<true, true>(tps);
                constexpr auto extensions = extract_<true, false>(tps);
                vktuple qistub{ device_queue_create_infos {}, tps };
                auto& queue_infos = get<0>(qistub);
                vktuple desc{ ::VkDeviceCreateInfo {
                    .sType = ::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                    .queueCreateInfoCount = ::std::uint32_t(::std::ranges::size(queue_infos)),
                    .pQueueCreateInfos = ::std::ranges::data(queue_infos),
                    .enabledLayerCount = ::std::uint32_t(::std::ranges::size(layers)),
                    .ppEnabledLayerNames = ::std::ranges::data(layers),
                    .enabledExtensionCount = ::std::uint32_t(::std::ranges::size(extensions)),
                    .ppEnabledExtensionNames = ::std::ranges::data(extensions),
                    .pEnabledFeatures = phyf,
                }, tps };
                ::vkCreateDevice(get<tuple_size_v<Tps> -1>(tps).phydv,
                    desc, this->allocator(), &device_) | propagate | "Create device failure.";
            }

            ~apply() { ::vkDestroyDevice(device_, this->allocator()); }

        protected:
            handle_type device_;
        };
    };
    template<> struct trait<extension_device::queue_family> {
        using self = extension_device::queue_family;
        static constexpr auto index = extension_stage + 0x1;
        using handle_type = ::VkQueue;

        struct queue_details {
            handle_type queue;
        };

        static constexpr auto create_info(device_queue_create_infos& info, self const& self) {
            info.push_back({
                .sType = ::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = self.families,
                .queueCount = self.queue_count,
                .pQueuePriorities = self.priorities.data(),
                });
        }
        template<auto d>
        struct apply : from_default<d> {
            template<typename Tps>
            apply(Tps const& tps) : from_default<d>{ tps } {
                auto& value = get_from<self>(tps);
            }

            template<typename...Ts>
            auto queue(queue queue_info, Ts&&...exts) {
                assert(queue_info.index < queue_count_ && "Get queue out of range.");
                handle_type handle;
                ::vkGetDeviceQueue(this->device_, family_, queue_info.index, &handle);
                return object{ static_cast<Ts&&>(exts)..., ::std::move(queue_info) ,child_of<typename apply::all_objects, queue_details>{{handle}} };
            }

        protected:
            ::std::uint32_t family_;
            ::std::uint32_t queue_count_;
        };
    };

}

template<auto d>
struct c {
    static constexpr auto s = d(0);
};
template<typename C>
constexpr auto ww = []<typename T>(T) {
    return::std::same_as<::std::tuple_element_t<0, C>, T>;
};


#include <iostream>
#include <typeinfo>
int main() {

    ::std::cout << c <ww <::std::tuple<int>>>().s;

    using namespace hh::meta;
    namespace ext = hh::extension_common;
    // extension instance.
    namespace extins = hh::extension_instance;
    // using ck = type_t<vktl::instance, type_t<vktl::allocation>, type_t<>>;
    // auto c = descriptor<ck>;
    // auto v = c(type<consume_t>);

    object v{ hh::instance{}, extins::surface{}, extins::debug_utils{}, extins::phydevice{} };
    object d = v.device({ 0 });
    // ::std::cout << v.c(type<query<extins::phydevice>>);
    // constexpr auto c = hh::meta::descriptor<type_t<type_t<extins::surface, extins::debug_utils, extins::phydevice, hh::instance>, type_t<>>>;
    // using cv = decltype(decide_next_<0, 0, void>(type<hh::instance, extins::surface, extins::debug_utils>));
    // using t = tuple_at_t<1, cv>;
    // ::std::cout << typeid(c(type<debug_get>)).name();

    // v.phydv_properties(0);
    // ::std::print(::std::cout, "{}", v.num_phydevice());
    // object phyd = v.create(hh::physical_device{});


    //int w = 800, h = 600;
    //initWindow(w, h);
    //createInstance();
    //createDevice();
    //createRenderPass(); // 创建 RenderPass
    //createSwapchain(w, h); // 内部需要 RenderPass 来创建 Framebuffer
    //createSyncObjects();

    // MSG msg;
    // while (true) {
    //     if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    //         if (msg.message == WM_QUIT) break;
    //         TranslateMessage(&msg);
    //         DispatchMessage(&msg);
    //     }
    //     else {
    //         drawFrame();
    //     }
    // }


    return 0;
}




