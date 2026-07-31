#pragma once

#include "public.hpp"

// --------------PRIVATE SCOPE---------------

#if defined(__has_cpp_attribute)
#  if __has_cpp_attribute(no_unique_address)
#    define VKTL_NO_UNIQUE_ADDRESS [[no_unique_address, msvc::no_unique_address]]
#  endif
#  if __has_cpp_attribute(nodiscard)
#    define VKTL_NODISCARD [[nodiscard]]
#  endif
#  if __has_cpp_attribute(likely)
#    define VKTL_LIKELY [[likely]]
#  endif
#  if __has_cpp_attribute(unlikely)
#    define VKTL_UNLIKELY [[unlikely]]
#  endif
#endif

#if !defined(VKTL_NO_UNIQUE_ADDRESS)
#  define VKTL_NO_UNIQUE_ADDRESS
#endif
#if !defined(VKTL_NODISCARD)
#  define VKTL_NODISCARD
#endif
#if !defined(VKTL_UNLIKELY)
#  define VKTL_UNLIKELY
#endif
#if !defined(VKTL_LIKELY)
#  define VKTL_LIKELY
#endif

#if VKTL_HAVE_STD_
#   include <concepts>
#   include <type_traits>
#   include <tuple>
#   include <vector>
#   include <string_view>
#   include <memory>
#   include <utility>
#   include <ranges>
#   include <algorithm>
#   include <array>
#   include <queue>
#   include <unordered_map>
#   include <atomic> 
#   include <thread>
#   include <mutex>
#   include <shared_mutex>
#   include <condition_variable>
#   include <semaphore>
// #   include <optional>
#endif

#if defined(VKTL_NO_ASSERT)
#   define assert(...)
#else 
#   include <assert.h>
#endif

#if !defined(VK_NAMESPACE)
#   define VK_NAMESPACE 
#endif 

namespace VK_NAMESPACE {
#include <vulkan/vulkan.h>
}

#define VK_ VK_NAMESPACE::

#define forward_(x) ::std::forward<decltype(x)>(x)

VKTL_EXPORT_ namespace vktl::detail {
    inline constexpr::std::uint8_t COMMON_SCOPE = 0x0;

    template<typename T>
    constexpr bool always_false = false;

    template<typename...Cs>
    struct type_list {};
    template<typename T>
    struct type : ::std::type_identity<T> {};

    template <typename List, typename...Ts>
    struct tuple_cat;
    template <typename List>
    struct tuple_cat<List> { using type = List; };
    template <template<typename...>typename Tp1, template<typename...>typename Tp2, typename... T1s, typename...T2s, typename...Rs>
    struct tuple_cat<Tp1<T1s...>, Tp2<T2s...>, Rs...> : tuple_cat<Tp1<T1s...>, Rs...> {};

    template <typename List1, typename...Lists>
    using tuple_cat_t = typename tuple_cat<List1, Lists...>::type;

    template <typename List, typename Left = type_list<>, typename Right = type_list<>>
    struct tuple_split;
    template <template<typename...>typename Tp, template<typename...>typename Tp1, template<typename...>typename Tp2,
        typename T1, typename T2, typename... Tail, typename... Ls, typename... Rs>
    struct tuple_split<Tp<T1, T2, Tail...>, Tp1<Ls...>, Tp2<Rs...>>
        : tuple_split<Tp<Tail...>, Tp1<Ls..., T1>, Tp2<Rs..., T2>> {
    };
    template <template<typename...>typename Tp, template<typename...>typename Tp1, template<typename...>typename Tp2,
        typename T, typename... Ls, typename... Rs>
    struct tuple_split<Tp<T>, Tp1<Ls...>, Tp2<Rs...>> {
        using left = Tp<Ls..., T>;
        using right = Tp<Rs...>;
    };
    template <template<typename...>typename Tp, template<typename...>typename Tp1, template<typename...>typename Tp2,
        typename... Ls, typename... Rs>
    struct tuple_split<Tp<>, Tp1<Ls...>, Tp2<Rs...>> {
        using left = Tp<Ls...>;
        using right = Tp<Rs...>;
    };

    template <template<typename, typename>typename Pred, typename List1, typename List2>
    struct tuple_sort_merge;
    template <template<typename, typename>typename Pred, typename T1, typename... Tail1, typename T2, typename...Tail2>
    struct tuple_sort_merge<Pred, type_list<T1, Tail1...>, type_list<T2, Tail2...>> {
        using type = ::std::conditional_t<
            (Pred<T1, T2>::value),
            tuple_cat_t<type_list<T1>, typename tuple_sort_merge<Pred, type_list<Tail1...>, type_list<T2, Tail2...>>::type>,
            tuple_cat_t<type_list<T2>, typename tuple_sort_merge<Pred, type_list<T1, Tail1...>, type_list<Tail2...>>::type>
        >;
    };
    template <template<typename, typename>typename Pred, typename... Ts>
    struct tuple_sort_merge<Pred, type_list<Ts...>, type_list<>> { using type = type_list<Ts...>; };
    template <template<typename, typename>typename Pred, typename... Ts>
    struct tuple_sort_merge<Pred, type_list<>, type_list<Ts...>> { using type = type_list<Ts...>; };
    template <template<typename, typename>typename Pred>
    struct tuple_sort_merge<Pred, type_list<>, type_list<>> { using type = type_list<>; };

    template <template<typename, typename>typename Pred, typename List>
    struct tuple_sort { using type = List; };
    template <template<typename, typename>typename Pred,
        typename T1, typename T2, typename... Tail>
    struct tuple_sort<Pred, type_list<T1, T2, Tail...>> {
        using splitter = tuple_split<type_list<T1, T2, Tail...>>;
        using sorted_left = typename tuple_sort<Pred, typename splitter::left>::type;
        using sorted_right = typename tuple_sort<Pred, typename splitter::right>::type;

        using type = typename tuple_sort_merge<Pred, sorted_left, sorted_right>::type;
    };

    template <template<typename, typename>typename Pred, typename List>
    using tuple_sort_t = typename tuple_sort<Pred, List>::type;

    template<typename T>
    struct tuple_like : ::std::false_type {};
    template<typename...Ts>
    struct tuple_like<::std::tuple<Ts...>> : ::std::true_type {};
    template<typename...Ts>
    struct tuple_like<type_list<Ts...>> : ::std::true_type {};

    template<::std::size_t index, typename T>
    struct tuple_at {};
    template<::std::size_t index, template<typename...>typename Tp, typename...Ts>
        requires(tuple_like<Tp<Ts...>>::value)
    struct tuple_at<index, Tp<Ts...>>
        : ::std::tuple_element<index, ::std::tuple<Ts...>> {
    };

    template<::std::size_t index, typename T>
    using tuple_at_t = typename tuple_at<index, T>::type;

    template<typename T>
    struct tuple_size {};
    template<template<typename...>typename Tp, typename...Ts>
        requires(tuple_like<Tp<Ts...>>::value)
    struct tuple_size<Tp<Ts...>> { static constexpr auto value{ sizeof...(Ts) }; };

    template<typename T>
        requires(tuple_like<T>::value)
    constexpr auto tuple_size_v = tuple_size<::std::remove_cvref_t<T>>::value;

    // template<typename...Ts>
    // struct tuple_size<type_list<Ts...>> { static constexpr auto value{ sizeof...(Ts) }; };

    // specialization should always have template `apply<typename>` and integral value `order`.
    // use for order, if the `index` is not specified, it will consider as `order_at_last`.
    template<typename T>
    struct meta_of;

    template<auto val>
    struct const_ {
        using type = ::std::remove_cvref_t<decltype(val)>;
        static constexpr type value = val;
    };
    template<auto val>
    constexpr const_<val> const_v{};

    template<::std::size_t index>
    using const_index = const_<index>;

    template<::std::size_t index>
    constexpr const_index<index> const_index_v{};

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

    template<typename T, ::std::size_t size, typename C, ::std::size_t...ids>
    constexpr auto append_array(::std::array<T, size> value, C&& new_val, ::std::index_sequence<ids...>) {
        return::std::array{ ::std::move(value.at(ids))..., T(static_cast<C&&>(new_val)) };
    }
    template<typename T, ::std::size_t size, typename C>
    constexpr auto append_array(::std::array<T, size> value, C&& new_val) {
        return append_array(::std::move(value), static_cast<C&&>(new_val), ::std::make_index_sequence<size>());
    }

    template<typename T, typename...Args>
    auto invoke(T, Args...) = delete;

    inline constexpr auto make_type_id(::std::uint8_t scope, ::std::uint32_t index) {
        return(::std::uint32_t(scope) << 24) | index;
    }

    template<::std::size_t size>
    struct fixed_string {
        char data[size];

        constexpr auto begin() const noexcept { return data; }
        constexpr auto end() const noexcept { return data + size; }
        constexpr auto begin() noexcept { return data; }
        constexpr auto end() noexcept { return data + size; }
        constexpr auto rbegin() noexcept { return data + size - 1; }
        constexpr auto rend() noexcept { return data - 1; }
    };
    template<::std::size_t size>
    fixed_string(const char(&)[size]) -> fixed_string<size>;

    template<typename T>
    struct make_index_seq_of {};
    template<typename T, ::std::size_t size>
    struct make_index_seq_of<::std::array<T, size>> : ::std::make_index_sequence<size> {};

    template<template<typename>typename Condition>
    struct find_all_if_ {
        template<::std::size_t index, ::std::size_t...ids, typename T, typename...Ts>
        static constexpr auto impl(::std::index_sequence<ids...>, T const&, Ts const&...vals) noexcept {
            if constexpr (Condition<T>::value)
                return impl(::std::index_sequence<ids..., index>(), vals...);
            else
                return impl(::std::index_sequence<ids...>(), vals...);
        }

        template<::std::size_t...ids>
        static constexpr auto impl(::std::index_sequence<ids...> r) noexcept {
            return r;
        }

        template<typename...Ts>
        constexpr auto operator()(Ts const&...vals) const noexcept {
            return impl(::std::index_sequence<>(), vals...);
        }
    };

    template <typename T, typename...Args>
    consteval::std::size_t get_index() {
        constexpr bool matches[] = { ::std::same_as<T, Args>... };
        for (::std::size_t i = 0; i < sizeof...(Args); ++i) {
            if (matches[i]) return i;
        }
        return static_cast<::std::size_t>(-1);
    }

    template<::std::size_t index, typename...Ts>
    constexpr auto&& get_by_index(Ts&&...vals) {
        return get_by<index>(::std::forward_as_tuple(static_cast<Ts&&>(vals)...));
    }

    template <typename T, typename Tuple>
    struct find_if_same;

    template <typename T, template<typename...>typename Tp, typename... Args>
        requires(tuple_like<Tp<Args...>>::value)
    struct find_if_same<T, Tp<Args...>>
        : ::std::integral_constant<::std::size_t, get_index<T, Args...>()> {
    };

    template <typename T, typename Tuple>
    constexpr::std::size_t find_if_same_v = find_if_same<T, Tuple>::value;

    template <typename Like, typename T>
    VKTL_NODISCARD constexpr auto&& forward_like(T&& member) noexcept {
        if constexpr (::std::is_lvalue_reference_v<T>) {
            return static_cast<
                ::std::conditional_t<
                ::std::is_const_v<::std::remove_reference_t<Like>>
                , ::std::add_const_t<::std::remove_reference_t<T>>
                , ::std::remove_reference_t<T>>&>(member);
        }
        else {
            return static_cast<
                ::std::conditional_t<
                ::std::is_const_v<::std::remove_reference_t<Like>>
                , ::std::add_const_t<::std::remove_reference_t<T>>
                , ::std::remove_reference_t<T>>&&>(member);
        }
    }

    template<typename T, typename U>
    using forward_like_t = decltype(forward_like<U>(::std::declval<T>()));

    template<typename State, typename This>
    struct consume : This {
        using foundation = This;

        // using This::This causing error.

        constexpr consume(auto&&...infos)
            : This{ forward_(infos)... } {

        }
    };

    template<typename Type>
    struct move_only_function;
    template<typename Rt, typename...Args>
    struct move_only_function<Rt(Args...)> {
        constexpr move_only_function() = default;
        // TODO: because lasy, should not express function ptr.
        template<typename Fn>
        constexpr move_only_function(Fn&& fn)
            requires(::std::convertible_to<::std::invoke_result_t<Fn&, Args...>, Rt>&& ::std::invocable<Fn&, Args...>)
        : ptr_{ new::std::remove_cvref_t<Fn>(static_cast<Fn&&>(fn)) } {
            deleter_ = [](void* ptr) { delete static_cast<::std::remove_cvref_t<Fn>*>(ptr); };
            fn_ = [](void* ptr, Args...vals) -> Rt { return (*static_cast<::std::remove_cvref_t<Fn>*>(ptr))(vals...); };
        }

        move_only_function(move_only_function const&) = delete;
        move_only_function& operator=(move_only_function const&) = delete;

        move_only_function(move_only_function&& other)
            : fn_{ ::std::exchange(other.fn_, nullptr) }
            , deleter_{ ::std::exchange(other.deleter_, nullptr) }
            , ptr_{ ::std::exchange(other.ptr_, nullptr) }
        {
        }
        move_only_function& operator=(move_only_function&& other) {
            ::std::swap(fn_, other.fn_);
            ::std::swap(deleter_, other.deleter_);
            ::std::swap(ptr_, other.ptr_);
            return *this;
        }

        ~move_only_function() {
            if (deleter_) {
                deleter_(ptr_);
            }
        }

        constexpr Rt operator()(Args...vals) const {
            return fn_(ptr_, vals...);
        }

    private:
        Rt(*fn_)(void*, Args...) { nullptr };
        void(*deleter_)(void*) { nullptr };
        void* ptr_{ nullptr };
    };

    template<typename T>
    struct default_value {
        template<typename...Args>
        constexpr default_value(Args&&...values)
            : value{ static_cast<Args&&>(values)... }
        {
        }

        T value;
    };

    template<typename T>
    struct handle_wrapper : default_value<T> {};

    template<typename T>
        requires(::std::is_pointer_v<T>)
    struct handle_wrapper<T> : default_value<T> {
        using default_value<T>::default_value;

        constexpr bool operator<(handle_wrapper const& other) const noexcept {
            return this->value < other.value;
        }
        constexpr bool operator>(handle_wrapper const& other) const noexcept {
            return this->value > other.value;
        }
        constexpr bool operator<=(handle_wrapper const& other) const noexcept {
            return this->value <= other.value;
        }
        constexpr bool operator>=(handle_wrapper const& other) const noexcept {
            return this->value >= other.value;
        }

        constexpr bool operator==(handle_wrapper const& other) const noexcept {
            return this->value == other.value;
        }
        constexpr bool operator!=(handle_wrapper const& other) const noexcept {
            return this->value != other.value;
        }
    };

    template<typename T>
    struct move_only : handle_wrapper<T> {
        using base = handle_wrapper<T>;
        constexpr move_only() = default;
        template<typename...Args>
        constexpr move_only(Args&&...args)
            : base{ static_cast<Args&&>(args)... }
        {
        }

        constexpr move_only(move_only const&) = delete;
        constexpr move_only& operator=(move_only const&) = delete;

        constexpr move_only(move_only&& other) noexcept {
            this->value = ::std::exchange(other.value, {});
        }
        constexpr move_only& operator=(move_only&& other) noexcept {
            ::std::swap(value, other.value);
            return *this;
        }

        constexpr operator T& () noexcept {
            return value;
        }
        constexpr operator T const& () const noexcept {
            return value;
        }

        constexpr auto operator&() noexcept { return &value; }
        constexpr auto operator&() const noexcept { return &value; }

        T value;
    };

    template<typename T, typename U>
    concept similiar_to = ::std::same_as<::std::remove_cvref_t<T>, ::std::remove_cvref_t<U>>;



    template <typename T, typename Allocator = ::std::allocator<T>>
    class list {
        struct header {
            header* next{ this };
            header* prev{ this };
        };

        struct node : header {
            T value;

            template <typename... Args>
            constexpr explicit node(Args&&... args)
                : value(::std::forward<Args>(args)...) {
            }
        };

    public:
        using value_type = T;
        using allocator_type = Allocator;
        using size_type = ::std::size_t;
        using difference_type = ::std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = typename ::std::allocator_traits<Allocator>::pointer;
        using const_pointer = typename ::std::allocator_traits<Allocator>::const_pointer;

        template <bool IsConst>
        class iterator_impl {
        public:
            using iterator_category = ::std::bidirectional_iterator_tag;
            using iterator_concept = ::std::bidirectional_iterator_tag;
            using value_type = T;
            using difference_type = ::std::ptrdiff_t;
            using pointer = ::std::conditional_t<IsConst, const T*, T*>;
            using reference = ::std::conditional_t<IsConst, const T&, T&>;

            header* ptr_{ nullptr };

            constexpr iterator_impl() noexcept = default;
            constexpr explicit iterator_impl(header* p) noexcept : ptr_(p) {}

            template <bool OtherConst>
                requires (IsConst && !OtherConst)
            constexpr iterator_impl(const iterator_impl<OtherConst>& other) noexcept
                : ptr_(other.ptr_) {
            }

            constexpr reference operator*() const noexcept {
                return static_cast<node*>(ptr_)->value;
            }

            constexpr pointer operator->() const noexcept {
                return ::std::addressof(static_cast<node*>(ptr_)->value);
            }

            constexpr iterator_impl& operator++() noexcept {
                ptr_ = ptr_->next;
                return *this;
            }

            constexpr iterator_impl operator++(int) noexcept {
                iterator_impl tmp = *this;
                ptr_ = ptr_->next;
                return tmp;
            }

            constexpr iterator_impl& operator--() noexcept {
                ptr_ = ptr_->prev;
                return *this;
            }

            constexpr iterator_impl operator--(int) noexcept {
                iterator_impl tmp = *this;
                ptr_ = ptr_->prev;
                return tmp;
            }

            constexpr bool operator==(const iterator_impl& other) const noexcept = default;
        };

        using iterator = iterator_impl<false>;
        using const_iterator = iterator_impl<true>;
        using reverse_iterator = ::std::reverse_iterator<iterator>;
        using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;

        // Constructors & Destructor
        constexpr list() noexcept(noexcept(Allocator())) : list(Allocator()) {}

        constexpr explicit list(const Allocator& alloc) noexcept : alloc_(alloc) {}

        constexpr explicit list(size_type count, const T& value, const Allocator& alloc = Allocator())
            : alloc_(alloc) {
            insert(cend(), count, value);
        }

        constexpr explicit list(size_type count, const Allocator& alloc = Allocator())
            : alloc_(alloc) {
            for (size_type i = 0; i < count; ++i) {
                emplace_back();
            }
        }

        template <::std::input_iterator InputIt>
        constexpr list(InputIt first, InputIt last, const Allocator& alloc = Allocator())
            : alloc_(alloc) {
            insert(cend(), first, last);
        }

        constexpr list(const list& other)
            : list(other, ::std::allocator_traits<Allocator>::select_on_container_copy_construction(other.alloc_)) {
        }

        constexpr list(const list& other, const Allocator& alloc)
            : alloc_(alloc) {
            insert(cend(), other.begin(), other.end());
        }

        constexpr list(list&& other) noexcept
            : alloc_(::std::move(other.alloc_)) {
            splice_nodes(cend(), other.sentinel_.next, &other.sentinel_);
            size_ = ::std::exchange(other.size_, 0);
        }

        constexpr list(list&& other, const Allocator& alloc)
            : alloc_(alloc) {
            if (alloc_ == other.alloc_) {
                splice_nodes(cend(), other.sentinel_.next, &other.sentinel_);
                size_ = ::std::exchange(other.size_, 0);
            }
            else {
                insert(cend(), ::std::make_move_iterator(other.begin()), ::std::make_move_iterator(other.end()));
            }
        }

        constexpr list(::std::initializer_list<T> init, const Allocator& alloc = Allocator())
            : list(init.begin(), init.end(), alloc) {
        }

        constexpr ~list() {
            clear();
        }

        // Assignment
        constexpr list& operator=(const list& other) {
            if (this != &other) {
                if constexpr (::std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
                    if (alloc_ != other.alloc_) {
                        clear();
                        alloc_ = other.alloc_;
                    }
                }
                assign(other.begin(), other.end());
            }
            return *this;
        }

        constexpr list& operator=(list&& other) noexcept(
            ::std::allocator_traits<Allocator>::is_always_equal::value ||
            ::std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
            if (this != &other) {
                clear();
                if constexpr (::std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
                    alloc_ = ::std::move(other.alloc_);
                }
                if (alloc_ == other.alloc_) {
                    splice_nodes(cend(), other.sentinel_.next, &other.sentinel_);
                    size_ = ::std::exchange(other.size_, 0);
                }
                else {
                    insert(cend(), ::std::make_move_iterator(other.begin()), ::std::make_move_iterator(other.end()));
                    other.clear();
                }
            }
            return *this;
        }

        constexpr list& operator=(::std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
            return *this;
        }

        constexpr void assign(size_type count, const T& value) {
            clear();
            insert(cend(), count, value);
        }

        template <::std::input_iterator InputIt>
        constexpr void assign(InputIt first, InputIt last) {
            clear();
            insert(cend(), first, last);
        }

        constexpr void assign(::std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
        }

        // Element Access
        VKTL_NODISCARD constexpr reference front() noexcept { return static_cast<node*>(sentinel_.next)->value; }
        VKTL_NODISCARD constexpr const_reference front() const noexcept { return static_cast<node*>(sentinel_.next)->value; }
        VKTL_NODISCARD constexpr reference back() noexcept { return static_cast<node*>(sentinel_.prev)->value; }
        VKTL_NODISCARD constexpr const_reference back() const noexcept { return static_cast<node*>(sentinel_.prev)->value; }

        // Iterators
        VKTL_NODISCARD constexpr iterator begin() noexcept { return iterator(sentinel_.next); }
        VKTL_NODISCARD constexpr const_iterator begin() const noexcept { return const_iterator(sentinel_.next); }
        VKTL_NODISCARD constexpr const_iterator cbegin() const noexcept { return const_iterator(sentinel_.next); }

        VKTL_NODISCARD constexpr iterator end() noexcept { return iterator(&sentinel_); }
        VKTL_NODISCARD constexpr const_iterator end() const noexcept { return const_iterator(const_cast<header*>(&sentinel_)); }
        VKTL_NODISCARD constexpr const_iterator cend() const noexcept { return const_iterator(const_cast<header*>(&sentinel_)); }

        VKTL_NODISCARD constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
        VKTL_NODISCARD constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(cend()); }
        VKTL_NODISCARD constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

        VKTL_NODISCARD constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
        VKTL_NODISCARD constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(cbegin()); }
        VKTL_NODISCARD constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

        // Capacity
        VKTL_NODISCARD constexpr bool empty() const noexcept { return sentinel_.next == &sentinel_; }
        VKTL_NODISCARD constexpr size_type size() const noexcept { return size_; }
        VKTL_NODISCARD constexpr size_type max_size() const noexcept {
            return ::std::allocator_traits<node_allocator>::max_size(node_allocator(alloc_));
        }

        // Modifiers
        constexpr void clear() noexcept {
            header* cur = sentinel_.next;
            while (cur != &sentinel_) {
                header* next = cur->next;
                destroy_node(static_cast<node*>(cur));
                cur = next;
            }
            sentinel_.next = &sentinel_;
            sentinel_.prev = &sentinel_;
            size_ = 0;
        }

        template <typename... Args>
        constexpr iterator emplace(const_iterator pos, Args&&... args) {
            node* new_node = create_node(::std::forward<Args>(args)...);
            link_before(pos.ptr_, new_node);
            ++size_;
            return iterator(new_node);
        }

        template <typename... Args>
        constexpr reference emplace_back(Args&&... args) {
            return *emplace(cend(), ::std::forward<Args>(args)...);
        }

        template <typename... Args>
        constexpr reference emplace_front(Args&&... args) {
            return *emplace(cbegin(), ::std::forward<Args>(args)...);
        }

        constexpr iterator insert(const_iterator pos, const T& value) { return emplace(pos, value); }
        constexpr iterator insert(const_iterator pos, T&& value) { return emplace(pos, ::std::move(value)); }

        template <::std::input_iterator InputIt>
        constexpr iterator insert(const_iterator pos, InputIt first, InputIt last) {
            iterator first_inserted;
            bool has_first = false;
            for (; first != last; ++first) {
                auto it = emplace(pos, *first);
                if (!has_first) {
                    first_inserted = it;
                    has_first = true;
                }
            }
            return !has_first ? iterator(const_cast<header*>(pos.ptr_)) : first_inserted;
        }

        constexpr iterator insert(const_iterator pos, ::std::initializer_list<T> ilist) {
            return insert(pos, ilist.begin(), ilist.end());
        }

        constexpr void push_back(const T& value) { emplace_back(value); }
        constexpr void push_back(T&& value) { emplace_back(::std::move(value)); }
        constexpr void push_front(const T& value) { emplace_front(value); }
        constexpr void push_front(T&& value) { emplace_front(::std::move(value)); }

        constexpr iterator erase(const_iterator pos) noexcept {
            header* target = pos.ptr_;
            header* next_node = target->next;
            unlink(target);
            destroy_node(static_cast<node*>(target));
            --size_;
            return iterator(next_node);
        }

        constexpr iterator erase(const_iterator first, const_iterator last) noexcept {
            while (first != last) {
                first = erase(first);
            }
            return iterator(const_cast<header*>(last.ptr_));
        }

        constexpr void pop_back() noexcept { erase(--end()); }
        constexpr void pop_front() noexcept { erase(begin()); }

        constexpr void swap(list& other) noexcept(
            ::std::allocator_traits<Allocator>::is_always_equal::value ||
            ::std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
            if (this == &other) return;

            if (empty()) {
                if (!other.empty()) {
                    sentinel_.next = other.sentinel_.next;
                    sentinel_.prev = other.sentinel_.prev;
                    sentinel_.next->prev = &sentinel_;
                    sentinel_.prev->next = &sentinel_;
                    other.sentinel_.next = &other.sentinel_;
                    other.sentinel_.prev = &other.sentinel_;
                }
            }
            else if (other.empty()) {
                other.sentinel_.next = sentinel_.next;
                other.sentinel_.prev = sentinel_.prev;
                other.sentinel_.next->prev = &other.sentinel_;
                other.sentinel_.prev->next = &other.sentinel_;
                sentinel_.next = &sentinel_;
                sentinel_.prev = &sentinel_;
            }
            else {
                ::std::swap(sentinel_.next, other.sentinel_.next);
                ::std::swap(sentinel_.prev, other.sentinel_.prev);
                sentinel_.next->prev = &sentinel_;
                sentinel_.prev->next = &sentinel_;
                other.sentinel_.next->prev = &other.sentinel_;
                other.sentinel_.prev->next = &other.sentinel_;
            }

            ::std::swap(size_, other.size_);
            if constexpr (::std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
                ::std::swap(alloc_, other.alloc_);
            }
        }

        // Operations
        constexpr void splice(const_iterator pos, list& other) noexcept {
            if (!other.empty()) {
                splice(pos, other, other.begin(), other.end());
            }
        }

        constexpr void splice(const_iterator pos, list&& other) noexcept {
            splice(pos, other);
        }

        constexpr void splice(const_iterator pos, list& other, const_iterator it) noexcept {
            const_iterator next = it;
            ++next;
            if (pos == it || pos == next) return;

            header* target = it.ptr_;
            unlink(target);
            link_before(pos.ptr_, target);

            --other.size_;
            ++size_;
        }

        constexpr void splice(const_iterator pos, list&& other, const_iterator it) noexcept {
            splice(pos, other, it);
        }

        constexpr void splice(const_iterator pos, list& other, const_iterator first, const_iterator last) noexcept {
            if (first == last) return;

            size_type count = 0;
            if (this != &other) {
                for (auto it = first; it != last; ++it) ++count;
                other.size_ -= count;
                size_ += count;
            }

            splice_nodes(pos, first.ptr_, last.ptr_);
        }

        constexpr void splice(const_iterator pos, list&& other, const_iterator first, const_iterator last) noexcept {
            splice(pos, other, first, last);
        }

        VKTL_NODISCARD constexpr allocator_type get_allocator() const noexcept { return alloc_; }

    private:
        using node_allocator = typename ::std::allocator_traits<Allocator>::template rebind_alloc<node>;
        using node_traits = ::std::allocator_traits<node_allocator>;

        header sentinel_;
        size_type size_{ 0 };
        VKTL_NO_UNIQUE_ADDRESS Allocator alloc_;

        constexpr static void link_before(header* pos, header* new_node) noexcept {
            header* prev_node = pos->prev;
            new_node->next = pos;
            new_node->prev = prev_node;
            prev_node->next = new_node;
            pos->prev = new_node;
        }

        constexpr static void unlink(header* node) noexcept {
            node->prev->next = node->next;
            node->next->prev = node->prev;
        }

        constexpr static void splice_nodes(const_iterator pos, header* first, header* last) noexcept {
            header* last_node = last->prev;
            header* first_node = first;

            first_node->prev->next = last;
            last->prev = first_node->prev;

            header* pos_prev = pos.ptr_->prev;
            first_node->prev = pos_prev;
            pos_prev->next = first_node;

            last_node->next = const_cast<header*>(pos.ptr_);
            pos.ptr_->prev = last_node;
        }

        template <typename... Args>
        constexpr node* create_node(Args&&... args) {
            node_allocator n_alloc(alloc_);
            node* p = node_traits::allocate(n_alloc, 1);
            try {
                node_traits::construct(n_alloc, p, ::std::forward<Args>(args)...);
            }
            catch (...) {
                node_traits::deallocate(n_alloc, p, 1);
                throw;
            }
            return p;
        }

        constexpr void destroy_node(node* p) noexcept {
            node_allocator n_alloc(alloc_);
            node_traits::destroy(n_alloc, p);
            node_traits::deallocate(n_alloc, p, 1);
        }
    };

    // shit.
    template<typename T>
    struct safe : T {
        using value_type = typename T::value_type;

        template<typename U>
        constexpr value_type* operator[](U value) noexcept
            requires(sizeof(T) <= 16 && !::std::is_void_v<decltype(do_visit<U>(::std::declval<T&>(), value))>) {
            return do_visit<U>(*this, value);
        }
        template<typename U>
        constexpr value_type* operator[](U const& value) noexcept
            requires(sizeof(T) > 16 && !::std::is_void_v<decltype(do_visit<U const&>(::std::declval<T&>(), value))>) {
            return do_visit<U const&>(*this, value);
        }
        template<typename U>
        constexpr value_type const* operator[](U const& value) const noexcept
            requires(sizeof(T) <= 16 && !::std::is_void_v<decltype(do_visit<U>(::std::declval<T&>(), value))>) {
            return do_visit<U>(*this, value);
        }
        template<typename U>
        constexpr value_type const* operator[](U const& value) const noexcept
            requires(sizeof(T) > 16 && !::std::is_void_v<decltype(do_visit<U>(::std::declval<T&>(), value))>) {
            return do_visit<U const&>(*this, value);
        }

    private:
        template<typename U>
        static constexpr forward_like_t<value_type, U>* do_visit(T& value, U key) {
            if constexpr (requires { typename T::key_type; }) {
                auto it = value.find(key);
                if (it != value.end()) {
                    return &*it;
                }
                else {
                    return nullptr;
                }
            }
            else if constexpr (requires { value[key]; }) {
                if (value < value.size()) {
                    return &T::operator[](key);
                }
                else {
                    return nullptr;
                }
            }
        }
    };

    template<::std::size_t index, typename T>
        requires(index < tuple_size_v<T>)
    struct consume<const_index<index>, T>
        : T::template apply<tuple_at_t<index, T>, consume<const_index<index + 1u>, T>> {
        using prev = consume<const_index<index == 0 ? 0 : index - 1>, T>;
        using next = consume<const_index<index + 1u>, T>;
        using base = typename T::template apply<tuple_at_t<index, T>, consume<const_index<index + 1u>, T>>;

        static constexpr auto current_index = index;

        constexpr consume(auto&&...infos)
            : base{ forward_(infos)... }
        {
        }

    private:
        using this_t = tuple_at_t<index, T>;

        template<::std::size_t...ids>
        static constexpr auto reverse_ids(::std::index_sequence<ids...>) {
            return::std::index_sequence<(ids + index)...>();
        }

    public:
        // template<typename U>
        // static constexpr bool have_parent() {
        //     return T::template query<U>(reverse_ids(::std::make_index_sequence<tuple_size_v<T> -index - 1u>()));
        // }
        // have no idea to implment this.
        // template<typename U>
        // static constexpr bool is_derived_by() {
        //     return T::template query<U>(::std::make_index_sequence<index>());
        // }

        template<::std::size_t idx>
            requires(idx < tuple_size_v<T> -1u)
        constexpr auto& get() noexcept {
            constexpr auto cur_index = current_index - 1u;
            if constexpr (idx == cur_index) {
                return *static_cast<base*>(this);
            }
            else if constexpr (idx < cur_index) {
                return static_cast<prev*>(this)->template get<idx>();
            }
            else {
                return static_cast<next*>(this)->template get<idx>();
            }
        }
        template<::std::size_t idx>
            requires(idx < tuple_size_v<T> -1u)
        constexpr auto& get() const noexcept {
            constexpr auto cur_index = current_index - 1u;
            if constexpr (idx == cur_index) {
                return *static_cast<next const*>(this);
            }
            else if constexpr (idx < cur_index) {
                return static_cast<prev const*>(this)->template get<idx>();
            }
            else {
                return static_cast<base const*>(this)->template get<idx>();
            }
        }
    };

    // template<typename T, typename U>
    // concept parent_contains = T::template is_child_of<U>();
    // template<typename T, typename U>
    // concept child_contains = T::template is_dervied_by<U>();

    // template<typename T, typename N>
    // using seq_in = ::std::in_place_index_t<N::template current_sequence<T>>;

    template<typename T, typename Parent>
    struct trans_from_other_ {
        using type = T;
    };
    template<typename T, ::std::size_t index, typename Tp>
    struct trans_from_other_<T, consume<const_index<index>, Tp>> {
        template<::std::size_t idx, typename V>
        static auto invoke() noexcept {
            if constexpr (idx < tuple_size_v<Tp>) {
                if constexpr (requires { typename meta_of<tuple_at_t<idx, Tp>>::template transform<V>::type; }) {
                    return invoke<idx + 1, typename meta_of<tuple_at_t<idx, Tp>>::template transform<V>::type>();
                }
                else {
                    return invoke<idx + 1, V>();
                }
            }
            else {
                return::std::type_identity<V>();
            }
        }

        using type = typename decltype(invoke<1, T>())::type;
    };
    template<typename T, typename Parent>
    using transformed = typename trans_from_other_<T, Parent>::type;

    // the is_same is a binder for metaprogramming.
    template<typename T>
    struct is_same {
        template<typename V>
        struct apply : ::std::is_same<T, V> {};
    };

    template<typename T, typename Q>
    struct do_query_ : ::std::bool_constant<::std::same_as<T, Q>> {};

    // consume head.
    template<typename Base, typename...Ts>
    struct ch_ : Base {
        using prev = consume<const_index<sizeof...(Ts) - 1u>, ch_<Base, Ts...>>;
        using self = consume<const_index<1>, ch_<Base, Ts...>>;

        using types = type_list<Ts...>;

        template<typename...Rs>
        constexpr ch_(Rs&&...args)
            requires(::std::constructible_from<Base, Rs&&...>) : Base{ static_cast<Rs&&>(args)... } {}
        template<typename...Rs>
        constexpr ch_(Rs&&...) // for base is empty.
            requires(!::std::constructible_from<Base, Rs&&...>&& ::std::constructible_from<Base>) : Base{} {}

        constexpr auto&& as_self() noexcept { return *static_cast<self*>(this); }
        constexpr auto&& as_self() const noexcept { return *static_cast<self const*>(this); }
        constexpr auto as_this() noexcept { return static_cast<self*>(this); }
        constexpr auto as_this() const noexcept { return static_cast<self const*>(this); }

        template<typename T>
        static constexpr auto query() noexcept {
            return ((do_query_<T, Ts>::value) || ...);
        }

        template<typename T, ::std::size_t...ids>
        static constexpr auto query(::std::index_sequence<ids...>) noexcept {
            if constexpr (((ids < tuple_size_v<types>) && ...)) {
                return ((do_query_<T, tuple_at_t<ids, types>>::value) || ...);
            }
            else {
                static_assert(((ids < tuple_size_v<types>) && ...), "Query index is out of range.");
            }
        }

        constexpr auto& get(const const_index<2000>)
            noexcept {
            return *static_cast<prev*>(this);
        }
        constexpr auto& get(const const_index<sizeof...(Ts) - 1u>)
            const noexcept {
            return *static_cast<prev const*>(this);
        }
    };
    template<typename...Ts>
    struct tuple_like<ch_<Ts...>> : ::std::true_type {};

    template<typename Base, typename...Ts>
    using basic_consume = consume<const_index<1u>, ch_<Base, Ts...>>;

    struct e_ {};

    template<template<typename>typename Tp>
    struct one_stub {};

    template<typename T>
    struct inherit_from {};

    inline constexpr struct shared_t {} shared;

    template<typename...Ts>
    struct os_ {};

    template<typename...Ts, typename T>
    struct do_query_<os_<Ts...>, T> : ::std::bool_constant<((do_query_<Ts, T>::value) || ...)> {};

    // object node.

    template<typename Base, typename T>
    constexpr bool have_parent_v = Base::template have_parent<T>();

    template<typename Base, typename T>
    using object_parent_t = decltype(::std::declval<Base&>().template parent<T>());

    template<typename Base, typename...Ts>
    struct on_ : Base {
        constexpr void relocate() noexcept { Base::relocate(); }
        constexpr auto allocator() noexcept { return Base::allocator(); }

        constexpr on_(auto&&...infos)
            : Base{ forward_(infos)... } {
        }

        template<typename T>
        static constexpr auto local_query() noexcept {
            return ((do_query_<T, Ts>::value) || ...);
        }

        constexpr auto&& as_local() noexcept {
            return Base::template get<Base::current_index - 2u>();
        }
        constexpr auto&& as_local() const noexcept {
            return Base::template get<Base::current_index - 2u>();
        }

        template<typename T>
        constexpr auto parent() noexcept {
            if constexpr (Base::template local_query<T>()) {
                return this->parent();
            }
            else {
                return Base::template parent<T>();
            }
        }

        template<typename T>
        constexpr auto parent() const noexcept {
            if constexpr (Base::template local_query<T>()) {
                return this->parent();
            }
            else {
                return Base::template parent<T>();
            }
        }

        template<typename T>
        static constexpr auto have_parent() noexcept {
            return!::std::is_null_pointer_v<object_parent_t<Base, T>>;
        }

        constexpr auto parent() const noexcept {
            return static_cast<Base const*>(this);
        }
        constexpr auto parent() noexcept {
            return static_cast<Base*>(this);
        }

        static constexpr auto send(auto&&...args) {
            if constexpr (requires { Base::send(forward_(args)...); }) {
                return Base::send(forward_(args)...);
            }
        }
        static constexpr auto receive(auto&&...args) {
            if constexpr (requires { Base::receive(forward_(args)...); }) {
                return Base::receive(forward_(args)...);
            }
        }
    };
    template<typename...Ts>
    struct tuple_like<on_<Ts...>> : ::std::true_type {};



    enum notify_property_t : ::std::uint32_t {
        notify_prior = 0x0u,
        notify_later = 0x1u << 0u,
        notify_one_time = 0x1u << 1u,
    };
    // object notifier.
    struct o_n_ {
        ::std::uint32_t property;
        ::std::uint32_t type_id;
        void* ptr;
        void(*fn)(void*, void*, void*); // ptr, event_ptr.
    };

    template<>
    struct meta_of<error> {
        static constexpr auto type_id = make_type_id(COMMON_SCOPE, 0x5f22u);
        static constexpr auto name = fixed_string{ "error" };
        using extend = void;
    };

    template<typename Base = e_>
    struct o_ {
    private:
        template<typename T, typename N>
        struct a_ { using type = typename meta_of<T>::template make<N>; };
        template<typename...Ts, typename N>
        struct a_<os_<Ts...>, N> { using type = consume<const_index<1u>, on_<N, Ts...>>; };

    public:
        template<typename T, typename N>
        using apply = typename a_<T, N>::type;

        static constexpr void add_ref() noexcept {}
        static constexpr void release() noexcept {}

        static constexpr void receive(auto&&...) noexcept {}
        static constexpr void send(auto&&...) noexcept {}
        static constexpr void relocate() noexcept {}

        template<typename T>
        static constexpr auto parent() noexcept { return nullptr; }
        static constexpr auto parent() noexcept { return nullptr; }
        static constexpr auto allocator() noexcept { return nullptr; }

        template<typename T>
        static constexpr auto local_query() noexcept { return false; }
    };

    template<typename Base, typename...Ts>
    struct o_deduce {};
    template<typename Base>
    struct o_deduce<Base> { using type = Base; };

    template<typename Base>
    struct o_deduce<ch_<o_<Base>>> {}; // not allow empty objects


    template<typename...Ts>
    struct o_deduce_remove_no_make {};
    template<typename...Ts>
    struct o_deduce_remove_no_make<os_<Ts...>> { using type = os_<Ts...>; };
    template<typename...Ts, typename F, typename...Us>
    struct o_deduce_remove_no_make<os_<Ts...>, F, Us...> : o_deduce_remove_no_make<os_<Ts...>, Us...> {};
    template<typename...Ts, typename F, typename...Us>
        requires(requires { one_stub<meta_of<F>::template make>(); })
    struct o_deduce_remove_no_make<os_<Ts...>, F, Us...> : o_deduce_remove_no_make<os_<Ts..., F>, Us...> {};

    template<typename Base, typename...Ts>
    struct infomation;

    template<template<typename...>typename Base, typename...Ts, typename B, typename...Us, typename...Os>
    struct o_deduce<Base<Ts...>, infomation<B, Us...>, Os...>
        : o_deduce<Base<Ts..., typename o_deduce_remove_no_make<os_<>, Us...>::type>, Os...> {
    };

    template<typename Type>
    struct objects : consume<const_index<1u>, Type> {
        using base = consume<const_index<1u>, Type>;
        using object_tag = void;

        template<typename...Args>
        constexpr objects(Args&&...infos)
            : base{ forward_(infos)... } {
        }

        constexpr objects(objects&& other)
            noexcept(::std::is_nothrow_move_constructible_v<Type>)
            requires(::std::is_move_constructible_v<Type>)
        : base{ ::std::move(other) } {
            base::relocate();
        }
        constexpr objects& operator=(objects&& other)
            noexcept(::std::is_nothrow_move_assignable_v<Type>)
            requires(::std::is_move_assignable_v<Type>) {
            static_cast<base&>(*this) = ::std::move(other);
            base::relocate();
        }

        constexpr objects(objects const&) = delete;
        constexpr objects& operator=(objects const&) = delete;

        template<typename E, typename C>
        auto notify(C& object, ::std::uint32_t property = notify_later | notify_one_time)
            requires (requires { object.send(::std::declval<E&>()); }) {
            listeners_.emplace(meta_of<E>::type_id, o_n_{ property,
                meta_of<C>::type_id, &object,
                [](void* ptr, void* pthis, void* event_ptr) {
                    static_cast<C*>(ptr)->receive(*static_cast<E*>(event_ptr), *static_cast<objects*>(pthis));
                }
                });
        }

        template<typename E>
        void send(E event) {
            try {
                no_catch(this, listeners_, event, notify_prior);
                base::send(event);
                no_catch(this, listeners_, event, notify_later);
            }
            catch (error e) {
                no_catch(this, listeners_, e, notify_prior);
                base::send(e);
                no_catch(this, listeners_, e, notify_later);
                if (e.code != VK_ VK_SUCCESS) {
                    throw e;
                }
            }
        }

    private:
        template<typename E>
        static void no_catch(auto pthis, auto& list, E& event, notify_property_t property) {
            for (auto [begin, end] = list.equal_range(meta_of<E>::type_id);
                begin != end;) {
                auto& [_, on] = *begin;
                if ((on.property & property) != 0) {
                    on.fn(on.ptr, pthis, &event);
                    if (property & notify_one_time) {
                        begin = list.erase(begin);
                    }
                    else {
                        begin++;
                    }

                    if constexpr (similiar_to<E, error>) {
                        if (event.code == VK_ VK_SUCCESS) {
                            return;
                        }
                    }
                }
                else {
                    begin++;
                }
            }
        }

    private:
        ::std::unordered_multimap<::std::uint32_t, o_n_> listeners_;
    };
    template<typename...Ts>
    objects(Ts&&...) -> objects<typename o_deduce<ch_<o_<e_>>, ::std::remove_cvref_t<Ts>...>::type>;

    template<typename T>
    concept is_object = requires { typename::std::remove_cvref_t<T>::object_tag; };
    template<typename T, typename...Ts>
    concept local_contains = (... && (T::template local_query<Ts>()));

    template<typename T>
    struct skipped_make : T {
        constexpr skipped_make(auto&&, auto&&...infos)
            : T{ forward_(infos)... } {
        }
    };

    template<typename Base>
    struct i_ : Base {
        template<typename T, typename N>
        using apply = typename meta_of<T>::template info<N>;

        using Base::Base;

        static constexpr void connect(auto&&) noexcept {}
        static constexpr void relocate() noexcept {}
        static constexpr void set_connectable() noexcept {}
    };


    template<typename Base, typename...Ts>
    struct infomation
        : basic_consume<i_<Base>, Ts...> {
        using infomation_tag = void;

        using consume = basic_consume<i_<Base>, Ts...>;

        constexpr infomation(auto...infos)
            : consume{ ::std::tuple(::std::move(infos)...) } {
        }
        constexpr infomation(inherit_from<Base>, auto...rs)
            : consume{ ::std::tuple(::std::move(rs)...) } {
        }

        constexpr infomation(infomation const& other)
            noexcept(::std::is_nothrow_copy_constructible_v<consume>)
            requires(::std::is_copy_constructible_v<consume>)
        : consume{ other } {
            consume::relocate();
        }

        constexpr infomation& operator=(infomation const& other)
            noexcept(::std::is_nothrow_copy_assignable_v<consume>)
            requires(::std::is_copy_assignable_v<consume>) {
            static_cast<consume&>(*this) = other;
            consume::relocate();
        }

        constexpr infomation(infomation&& other)
            noexcept(::std::is_nothrow_move_constructible_v<consume>)
            requires(::std::is_move_constructible_v<consume>)
        : consume{ ::std::move(other) } {
            consume::relocate();
        }

        constexpr infomation& operator=(infomation&& other)
            noexcept(::std::is_nothrow_move_assignable_v<consume>)
            requires(::std::is_move_assignable_v<consume>) {
            static_cast<consume&>(*this) = ::std::move(other);
            consume::relocate();
        }
    };
    template<typename...Ts>
    infomation(Ts...) -> infomation<e_, Ts...>;
    template<typename Base, typename...Ts>
    infomation(inherit_from<Base>, Ts...) -> infomation<Base, Ts...>;
    template<typename...Ts>
    struct tuple_like<infomation<Ts...>> : ::std::true_type {};

    inline constexpr struct relocate_t {
        constexpr void impl() const {}
        constexpr void impl(auto& val, auto&...vals) const {
            if constexpr (requires { invoke(*this, val); }) {
                invoke(*this, val);
            }
            else {
                val.relocate();
            }
            impl(vals...);
        }

        constexpr void operator()(auto&...vals) const noexcept {
            ((impl(vals)), ...);
        }
    } relocate;

    struct setup_backward_t {
        constexpr void operator()(auto& info) const {}

        template<typename Info, typename First, typename... Infos>
        constexpr void operator()(Info& info, First& first, Infos&... infos) const {
            (*this)(info, infos...);
            (*this)(first, infos...);
            if constexpr (requires { invoke(*this, info, first); }) {
                invoke(*this, info, first);
            }
            else if constexpr (requires { info.setup_backward(first); }) {
                info.setup_backward(first);
            }
            else if constexpr (requires { info.setup(first); }) {
                info.setup(first);
            }
        }
    };
    inline constexpr setup_backward_t setup_backward{};

    struct setup_forward_t {
        constexpr void operator()(auto& info) const {}

        template<typename Info, typename First, typename... Infos>
        constexpr void operator()(Info& info, First& first, Infos&... infos) const {
            (*this)(info, infos...);
            (*this)(first, infos...);
            if constexpr (requires { invoke(*this, info, first); }) {
                invoke(*this, info, first);
            }
            else if constexpr (requires { info.setup_forward(first); }) {
                info.setup_forward(first);
            }
        }
    };
    inline constexpr setup_forward_t setup_forward{};

    struct connect_backward_t {
        constexpr void operator()(auto& info) const {}

        template<typename Info, typename First, typename... Infos>
        constexpr void operator()(Info& info, First& first, Infos&... infos) const {
            (*this)(info, infos...);
            (*this)(first, infos...);
            if constexpr (requires { invoke(*this, info, first); }) {
                invoke(*this, info, first);
            }
            else if constexpr (requires { info.connect_backward(first); }) {
                info.connect_backward(first);
            }
        }
    };
    inline constexpr connect_backward_t connect_backward{};

    struct connect_forward_t {
        constexpr void operator()(auto& info) const {}

        template<typename Info, typename First, typename... Infos>
        constexpr void operator()(Info& info, First& first, Infos&... infos) const {
            if constexpr (requires { invoke(*this, info, first); }) {
                invoke(*this, info, first);
            }
            else if constexpr (requires { info.connect_forward(first); }) {
                info.connect_forward(first);
            }
            else if constexpr (requires { info.connect(first); }) {
                info.connect(first);
            }
            (*this)(info, infos...);
            (*this)(first, infos...);
        }
    };
    inline constexpr connect_forward_t connect_forward{};

    struct finalize_forward_t {
        constexpr void operator()(auto& info) const {}

        template<typename Info, typename First, typename... Infos>
        constexpr void operator()(Info& info, First& first, Infos&... infos) const {
            if constexpr (requires { invoke(*this, info, first); }) {
                invoke(*this, info, first);
            }
            else if constexpr (requires { info.finalize_forward(first); }) {
                info.finalize_forward(first);
            }
            else if constexpr (requires { info.finalize(first); }) {
                info.finalize(first);
            }
            (*this)(info, infos...);
            (*this)(first, infos...);
        }
    };
    inline constexpr finalize_forward_t finalize_forward{};

    struct finalize_backward_t {
        constexpr void operator()(auto& info) const {}

        template<typename Info, typename First, typename... Infos>
        constexpr void operator()(Info& info, First& first, Infos&... infos) const {
            (*this)(info, infos...);
            (*this)(first, infos...);
            if constexpr (requires { invoke(*this, info, first); }) {
                invoke(*this, info, first);
            }
            else if constexpr (requires { info.finalize_backward(first); }) {
                info.finalize_backward(first);
            }
        }
    };
    inline constexpr finalize_backward_t finalize_backward{};

    struct set_connectable_t {
        constexpr void operator()() const {}

        template<typename First, typename... Infos>
        constexpr void operator()(First& first, Infos&... infos) const {
            if constexpr (requires { invoke(*this, first); }) {
                invoke(*this, first);
            }
            else if constexpr (requires { first.set_connectable(); }) {
                first.set_connectable();
            }
            else if constexpr (requires { first.set_connectable(true); }) {
                first.set_connectable(true);
            }
            (*this)(infos...);
        }
    };
    inline constexpr set_connectable_t set_connectable{};

    inline constexpr struct connecter_ {
        static constexpr relocate_t          relocate{};
        static constexpr setup_backward_t    setup_backward{};
        static constexpr setup_forward_t     setup_forward{};
        static constexpr connect_backward_t  connect_backward{};
        static constexpr connect_forward_t   connect_forward{};
        static constexpr finalize_forward_t  finalize_forward{};
        static constexpr finalize_backward_t finalize_backward{};
        static constexpr set_connectable_t   set_connectable{};

        constexpr void operator()(auto&...all) const {
            setup_backward(all...);
            setup_forward(all...);
            connect_backward(all...);
            connect_forward(all...);
            finalize_forward(all...);
            finalize_backward(all...);
            relocate(all...);
        }
    } connect{};
    template<typename T, typename Q>
        requires(T::template query<Q>())
    struct do_query_<T, Q> : ::std::bool_constant<T::template query<Q>()> {};

    template<typename...Ts>
    struct infos : ::std::tuple<Ts...> {
        using base = ::std::tuple<Ts...>;
        template<typename...Args>
        constexpr infos(Args&&...args)
            noexcept(::std::is_nothrow_constructible_v<base, Args&&...>)
            : base{ static_cast<Args&&>(args)... } {
            invoke(connect, *this);
        }

        constexpr infos(infos const&) = default;
        constexpr infos& operator=(infos const&) = default;
        constexpr infos(infos&&) = default;
        constexpr infos& operator=(infos&&) = default;

        template<typename T>
        static constexpr auto query() noexcept {
            return ((do_query_<Ts, T>::value) || ...);
        }

        template<similiar_to<infos> Self, typename T, typename...Os>
        friend constexpr auto invoke(T tag, Self&& self, Os&&...objs)
            noexcept(::std::is_nothrow_invocable_v<T, forward_like_t<Ts, Self&&>..., Os&&...>)
            requires(::std::invocable<T, forward_like_t<Ts, Self&&>..., Os&&...>) {
            return apply_back(static_cast<base&>(self),
                ::std::make_index_sequence<sizeof...(Ts)>(), tag, static_cast<Os&&>(objs)...);
        }
        template<similiar_to<infos> Self, typename T, typename Other>
        friend constexpr auto invoke(T tag, Other&& objs, Self&& self)
            noexcept(::std::is_nothrow_invocable_v<T, Other&&, forward_like_t<Ts, Self&&>...>)
            requires(::std::invocable<T, Other&&, forward_like_t<Ts, Self&&>...>) {
            return apply_front(static_cast<base&>(self),
                ::std::make_index_sequence<sizeof...(Ts)>(), tag, static_cast<Other&&>(objs));
        }

    private:
        template<typename Self, ::std::size_t...ids, typename...Args>
        static constexpr auto apply_front(Self&& self, ::std::index_sequence<ids...>, auto fn, Args&&...args) {
            return fn(get<ids>(static_cast<Self&&>(self))..., static_cast<Args&&>(args)...);
        }
        template<typename Self, ::std::size_t...ids, typename...Args>
        static constexpr auto apply_back(Self&& self, ::std::index_sequence<ids...>, auto fn, Args&&...args) {
            return fn(static_cast<Args&&>(args)..., get<ids>(static_cast<Self&&>(self))...);
        }
    };
    template<typename...Args>
    infos(Args&&...) -> infos<::std::remove_cvref_t<Args>...>;

    template<typename...Ts>
    struct tuple_like<infos<Ts...>> : ::std::true_type {};

    template<typename...Ts, typename Q>
    struct do_query_<infos<Ts...>, Q> : ::std::bool_constant<((Ts::template query<Q>()) || ...)> {
        static_assert(always_false<Q>,
            "This assertion is for test, "
            "if trigger this assertion, please release an issue "
            "to respository and clearify the usage scenary.");
    };

    template<typename T>
    concept is_infomation = requires { typename::std::remove_cvref_t<T>::infomation_tag; };

    template<typename T, typename...Cs>
    concept contains = (... && (do_query_<T, ::std::remove_cvref_t<Cs>>::value));

    template<typename T, typename...Cs>
    concept object_of = is_object<T> && contains<T, Cs...>;
    template<typename T, typename...Cs>
    concept infomation_of = is_infomation<T> && contains<T, Cs...>;

    // Spec require not template.
    // use tuple_spec or other template binder.
    template<typename T, typename Spec, typename = void>
    struct is_specialization_of : ::std::false_type {};
    template<typename T, typename C>
    struct is_specialization_of<T, C, ::std::void_t<typename C::template apply<T>>> : C::template apply<T> {};

    template<template<typename...>typename Tp>
    struct tuple_spec {
        template<typename T>
        struct apply : ::std::false_type {};
        template<typename...Ts>
        struct apply<Tp<Ts...>> : ::std::true_type {};
    };

    template<typename T, template<typename>typename Attr>
    struct is_extend_from : ::std::false_type {};
    template<typename T, template<typename>typename Other, template<typename>typename Attr>
    struct is_extend_from<Other<T>, Attr> : is_extend_from<T, Attr> {};
    template<typename T, template<typename>typename Attr>
    struct is_extend_from<Attr<T>, Attr> : ::std::true_type {};

    template<typename T, template<typename...>typename Tp>
    concept spec_of = is_specialization_of<T, tuple_spec<Tp>>::value;

    template<typename Getter, typename Type = const char*>
    struct enumerate_ {
        template<typename T>
        static constexpr decltype(auto) atleast_range(T&& value) {
            if constexpr (::std::ranges::range<T>) {
                return static_cast<T&&>(value);
            }
            else if (!::std::is_null_pointer_v<::std::remove_cvref_t<T>>) {
                return::std::array{ static_cast<T&&>(value) };
            }
            else {
                return::std::array<Type, 0>{};
            }
        }

        template<typename R, typename T, ::std::size_t...ids1, ::std::size_t...ids2>
        static constexpr auto merge_impl(R&& rng, T&& result, ::std::index_sequence<ids1...>, ::std::index_sequence<ids2...>) {
            return::std::array{ get<ids1>(static_cast<R&&>(rng))..., get<ids2>(static_cast<T&&>(result))... };
        }

        template<typename R, typename T, ::std::size_t...ids>
        static constexpr auto merge(R&& rng, T&& result, ::std::index_sequence<ids...> id) {
            return merge_impl(static_cast<R&&>(rng), static_cast<T&&>(result), make_index_seq_of<::std::remove_cvref_t<R>>(), id);
        }

        template<typename T, typename...Rs, ::std::size_t size>
        static constexpr auto impl(::std::array<Type, size> result, T const& v, Rs const&...r) {
            if constexpr (::std::is_void_v<::std::invoke_result_t<Getter, T const&>>)
                return impl(::std::move(result), r...);
            else
                return impl(merge(atleast_range(Getter()(v)),
                    ::std::move(result), ::std::make_index_sequence<size>()), r...);
        }
        template<::std::size_t size>
        static constexpr auto impl(::std::array<Type, size> result) { return::std::move(result); }

        template<typename...Ts>
        constexpr auto operator()(Ts const&...vs) const noexcept {
            return impl(::std::array<Type, 0>(), vs...);
        }

        template<typename...Ts, ::std::size_t...ids>
        static constexpr auto from_tuples(::std::tuple<Ts...> const& vs, ::std::index_sequence<ids...>) noexcept {
            return impl(::std::array<Type, 0>{}, get<ids>(vs)...);
        }
        template<typename...Ts>
        constexpr auto operator()(::std::tuple<Ts...> const& vs) const noexcept {
            return from_tuples(vs, ::std::make_index_sequence<sizeof...(Ts)>());
        }
    };

    template<typename Getter>
    constexpr enumerate_<Getter> enumerate_chars{};

    struct popup {
        template<typename T>
        struct closure {
            friend auto operator|(VkResult result, closure self) noexcept(false) {
                if (!self.value_->handle_error(result))
                    throw error{ result, self.error_ };
            }

            T* value_;
            const char* error_;
        };

        template<typename T>
        constexpr auto operator()(T& handler) {
            return closure<T>{ &handler };
        }

        friend void operator|(VK_ VkResult result, popup self) noexcept(false) {
            if (result != VK_ VK_SUCCESS) {
                throw error{ result, self.error_ };
            }
        }

        const char* error_;
    };

    template<typename Vec, typename Callback, typename...Ts>
    VK_ VkResult invoke(Vec& vec, Callback call, Ts...val) {
        ::std::uint32_t count;

        constexpr bool return_void = ::std::is_void_v<decltype(call(val..., &count, nullptr))>;

        VK_ VkResult result = VK_ VK_SUCCESS;
        if constexpr (return_void)
            call(val..., &count, nullptr);
        else
            result = call(val..., &count, nullptr);
        if (result == VK_ VK_SUCCESS) {
            vec.resize(count);
            if constexpr (return_void)
                call(val..., &count, vec.data());
            else
                result = call(val..., &count, vec.data());
        }
        return result;
    }

    template<::std::size_t index, typename...Ts>
    constexpr auto max_api_version(::std::tuple<Ts...> const& val, ::std::uint32_t basic) {
        using types = type_list<Ts...>;
        if constexpr (index < sizeof...(Ts)) {
            using this_t = meta_of<tuple_at_t<index, types>>;
            if constexpr (requires { this_t::api_version; })
                return max_api_version<index + 1>(val, basic > this_t::api_version ? basic : this_t::api_version);
            else
                return max_api_version<index + 1>(val, basic);
        }
        else {
            return basic;
        }
    }

    template<typename T>
    struct trait_vkfn {};

    template<typename R, typename...Args>
    struct trait_vkfn<R(*)(Args...)> {
        using params_list = type_list<Args...>;
        using result_type = ::std::remove_pointer_t<tuple_at_t<sizeof...(Args) - 1u, params_list>>;
        using return_type = R;
    };


    struct extend_any {};
    template<typename...Ts>
    using extend_one_of = type_list<Ts...>;

    namespace order {
        struct at_front;
        struct at_last;
        struct at_middle;
        struct discard;
    }

    struct describe_tag {
        template<typename...Ts>
        static constexpr auto have_infos_v = ((is_specialization_of<::std::remove_cvref_t<Ts>, tuple_spec<infos>>::value) || ...);

        template<typename T, ::std::size_t...ids>
        static constexpr auto reverse(T tuple, ::std::index_sequence<ids...>) {
            return infomation{ get<tuple_size_v<T> -1u - ids>(::std::move(tuple))... };
        }

        template<typename T, typename U, ::std::size_t...lids, ::std::size_t...mids, ::std::size_t...rids>
        static constexpr auto trans_make_make(T tuple, U&& infos,
            ::std::index_sequence<lids...>, ::std::index_sequence<mids...>, ::std::index_sequence<rids...>) {
            return trans_make(::std::forward_as_tuple(get<lids>(::std::move(tuple))..., get<mids>(forward_(infos))..., get<rids>(::std::move(tuple))...),
                ::std::index_sequence<lids..., mids...>(), ::std::index_sequence<(rids + sizeof...(mids))...>()); // infos<infos<>...> is not permitted.
        }

        template<typename T, ::std::size_t...lids, ::std::size_t index, ::std::size_t...rids>
        static constexpr auto trans_make(T tuple, ::std::index_sequence<lids...> lid, ::std::index_sequence<index, rids...>) {
            if constexpr (have_infos_v<tuple_at_t<index, T>>) {
                return trans_make_make(::std::move(tuple), get<index>(::std::move(tuple)),
                    lid, ::std::make_index_sequence<tuple_size_v<tuple_at_t<index, T>>>(), ::std::index_sequence<rids...>());
            }
            else {
                return trans_make(::std::move(tuple), ::std::index_sequence<lids..., index>(), ::std::index_sequence<rids...>());
            }
        }

        template<typename T, ::std::size_t...ids>
        static constexpr auto trans_reverse(T tuple, ::std::index_sequence<ids...> id) {
            if constexpr (have_infos_v<tuple_at_t<ids, T>...>) {
                return trans_make(::std::move(tuple), ::std::index_sequence<>(), id);
            }
            else {
                return infos{ get<tuple_size_v<T> -1u - ids>(::std::move(tuple))... };
            }
        }

        template<typename...Ts>
        constexpr auto operator()(Ts&&...ts) const {
            if constexpr (((!is_infomation<Ts>) && ...)) {
                return reverse(::std::forward_as_tuple(forward_(ts)...),
                    ::std::make_index_sequence<sizeof...(Ts)>());
            }
            else if constexpr (((is_infomation<Ts>) && ...)) {
                return trans_reverse(::std::forward_as_tuple(forward_(ts)...),
                    ::std::make_index_sequence<sizeof...(Ts)>());
            }
            else {
                static_assert(always_false<type_list<Ts...>>, "Not allow use not infomation's object combine with infomation's object.");
            }
        }
    };


    template<typename...Ts>
    struct ref : ::std::tuple<Ts*...> {};
    template<typename...Ts>
    ref(Ts*...) -> ref<Ts...>;
    // open hole.
    template<typename...Ts, typename...Ps>
    struct o_deduce<ch_<Ts...>, ref<Ps...>> : o_deduce<ch_<Ts..., ref<Ps...>>> {};

    struct create_tag {

        static constexpr auto do_reloc(auto&...infos) {
            ((infos.relocate()), ...);
        }

        // for fucking EDG intellisense, fuck you all EDG.

        template<typename...Ts>
        static constexpr auto make_impl(::std::tuple<> hosts, ::std::index_sequence<>, Ts&&...infos) {
            return objects<typename o_deduce<ch_<o_<e_>>, ::std::remove_cvref_t<Ts>...>::type>
            { forward_(infos)... };
        }

        template<typename...Us, ::std::size_t...ids, typename...Ts>
        static constexpr auto make_impl(::std::tuple<Us...> hosts, ::std::index_sequence<ids...>, Ts&&...infos) {
            return objects<typename o_deduce<ch_<o_<e_>>, ::std::remove_cvref_t<Ts>..., ref<::std::remove_reference_t<Us>...>>::type>
            {forward_(infos)..., ref{ &get<ids>(::std::move(hosts))... }};
        }

        template<::std::size_t...ids, typename...Ts>
        static constexpr auto impl(auto hosts, ::std::index_sequence<ids...> id, Ts&&...infos) {
            if constexpr (sizeof...(infos)) {
                connect(infos..., get<ids>(hosts)...);
                return make_impl(::std::move(hosts), id, forward_(infos)...);
            }
            else {
                static_assert(always_false<decltype(hosts)>, "Not allow create with no infos.");
            }
        }


        template<typename HT, ::std::size_t...hids, typename ST, ::std::size_t...sids>
        static constexpr auto enumerate(HT hosts, ST infos, ::std::index_sequence<hids...> ids, ::std::index_sequence<sids...>) {
            return impl(::std::move(hosts), ids, get<sids>(::std::move(infos))...);
        }
        template<typename HT, ::std::size_t...hids, typename ST, ::std::size_t...sids, typename F>
        static constexpr auto enumerate(HT hosts, ST infos, ::std::index_sequence<hids...> hid, ::std::index_sequence<sids...> sid, F&& val, auto&&...vals) {
            if constexpr (is_object<F>) {
                return enumerate(::std::forward_as_tuple(get<hids>(::std::move(hosts))..., forward_(val))
                    , ::std::move(infos)
                    , ::std::index_sequence<0, (hids + 1)...>()
                    , sid, forward_(vals)...);
            }
            else if constexpr (is_infomation<F>) {
                return enumerate(::std::move(hosts)
                    , ::std::forward_as_tuple(forward_(val), get<sids>(::std::move(infos))...)
                    , hid, ::std::index_sequence<0, (sids + 1)...>()
                    , forward_(vals)...);
            }
            else if constexpr (requires { one_stub<meta_of<::std::remove_cvref_t<F>>::template info>{}; }) {
                return enumerate(::std::move(hosts)
                    , ::std::forward_as_tuple(infomation{ forward_(val) }, get<sids>(::std::move(infos))...)
                    , hid, ::std::index_sequence<0, (sids + 1)...>()
                    , forward_(vals)...);
            }
            else {
                static_assert(always_false<F>, "Create doesnt allow non object or non infomation's instance.");
            }
        }

        VKTL_NODISCARD constexpr auto operator()(auto&&...infos) const {
            return enumerate(::std::tuple<>(), ::std::tuple<>(), ::std::index_sequence<>(), ::std::index_sequence<>(), forward_(infos)...);
        }
    };

    template<typename Next>
    struct control_connectable : Next {
        constexpr control_connectable(auto&&...others)
            : Next{ forward_(others)... }
        {
        }

        constexpr void set_connectable(bool enabled = true) noexcept {
            connectable_ = enabled;
        }
        constexpr auto connectable() const noexcept { return connectable_; }

    private:
        bool connectable_ = false;
    };

    template<typename T>
    struct get_from_type {
        template<typename F>
        static constexpr auto&& invoke(F&& tuple) noexcept {
            return::std::get<T>(static_cast<F&&>(tuple));
        }
    };
    template<::std::size_t idx, typename T>
    struct get_from_type<consume<const_index<idx>, T>> {

        // usually call from child class, thus, idx should - 2u.
        template<typename F>
        static constexpr auto&& invoke(F&& tuple) noexcept {
            return::std::get<idx - 2u>(static_cast<F&&>(tuple));
        }

        template<is_infomation F>
        static constexpr auto&& invoke(F&& info) noexcept {
            return forward_like<F>(info.template get<idx - 2u>());
        }
    };

    template<typename F, typename T>
    constexpr auto&& get_by(T&& tuples) noexcept {
        return get_from_type<F>::invoke(forward_(tuples));
    }

    template<typename T>
    using cspan = ::std::span<T const>;

    template<::std::size_t index = 0u, typename T>
    constexpr void vk_next_modify(T*, T*) noexcept = delete;

    inline constexpr struct compare_next_ {
        // allow order is not same, but count must same.
        static constexpr bool same(void const* l, void const* r) noexcept {
            while (l) {
                auto pleft = reinterpret_cast<VK_ VkBaseInStructure const*>(l);
                auto pright = reinterpret_cast<VK_ VkBaseInStructure const*>(r);
                ::std::uint32_t same_count = 0u;
                while (pright) {
                    if (pright->sType == pleft->sType) {
                        same_count++;
                    }
                    pright = reinterpret_cast<VK_ VkBaseInStructure const*>(pright->pNext);
                }
                if (same_count != 1u) {
                    return false;
                }
                l = pleft->pNext;
            }
            return true;
        }

        template<typename T>
        static constexpr void modify(T* left, T* right) {
            if constexpr (requires { vk_next_modify<0u>(left, right); }) {
                vk_next_modify<0u>(left, right);
            }
        }
    } vk_next{};

}

VKTL_EXPORT_ namespace vktl {
    inline constexpr detail::describe_tag describe{};
    inline constexpr detail::create_tag create{};
    using detail::connect;
    using detail::set_connectable;
}

// BEGIN COMMON

VKTL_EXPORT_ namespace vktl::detail {
    struct empty_info {};

    template<typename T>
    struct staged : T {
        VK_ VkPipelineStageFlags stages;
    };
    template<typename T = empty_info>
    struct unhandled : T {
        VK_ VkAccessFlags access = VK_ VK_ACCESS_NONE;
        VK_ VkDependencyFlags dependency = VK_ VkDependencyFlags(0u);
    };

    template<typename T = empty_info>
    struct unused : T {
        view_usage::type usages = 0u;
    };

    template<typename T = empty_info>
    struct unattached : T {
        ::std::uint16_t index;
    };
    template<typename T = empty_info>
    struct unbinded : T {
        ::std::uint16_t set = invalid;
        ::std::uint16_t binding = invalid;
    };

    template<typename T = empty_info>
    struct unbinds : unbinded<T> {
        ::std::uint32_t count;
    };

    // template<typename T = empty_info>
    // struct unresolved : staged<T> {
    //     ::std::uint16_t subpass = invalid;
    // };
    template<typename T = empty_info>
    struct image_layouted : T {
        VK_ VkImageLayout layout;
    };
    // user to decide what member should it use.
    template<typename Type = ::std::size_t>
    struct range {
        union {
            Type offset;
            Type padding;
            Type begin;
        };
        union {
            Type count;
            Type size;
            Type end;
        };
    };

    template<typename T>
    using staged_access = unattached<unhandled<staged<T>>>;

    using default_buffer_access
        = staged_access<unattached<range<>>>;
    using default_image_access
        = staged_access<unattached<image_layouted<VK_ VkImageSubresourceRange>>>;

    using staged_buffer_ref
        = staged_access<handle_wrapper<VK_ VkBuffer>>;
    using staged_image_ref
        = staged_access<image_layouted<handle_wrapper<VK_ VkImage>>>;

    // handle have serial access.
    // 
    // dynamic barrier can be set according to accesses.
    // index in access means the queue family index.

    struct buffer_handle : move_only<VK_ VkBuffer> {
        using base = move_only<VK_ VkBuffer>;
        constexpr buffer_handle() = default;
        constexpr buffer_handle(VK_ VkBuffer buffer) : base{ buffer } {}

        VK_ VkSharingMode share_mode;
        VK_ VkBufferCreateFlags flags;
        list<default_buffer_access> accesses;
    };
    struct image_handle : move_only<VK_ VkImage> {
        using base = move_only<VK_ VkImage>;
        constexpr image_handle() = default;
        constexpr image_handle(VK_ VkImage image) : base{ image } {}

        VK_ VkSharingMode share_mode;
        VK_ VkImageCreateFlags flags;
        list<default_image_access> accesses;
    };

    struct image_view_handle : move_only<VK_ VkImageView> {
        using base = move_only<VK_ VkImageView>;
        constexpr image_view_handle() = default;
        constexpr image_view_handle(VK_ VkImage handle, VK_ VkImageView view)
            : base{ view }
            , handle{ handle }
        {
        }

        VK_ VkImageViewCreateFlags flags;
        move_only<VK_ VkImage> handle;
        view_usage::type usage;
        VK_ VkImageSubresourceRange range;
    };
    struct buffer_view_handle : move_only<VK_ VkBufferView> {
        using base = move_only<VK_ VkBufferView>;
        constexpr buffer_view_handle() = default;
        constexpr buffer_view_handle(VK_ VkBuffer handle, VK_ VkBufferView view)
            : base{ view }
            , handle{ handle }
        {
        }
        VK_ VkBufferViewCreateFlags flags;
        view_usage::type usage;
        move_only<VK_ VkBuffer> handle;
    };

    template<typename T = empty_info>
    struct barrier_points : T {
        VK_ VkPipelineStageFlags front_stages, trigger_stages;
        VK_ VkDependencyFlags dependencies = 0u;
    };

    template<typename T>
    using empty_apply = T;

    template<template<typename>typename Tp = empty_apply>
    struct barrier_dispatch : barrier_points<> {
        ::std::vector<Tp<VK_ VkEvent>> events;
        ::std::vector<Tp<VK_ VkMemoryBarrier>> memories;
        ::std::vector<Tp<VK_ VkBufferMemoryBarrier>> buffers;
        ::std::vector<Tp<VK_ VkImageMemoryBarrier>> images;
    };

    inline constexpr struct subres_ {
        static constexpr auto remaining_mip = VK_REMAINING_MIP_LEVELS;
        static constexpr auto remaining_arr = VK_REMAINING_ARRAY_LAYERS;

        static constexpr bool adjacent_intersect(VK_ VkImageSubresourceRange const& first, VK_ VkImageSubresourceRange const& second) noexcept {
            if (first.aspectMask != second.aspectMask) VKTL_UNLIKELY {
                return false;
            }
            const bool 
                same_layers = (first.baseArrayLayer == second.baseArrayLayer) && (first.layerCount == second.layerCount),
                adjacent_mips = same_layers && intersected(first.baseArrayLayer, first.layerCount, second.baseArrayLayer, second.layerCount),
                same_mips = (first.baseMipLevel == second.baseMipLevel) && (first.levelCount == second.levelCount),
                adjacent_layers = same_mips && intersected(first.baseArrayLayer, first.layerCount, second.baseArrayLayer, second.layerCount);
            return adjacent_mips || adjacent_layers;
        }

        static constexpr auto merge(
            VK_ VkImageSubresourceRange const& first,
            VK_ VkImageSubresourceRange const& second) noexcept {
            VK_ VkImageSubresourceRange result{};
            result.aspectMask = first.aspectMask | second.aspectMask;
            result.baseMipLevel = (::std::min)(first.baseMipLevel, second.baseMipLevel);
            auto 
                end1 = add(first.levelCount, first.baseMipLevel),
                end2 = add(second.levelCount, second.baseMipLevel),
                max_end = (::std::max)(end1, end2);
            result.levelCount = max_end - result.baseMipLevel;
            result.baseArrayLayer = (::std::min)(first.baseArrayLayer, second.baseArrayLayer);
            end1 = first.baseArrayLayer + first.layerCount;
            end2 = second.baseArrayLayer + second.layerCount;
            max_end = (::std::max)(end1, end2);
            result.layerCount = max_end - result.baseArrayLayer;
            return result;
        }
        
        static constexpr bool intersected(VK_ VkImageSubresourceRange const& first, VK_ VkImageSubresourceRange const& second) noexcept {
            if (!(first.aspectMask & second.aspectMask)) VKTL_UNLIKELY {
                return false;
            }

            const auto
                first_level_end = add(first.levelCount, first.baseMipLevel),
                second_level_end = add(second.levelCount, second.baseMipLevel),
                first_layer_end = add(first.layerCount, first.baseArrayLayer),
                second_layer_end = add(second.layerCount, second.baseArrayLayer);
            const auto
                level_intersects = (first.baseMipLevel < second_level_end) &&
                    (second.baseMipLevel < first_level_end),
                layer_intersects = (first.baseArrayLayer < second_layer_end) &&
                    (second.baseArrayLayer < first_layer_end);
            return level_intersects && layer_intersects;
        }

        static constexpr auto get_intersect(
            VK_ VkImageSubresourceRange const& left, VK_ VkImageSubresourceRange const& right) noexcept {

            const auto
                base_mip = (left.baseMipLevel > right.baseMipLevel) ? left.baseMipLevel : right.baseMipLevel,
                end_mip1 = (left.levelCount == remaining_mip) ? remaining_mip : left.baseMipLevel + left.levelCount,
                end_mip2 = (right.levelCount == remaining_mip) ? remaining_mip : right.baseMipLevel + right.levelCount,
                end_mip = (end_mip1 < end_mip2) ? end_mip1 : end_mip2;

            const auto
                base_arr = (left.baseArrayLayer > right.baseArrayLayer) ? left.baseArrayLayer : right.baseArrayLayer,
                end_arr1 = (left.layerCount == remaining_arr) ? remaining_arr : left.baseArrayLayer + left.layerCount,
                end_arr2 = (right.layerCount == remaining_arr) ? remaining_arr : right.baseArrayLayer + right.layerCount,
                end_arr = (end_arr1 < end_arr2) ? end_arr1 : end_arr2;

            return VK_ VkImageSubresourceRange{
                .aspectMask = left.aspectMask & right.aspectMask,
                .baseMipLevel = base_mip,
                .levelCount = (end_mip == remaining_mip) ? remaining_mip : (end_mip - base_mip),
                .baseArrayLayer = base_arr,
                .layerCount = (end_arr == remaining_arr) ? remaining_arr : (end_arr - base_arr)
            };
        }

        struct diff_img : VK_ VkImageSubresourceRange {
            using base = VK_ VkImageSubresourceRange;
            constexpr diff_img() = default;
            constexpr diff_img(VK_ VkImageAspectFlags mask,
                uint32_t base_mip, uint32_t level_count,
                uint32_t base_layer, uint32_t layer_count,
                bool is_left = true) 
                : base{
                    .aspectMask = mask,
                    .baseMipLevel = base_mip,
                    .levelCount = level_count,
                    .baseArrayLayer = base_layer,
                    .layerCount = layer_count,
                }, is_left{ is_left } {
            }

            bool is_left;
        };
        template<typename T, ::std::size_t size = 4>
        struct diffs : ::std::array<T, size> {
            ::std::size_t count;

            constexpr auto end() noexcept { return begin() + count; }
            constexpr auto end() const noexcept { return begin() + count; }
        };

        static constexpr auto empty(VK_ VkImageSubresourceRange const& value) {
            return !value.layerCount && value.levelCount;
        }

        template<typename T>
        static constexpr auto empty(range<T> const& value) {
            return !value.size;
        }


        static constexpr auto get_not_intersected(
            VK_ VkImageSubresourceRange const& first,
            VK_ VkImageSubresourceRange const& second) noexcept {
            diffs<diff_img, 4> result{};
            const auto 
                first_right = add(no_zero(first.levelCount), first.baseMipLevel),
                second_right = add(no_zero(second.levelCount), second.baseMipLevel),
                left = (::std::min)(first.baseMipLevel, second.baseMipLevel),
                right = (::std::max)(first_right, second_right),
                first_bottom = add(no_zero(first.layerCount), first.baseArrayLayer),
                second_bottom = add(no_zero(second.layerCount), second.baseArrayLayer),
                top = (::std::min)(first.baseArrayLayer, second.baseArrayLayer),
                bottom = (::std::max)(first_bottom, second_bottom),
                intersect_base_mip = (::std::max)(first.baseMipLevel, second.baseMipLevel),
                intersect_end_mip = (::std::min)(first_right, second_right),
                intersect_base_arr = (::std::max)(first.baseArrayLayer, second.baseArrayLayer),
                intersect_end_arr = (::std::min)(second_bottom, first_bottom);

            if (left < intersect_base_mip) {
                auto is_first = left == first.baseMipLevel;
                auto const& value = is_first ? first : second;
                result[result.count++] = {
                    value.aspectMask,
                    left,
                    intersect_base_mip - value.baseMipLevel,
                    value.baseArrayLayer,
                    value.layerCount,
                    is_first
                };
            }

            if (right > intersect_end_mip) {
                auto is_first = first_right == right;
                auto const& value = is_first ? first : second;
                result[result.count++] = {
                    value.aspectMask,
                    intersect_end_mip,
                    sub(right, intersect_end_mip),
                    value.baseArrayLayer,
                    value.layerCount,
                    is_first
                };
            }

            const auto intersect_mip_count 
                = sub(intersect_end_mip, intersect_base_mip);
            if (top < intersect_base_arr) {
                auto is_first = first.baseArrayLayer == top;
                auto const& value = is_first ? first : second;
                result[result.count++] = {
                    value.aspectMask,
                    intersect_base_mip,
                    intersect_mip_count,
                    value.baseArrayLayer,
                    intersect_base_arr - value.baseArrayLayer,
                    is_first
                };
            }
            if (bottom > intersect_end_arr) {
                auto is_first = first_bottom == bottom;
                auto const& value = is_first ? first : second;
                result[result.count++] = {
                    value.aspectMask,
                    intersect_base_mip,
                    intersect_mip_count,
                    intersect_end_arr,
                    sub(bottom, intersect_end_arr),
                    is_first
                };
            }

            return result;
        }

        static constexpr bool same(VK_ VkImageSubresourceRange const& left, VK_ VkImageSubresourceRange const& right) noexcept {
            return left.aspectMask == right.aspectMask
                && left.baseMipLevel == right.baseMipLevel
                && left.levelCount == right.levelCount
                && left.baseArrayLayer == right.baseArrayLayer
                && left.layerCount == right.layerCount;
        }

        template<typename T>
        static constexpr bool same(range<T> const& left, range<T> const& right) noexcept {
            return left.offset = right.offset && left.size == right.size;
        }

        template<typename T>
        static constexpr bool intersected(T left_offset, T left_size, T right_offset, T right_size) noexcept {
            return intersected(range<T>{ {left_offset}, {left_size} }, range<T>{ {right_offset}, {right_size} });
        }

        template<typename T>
        static constexpr bool intersected(range<T> left, range<T> right) noexcept {
            left.size = no_zero(left.size);
            right.size = no_zero(right.size);
            const T left_end = add(left.size, left.offset);
            const T right_end = add(right.size, right.offset);
            return (left.offset < right_end) && (right.offset < left_end);
        }

        template<typename T>
        static constexpr auto get_intersect(T left_offset, T left_size, T right_offset, T right_size) noexcept {
            return get_intersect(range<T>{ {left_offset}, {left_size} }, range<T>{ {right_offset}, {right_size} });
        }
        
        // left right only indicate left or right position of parameter, not indicate left or right range.
        template<typename T>
        static constexpr auto get_intersect(range<T> left, range<T> right) noexcept {
            const T
                intersect_offset = (::std::max)(left.offset, right.offset),
                left_end = add(no_zero(left.size), left.offset),
                right_end = add(no_zero(right.size), right.offset),
                intersect_end = (::std::min)(left_end, right_end),
                intersect_size = sub(intersect_end, intersect_offset);

            range<T> range{};
            range.offset = intersect_offset;
            range.size = intersect_size;
            return range;
        }

        template<typename T>
        struct diff_buf : range<T> {
            constexpr diff_buf() = default;
            constexpr diff_buf(::std::size_t offset, ::std::size_t size, bool is_left)
                : range<>{ {offset}, {size} } // mother fucking clang.
                , is_left{is_left}
            {}

            bool is_left;
        };

        template<typename T>
        static constexpr auto get_not_intersect(range<T> first, range<T> second) noexcept {
            diffs<diff_buf<T>, 2u> result{};
            const T
                left = (::std::min)(first.offset, second.offset),
                intersect_offset = (::std::max)(first.offset, second.offset),
                left_end = add(no_zero(left.size), left.offset),
                right_end = add(no_zero(second.size), second.offset),
                right = (::std::max)(left_end, right_end),
                intersect_end = (::std::min)(left_end, right_end);

            if (left < intersect_offset) {
                auto is_first = left == first.offset;
                auto value = is_first ? left : right;
                result[result.count++] = { left, intersect_offset - left, is_first };
            }

            if (right > intersect_end) {
                auto is_first = right == left_end;
                auto value = is_first ? left : right;
                result[result.count++] = { intersect_end, sub(right, intersect_end), is_first };
            }

            return result;
        }


        template<typename T>
        static constexpr T no_zero(T value) noexcept {
            if (value == 0u) {
                return maximum;
            }
            else {
                return value;
            }
                
        }

        template<typename T, typename F>
        static constexpr T sub(T left, F right) noexcept {
            return no_zero(right) == maximum ? 0u : no_zero(left) == maximum ? maximum : left - right;
        }
        template<typename T, typename F>
        static constexpr T add(T left, F right) noexcept {
            return no_zero(left) == maximum || no_zero(right) == maximum ? maximum : left + right;
        }
    } subres{};

    // modify left no need to increase iterator, 
    //  since it will express at back to decide emplace at back or at front.
    // express (vector, ..., iterator, ..., new_vals, ...)
    constexpr void insert_buffer_(auto policy, auto&&...values) {
        policy.skip(values...);
        while (true) {
            if (policy.insert_at_front(values...)) {
                break;
            }

            if (!policy.merge(values...)) {
                auto&& left = policy.left(values...);
                auto&& right = policy.right(values...);

                auto left_beg = left.offset;
                auto left_end = subres.add(left.size, left.offset);

                auto right_beg = right.offset;
                auto right_end = subres.add(right.size, right.offset);

                auto left_size = right_beg - left_beg;
                auto left_overlap_size = (::std::min)(subres.sub(left_end, right_beg), right_end);
                auto right_rest_size // rest of part.
                    = left_overlap_size < right.size ? subres.sub(right_end, left_end) : 0u;

                if (left_size) {
                    policy.modify_left(left_beg, left_size, values...);
                }
                else {
                    policy.erase_left(values...);
                }

                policy.insert_overlap(bool(left_size), right_beg, left_overlap_size, values...);

                if (right_rest_size) {
                    policy.insert_right(left_end, right_rest_size, values...);
                }
                else {
                    policy.modify_right(left_end, right_rest_size, values...);
                }

                if (subres.sub(right.size, subres.add(right_rest_size, subres.add(left_overlap_size, left_size))) == 0u) {
                    break;
                }
            }
        }
    }

    // express (vector, ..., iterator, ..., new_vals, ...)
    constexpr void insert_image_(auto policy, auto&&...vals) {
        policy.skip(vals...);
        if (!policy.insert_at_front(vals...) && !policy.merge(vals...)) {
            auto [left_aspect,
                left_top, left_bottom,
                left_left, left_right,
                right_aspect,
                right_top, right_bottom,
                right_left, right_right] = policy.trait(vals...);
            auto
                intersected = policy.get_overlap(vals...);
            auto
                intersect_left = intersected.baseMipLevel,
                intersect_right = intersected.levelCount,
                intersect_top = intersected.baseArrayLayer,
                intersect_bottom = intersected.layerCount;
            auto
                outer_left = (::std::min)(left_left, right_left),
                outer_right = (::std::max)(left_right, right_right),
                outer_top = (::std::min)(left_top, right_top),
                outer_bottom = (::std::max)(left_bottom, right_bottom);

            policy.erase(vals...);

            auto
                mip_start = outer_left,
                mip_end = intersect_left,
                layer_start = outer_top,
                layer_end = outer_bottom;
            if (mip_end - mip_start) {
                policy.insert(outer_left == left_left,
                    mip_start, mip_end,
                    layer_start, layer_end,
                    vals...);
            }

            mip_start = intersect_left,
                mip_end = intersect_right,
                layer_start = outer_top,
                layer_end = intersect_top;
            if (layer_end - layer_start) {
                policy.insert(outer_top == left_top,
                    mip_start, mip_end,
                    layer_start, layer_end,
                    vals...);
            }

            policy.insert_overlap(intersect_left, intersect_right,
                intersect_top, intersect_bottom, vals...);

            mip_start = intersect_left,
                mip_end = intersect_right,
                layer_start = intersect_bottom,
                layer_end = outer_bottom;
            if (layer_end - layer_start) {
                policy.insert(outer_bottom == left_bottom,
                    mip_start, mip_end,
                    layer_start, layer_end,
                    vals...);
            }

            mip_start = intersect_right,
                mip_end = outer_right,
                layer_start = outer_top,
                layer_end = outer_bottom;
            if (layer_end - layer_start) {
                policy.insert(outer_right == left_right,
                    mip_start, mip_end,
                    layer_start, layer_end,
                    vals...);
            }
        }
    }


    struct image_access_policy {
        static constexpr void handle_equal(auto it, auto const& image) noexcept {
            if constexpr (requires { image.value; }) {
                return it->value == image.value;
            }
            else {
                return it->index == image.index;
            }
        }


        static constexpr void skip(auto& access, auto& it, auto const& img) noexcept {
            while (it != access.end()
                && ((handle_equal(it, img)
                        || it->baseMipLevel < img.baseMipLevel)
                    || (handle_equal(it, img)
                        || it->baseMipLevel == img.baseMipLevel
                        || it->baseArrayLayer < img.baseArrayLayer))) {
                it++;
            }
        }

        static constexpr bool not_intersect(auto& access, auto& it, auto const& img) {
            while (it != access.end() && it->baseMipLevel < subres.add(img.levelCount, img.baseMipLevel)) {
                if (subres.intersected(*it, img)) {
                    return false;
                }
            }
            return true;
        }

        static constexpr bool insert_at_front(auto& access, auto& it, auto& img) {
            if (it == access.end()
                || !handle_equal(it, img)
                || not_intersect(access, it, img)) {
                it = access.insert(it, ::std::move(img)) + 1u;
                return true;
            }
            else {
                return false;
            }
        }

        static constexpr bool merge(auto& access, auto& it, auto& img) {
            if (img.stages == it->stages
                && it->access == img.access
                && it->dependency == img.dependency
                && it->layout == img.layout
                && it->aspectMask == img.aspectMask) {
                auto actual_mip_count = subres.no_zero(img.levelCount);
                auto actual_layer_count = subres.no_zero(img.layerCount);
                auto it_level_count = subres.no_zero(it->levelCount);
                auto it_layer_count = subres.no_zero(it->layerCount);

                auto max_mip = (it_level_count == maximum || actual_mip_count == maximum) ? maximum :
                    (::std::max)(it->baseMipLevel + it_level_count, img.baseMipLevel + actual_mip_count);
                img.baseMipLevel = (::std::min)(it->baseMipLevel, img.baseMipLevel);
                img.levelCount = subres.sub(max_mip, img.baseMipLevel);

                auto max_layer = (it_layer_count == maximum || actual_layer_count == maximum) ? maximum :
                    (::std::max)(it->baseArrayLayer + it_layer_count, img.baseArrayLayer + actual_layer_count);
                img.baseArrayLayer = (::std::min)(it->baseArrayLayer, img.baseArrayLayer);
                img.layerCount = subres.sub(max_layer, img.baseArrayLayer);

                insert_image_(image_access_policy(), access, it, ::std::move(img));
                return true;
            }
            else {
                return false;
            }
        }

        static constexpr auto get_overlap(auto& accesses, auto it, auto const& img) {
            return subres.get_intersect(*it, img);
        }

        constexpr void insert(bool left, auto mip_beg, auto mip_end, auto layer_beg, auto layer_end,
            auto& access, auto& it, auto const& img) {
            auto image = left ? temp : img;
            image.baseMipLevel = mip_beg;
            image.levelCount = subres.sub(mip_end, mip_beg);
            image.baseArrayLayer = layer_beg;
            image.layerCount = subres.sub(layer_end, layer_beg);

            insert_image_(image_access_policy(), access, it, image);
        }

        constexpr void insert_overlap(auto mip_beg, auto mip_end, auto layer_beg, auto layer_end,
            auto& access, auto& it, auto const& img) {
            auto image = temp;
            image.aspectMask |= img.aspectMask;
            image.baseMipLevel = mip_beg;
            image.levelCount = subres.sub(mip_end, mip_beg);
            image.baseArrayLayer = layer_beg;
            image.layerCount = subres.sub(layer_end, layer_beg);
            image.stages |= img.stages;
            image.access |= img.access;
            image.dependency |= img.dependency;

            insert_image_(image_access_policy(), access, it, image);
        }

        constexpr void erase(auto& access, auto& it, auto const& img) {
            temp = ::std::move(*it);
            it = access.erase(it);
        }

        static constexpr auto trait(auto& access, auto it, auto const& img) {
            return::std::tuple(it->aspectMask,
                it->baseMipLevel, subres.add(it->baseMipLevel, it->levelCount),
                it->baseArrayLayer, subres.add(it->baseArrayLayer, it->layerCount),
                img.aspectMask,
                img.baseMipLevel, subres.add(img.baseMipLevel, img.levelCount),
                img.baseArrayLayer, subres.add(img.baseArrayLayer, img.layerCount));
        }

        default_image_access temp;
    };

    //struct image_barrier_policy {
    //    static constexpr void skip(auto& points, auto& barriers,
    //        auto& itm, auto& it, auto const& point, auto const& barrier) noexcept {
    //        while (it != barriers.end()
    //            && ((itm->index < point.index
    //                || it->subresourceRange.baseMipLevel < barrier.subresourceRange.baseMipLevel)
    //                || (itm->index == point.index
    //                    && it->subresourceRange.baseMipLevel == barrier.subresourceRange.baseMipLevel
    //                    && it->subresourceRange.baseArrayLayer < barrier.subresourceRange.baseArrayLayer))) {
    //            itm++; it++;
    //        }
    //    }

    //    static constexpr bool not_intersect(auto& access, auto it, auto const& img) {
    //        while (it != access.end() && it->subresourceRange.baseMipLevel < subres.add(img.subresourceRange.levelCount, img.subresourceRange.baseMipLevel)) {
    //            if (subres.intersected(it->subresourceRange, img.subresourceRange)) {
    //                return false;
    //            }
    //        }
    //        return true;
    //    }

    //    static constexpr bool insert_at_front(auto& points, auto& barriers,
    //        auto& itm, auto& it, auto const& point, auto const& barrier) {
    //        if (it == barriers.end() || not_intersect(barriers, it, barrier)) {
    //            itm = points.insert(itm, ::std::move(point));
    //            it = barriers.insert(it, ::std::move(barrier));
    //            return true;
    //        }
    //        else {
    //            return false;
    //        }
    //    }

    //    static constexpr bool merge(auto& points, auto& barriers,
    //        auto& itm, auto& it, auto const& point, auto const& barrier) {
    //        if (it->srcAccessMask == barrier.srcAccessMask
    //            && it->dstAccessMask == barrier.dstAccessMask
    //            && it->oldLayout == barrier.oldLayout
    //            && it->newLayout == barrier.newLayout
    //            && itm->front_stages == point.front_stages
    //            && itm->trigger_stages == point.trigger_stages
    //            && it->subresourceRange.aspectMask == barrier.subresourceRange.aspectMask) {
    //            auto actual_mip_count = subres.no_zero(barrier.subresourceRange.levelCount);
    //            auto actual_layer_count = subres.no_zero(barrier.subresourceRange.layerCount);
    //            auto it_level_count = subres.no_zero(it->subresourceRange.levelCount);
    //            auto it_layer_count = subres.no_zero(it->subresourceRange.layerCount);

    //            auto max_mip = (it_level_count == maximum || actual_mip_count == maximum) ? maximum :
    //                (::std::max)(it->subresourceRange.baseMipLevel + it_level_count, barrier.subresourceRange.baseMipLevel + actual_mip_count);
    //            auto min_mip = (::std::min)(it->subresourceRange.baseMipLevel, barrier.subresourceRange.baseMipLevel);

    //            auto max_layer = (it_layer_count == maximum || actual_layer_count == maximum) ? maximum :
    //                (::std::max)(it->subresourceRange.baseArrayLayer + it_layer_count, barrier.subresourceRange.baseArrayLayer + actual_layer_count);
    //            auto min_layer = (::std::min)(it->subresourceRange.baseArrayLayer, barrier.subresourceRange.baseArrayLayer);

    //            auto merged_barrier = barrier;
    //            merged_barrier.subresourceRange.baseMipLevel = min_mip;
    //            merged_barrier.subresourceRange.levelCount = subres.sub(max_mip, min_mip);
    //            merged_barrier.subresourceRange.baseArrayLayer = min_layer;
    //            merged_barrier.subresourceRange.layerCount = subres.sub(max_layer, min_layer);

    //            auto merged_point = point;
    //            merged_point.front_stages |= itm->front_stages;
    //            merged_point.trigger_stages |= itm->trigger_stages;
    //            merged_point.dependencies |= itm->dependencies;

    //            itm = points.erase(itm);
    //            it = barriers.erase(it);

    //            insert_image_(image_barrier_policy(),
    //                points, barriers, itm, it, ::std::move(merged_point), ::std::move(merged_barrier));
    //            return true;
    //        }
    //        else {
    //            return false;
    //        }
    //    }

    //    static constexpr auto trait(auto& points, auto& barriers,
    //        auto& itm, auto& it, auto const& point, auto const& barrier) {
    //        return ::std::tuple(
    //            it->subresourceRange.aspectMask,
    //            it->subresourceRange.baseArrayLayer,
    //            subres.add(it->subresourceRange.baseArrayLayer, it->subresourceRange.layerCount),
    //            it->subresourceRange.baseMipLevel,
    //            subres.add(it->subresourceRange.baseMipLevel, it->subresourceRange.levelCount),
    //            barrier.subresourceRange.aspectMask,
    //            barrier.subresourceRange.baseArrayLayer,
    //            subres.add(barrier.subresourceRange.baseArrayLayer, barrier.subresourceRange.layerCount),
    //            barrier.subresourceRange.baseMipLevel,
    //            subres.add(barrier.subresourceRange.baseMipLevel, barrier.subresourceRange.levelCount)
    //        );
    //    }

    //    static constexpr auto get_overlap(auto& points, auto& barriers,
    //        auto& itm, auto& it, auto const& point, auto const& barrier) {
    //        return subres.get_intersect(it->subresourceRange, barrier.subresourceRange);
    //    }

    //    constexpr void erase(auto& points, auto& barriers,
    //        auto& itm, auto& it, auto const& point, auto const& barrier) {
    //        temp = ::std::move(*it);
    //        meta = ::std::move(*itm);
    //        it = barriers.erase(it);
    //        itm = points.erase(itm);
    //    }

    //    constexpr void insert(bool left, auto mip_beg, auto mip_end, auto layer_beg, auto layer_end,
    //        auto& points, auto& barriers, auto& itm, auto& it, auto const& point, auto const& barrier) {

    //        auto b = left ? temp : barrier;
    //        auto p = left ? meta : point;

    //        b.subresourceRange.baseMipLevel = mip_beg;
    //        b.subresourceRange.levelCount = subres.sub(mip_end, mip_beg);
    //        b.subresourceRange.baseArrayLayer = layer_beg;
    //        b.subresourceRange.layerCount = subres.sub(layer_end, layer_beg);

    //        insert_image_(image_barrier_policy(),
    //            points, barriers, itm, it, ::std::move(p), ::std::move(b));
    //    }

    //    constexpr void insert_overlap(auto mip_beg, auto mip_end, auto layer_beg, auto layer_end,
    //        auto& points, auto& barriers, auto& itm, auto& it, auto const& point, auto const& barrier) {

    //        auto b = temp;
    //        b.subresourceRange.aspectMask |= barrier.subresourceRange.aspectMask;
    //        b.subresourceRange.baseMipLevel = mip_beg;
    //        b.subresourceRange.levelCount = subres.sub(mip_end, mip_beg);
    //        b.subresourceRange.baseArrayLayer = layer_beg;
    //        b.subresourceRange.layerCount = subres.sub(layer_end, layer_beg);
    //        b.srcAccessMask |= barrier.srcAccessMask;
    //        b.dstAccessMask |= barrier.dstAccessMask;

    //        auto p = meta;
    //        p.front_stages |= point.front_stages;
    //        p.trigger_stages |= point.trigger_stages;
    //        p.dependencies |= point.dependencies;

    //        insert_image_(image_barrier_policy(),
    //            points, barriers, itm, it, ::std::move(p), ::std::move(b));
    //    }

    //    VK_ VkImageMemoryBarrier temp;
    //    unattached<barrier_points> meta;
    //};

    struct image_barrier_policy {

    };

    template <typename T>
    struct buffer_policy : T {
        static constexpr auto get_range(auto const& buf) {
            range<empty_info, decltype(buf.offset)> val{};
            val.offset = buf.offset;
            val.size = buf.size;
            return val;
        }
        static constexpr auto left(auto& vec, auto& it, auto const& buf) noexcept {
            return get_range(*it);
        }
        static constexpr auto right(auto& vec, auto& it, auto const& buf) noexcept {
            return get_range(buf);
        }

        static constexpr void skip(auto& vec, auto& it, auto const& buf) noexcept {
            while (it != vec.end() && T::compare_less(*it, buf)) {
                it++;
            }
        }

        static constexpr bool insert_at_front(auto& vec, auto it, auto& buf) {
            if (it == vec.end() || (!T::has_intersection(*it, buf) && (it + 1 != vec.end() && !T::has_intersection(*(it + 1u), buf)))) {
                vec.insert(it + bool(it != vec.end()), ::std::move(buf));
                return true;
            }
            return false;
        }

        static constexpr bool merge(auto& vec, auto& it, auto& buf) {
            if (T::can_merge(*it, buf)) {
                auto end = (::std::max)(subres.add(buf.size, buf.offset), subres.add(it->size, it->offset));
                auto begin = (::std::min)(it->offset, buf.offset);

                buf.offset = begin;
                buf.size = subres.sub(end, begin);

                it = vec.erase(it);
                return true;
            }
            return false;
        }

        static constexpr auto modify(auto begin, auto count, auto& access, auto& it, auto& buf) {
            it->offset = begin;
            it->size = count;
        }

        constexpr auto modify_left(auto begin, auto count, auto& vec, auto& it, auto& buf) {
            modify(begin, count, vec, it, buf);
            T::temp = *it;
        }

        static constexpr auto erase(auto& access, auto& it, auto& buf) {
            it = access.erase(it);
        }

        constexpr auto erase_left(auto& vec, auto& it, auto& buf) {
            T::temp = ::std::move(*it);
            erase(vec, it, buf);
        }

        constexpr auto insert_overlap(bool at_back, auto begin, auto count, auto& vec, auto& it, auto& buf) {
            auto slice = T::temp;
            slice.offset = begin;
            slice.size = count;
            T::merge_attributes(slice, buf);
            it = vec.insert(it + at_back, ::std::move(slice));
        }

        static constexpr auto modify_right(auto begin, auto count, auto& vec, auto& it, auto& buf) {
            modify(begin, count, vec, it, buf);
            it++;
        }

        constexpr auto insert_right(auto begin, auto count, auto& vec, auto& it, auto& buf) {
            T::temp.offset = begin;
            T::temp.size = count;
            vec.insert(it + 1u, ::std::move(buf));
        }
    };

    struct buffer_insert_access {
        static constexpr void handle_equal(auto it, auto const& handle) noexcept {
            if constexpr (requires { handle.value; }) {
                return it->value == handle.value;
            }
            else {
                return it->index == handle.index;
            }
        }
        static constexpr void handle_less(auto it, auto const& handle) noexcept {
            if constexpr (requires { handle.value; }) {
                return it->value < handle.value;
            }
            else {
                return it->index < handle.index;
            }
        }
        static constexpr bool compare_less(auto const& lhs, auto const& rhs) noexcept {
            return handle_less(lhs, rhs) || (handle_equal(lhs, rhs) && lhs.offset + lhs.size < rhs.offset);
        }
        static constexpr bool has_intersection(auto const& lhs, auto const& rhs) noexcept {
            return handle_equal(lhs, rhs) && subres.intersected(lhs.offset, lhs.size, rhs.offset, rhs.size);
        }
        static constexpr bool can_merge(auto const& lhs, auto const& rhs) noexcept {
            return lhs.stages == rhs.stages && lhs.vec == rhs.vec && lhs.dependency == rhs.dependency;
        }
        static constexpr void merge_attributes(auto& slice, auto const& buf) noexcept {
            slice.stages |= buf.stages;
            slice.vec |= buf.vec;
            slice.dependency |= buf.dependency;
        }

        default_buffer_access temp;
    };
    using buffer_access_policy = buffer_policy<buffer_insert_access>;


    struct buffer_barrier_policy {

    };


    //struct buffer_barrier_policy {
    //    static constexpr auto get_range(auto const& buf) {
    //        range<> val{};
    //        val.offset = buf.offset;
    //        val.size = buf.size;
    //        return val;
    //    }
    //    static constexpr auto left(auto& points, auto& it, auto const& point, auto const& barrier) noexcept {
    //        return get_range(*it);
    //    }
    //    static constexpr auto right(auto& points, auto& it, auto const& point, auto const& barrier) noexcept {
    //        return get_range(barrier);
    //    }
    //    static constexpr void skip(auto& points, auto& it, auto const& point, auto const& barrier) noexcept {
    //        while (it != points.end() && (it->index < point.index || it->offset < barrier.offset)) { it++; }
    //    }

    //    static constexpr bool not_intersect(auto it, auto const& point, auto const& buf) {
    //        return itm->index != point.index || !subres.intersected(it->offset, it->size, buf.offset, buf.size);
    //    }

    //    constexpr bool insert_at_front(auto& points, auto& barriers, auto& itm, auto& it, auto& point, auto& barrier) {
    //        if (is_duplicated) { // TODO: if totally same, no insert.
    //            return true;
    //        }
    //        else if (it == barriers.end()
    //            || (not_intersect(itm, it, point, barrier)
    //                && (it + 1 != barriers.end() && not_intersect(itm + 1u, it + 1u, point, barrier)))) {
    //            points.insert(itm, ::std::move(point));
    //            barriers.insert(it, ::std::move(barrier));
    //            return true;
    //        }
    //        else {
    //            return false;
    //        }
    //    }

    //    static constexpr bool merge(auto& points, auto& barriers, auto& itm, auto& it, auto& point, auto& barrier) {
    //        if (itm->index == point.index
    //            && (it->offset + it->size == barrier.offset)
    //            && (it->srcAccessMask == barrier.srcAccessMask)
    //            && (it->dstAccessMask == barrier.dstAccessMask)) {

    //            point.front_stages |= itm->front_stages;
    //            point.trigger_stages |= itm->trigger_stages;
    //            if (barrier.size != maximum && it->size != maximum) {
    //                barrier.size = it->size + barrier.size - (barrier.offset - it->offset - it->size);
    //            }
    //            else {
    //                barrier.size = maximum;
    //            }
    //            barrier.offset = it->offset;

    //            itm = points.erase(itm);
    //            it = barriers.erase(it);
    //            return true;
    //        }
    //        else {
    //            return false;
    //        }
    //    }

    //    constexpr auto modify_left(auto begin, auto count,
    //        auto& points, auto& barriers, auto& itm, auto& it, auto const& point, auto const& barrier) {
    //        it->offset = begin;
    //        it->size = count;
    //        temp = *it;
    //        meta = *itm;
    //    }
    //    constexpr auto erase_left(auto& points, auto& barriers,
    //        auto& itm, auto& it,
    //        auto const& point, auto const& barrier) {
    //        temp = ::std::move(*it);
    //        meta = ::std::move(*itm);

    //        it = barriers.erase(it);
    //        itm = points.erase(itm);
    //    }

    //    constexpr auto insert_overlap(bool at_back, auto begin, auto count,
    //        auto& points, auto& barriers,
    //        auto& itm, auto& it, auto const& point, auto const& barrier) {
    //        auto slice = temp;
    //        slice.offset = begin;
    //        slice.size = count;
    //        slice.srcAccessMask |= barrier.srcAccessMask;
    //        slice.dstAccessMask |= barrier.dstAccessMask;

    //        auto smeta = meta;
    //        smeta.front_stages |= point.front_stages;
    //        smeta.trigger_stages |= point.trigger_stages;
    //        smeta.dependencies |= point.dependencies;

    //        it = barriers.insert(it + bool(at_back), ::std::move(slice));
    //        itm = points.insert(itm + bool(at_back), ::std::move(smeta));
    //    }

    //    constexpr auto modify_right(auto begin, auto count,
    //        auto& points, auto& barriers,
    //        auto& itm, auto& it, auto& point, auto& barrier) {
    //        barrier.offset = begin;
    //        barrier.size = count;
    //        it++;
    //        itm++;
    //    }
    //    constexpr auto insert_right(auto begin, auto count,
    //        auto& points, auto& barriers, auto& itm, auto& it, auto& point, auto& barrier) {
    //        temp.offset = begin;
    //        temp.size = count;

    //        it = barriers.insert(it + 1u, ::std::move(temp));
    //        itm = points.insert(itm + 1u, ::std::move(meta));
    //    }

    //    VK_ VkBufferMemoryBarrier temp;
    //    unattached<barrier_points> meta;
    //    bool is_duplicated = false;
    //};

    template<typename T>
    struct combine_sampler : T {
        ::std::uint16_t sampler_index = invalid;
        ::std::uint16_t attribute = 0u;
    };

    template<typename T>
    auto& inplace_unique(::std::vector<T>& vals) { // TODO: might need to remove sort.
        ::std::ranges::sort(vals);
        vals.erase(::std::unique(vals.begin(), vals.end()), vals.end());
        return vals;
    }

    struct binding_block_info {
        ::std::vector<::std::uint16_t> ori_indices; // original view create info index.
        ::std::vector<::std::uint16_t> sampler_indices; // static samplers.

        constexpr bool operator==(binding_block_info const& other) const noexcept {
            if (other.ori_indices.size() != ori_indices.size()
                || other.sampler_indices.size() != sampler_indices.size()) {
                return false;
            }
            else {
                const bool compare_sampler = sampler_indices.size();
                auto itoo = other.ori_indices.begin();
                auto itos = other.sampler_indices.begin();
                auto ito = ori_indices.begin();
                auto its = sampler_indices.begin();
                for (; ito != ori_indices.end(); itoo++, ito++) {
                    if (*ito != *itoo) {
                        return false;
                    }
                    if (compare_sampler) {
                        if (*itos != *its) {
                            return false;
                        }
                        itos++; its++;
                    }
                }
                return true;
            }
        }
    };

    struct descriptor_policy {
        binding_block_info meta;
        VK_ VkDescriptorSetLayoutBinding temp;

        static constexpr auto left(auto& binds, auto& metas,
            auto& itb, auto& itm, auto& bind, auto const& meta) {
            range<empty_info, ::std::uint32_t> r{};
            r.offset = itb->binding;
            r.count = itb->descriptorCount;
            return r;
        }

        static constexpr auto right(auto& binds, auto& metas,
            auto& itb, auto& itm, auto& bind, auto const& meta) {
            range<empty_info, ::std::uint32_t> r{};
            r.offset = bind.binding;
            r.count = bind.descriptorCount;
            return r;
        }

        static constexpr void skip(auto& binds, auto& metas,
            auto& itb, auto& itm, auto& bind, auto const& meta) {
            while (itb != binds.end() && itb->binding < bind.binding) {
                itb++;
                itm++;
            }
        }

        static constexpr bool not_intersected(auto it, auto const& bind) {
            return !subres.intersected(bind.binding, bind.descriptorCount, it->binding, it->descriptorCount);
        }

        static constexpr bool insert_at_front(auto& binds, auto& metas,
            auto& itb, auto& itm, auto& bind, auto& meta) {
            if (itb == binds.end() ||
                (not_intersected(itb, bind)
                    && (bind.binding >= itb->binding && itb + 1 != binds.end() && not_intersected(itb, bind)))) {
                itb = binds.insert(itb, bind);
                itm = metas.insert(itm, ::std::move(meta));
                return true;
            }
            else {
                return false;
            }
        }

        constexpr bool merge(auto& binds, auto& metas, auto& itb, auto& itm, auto& bind, auto& meta) {
            if ((itb->stageFlags & bind.stageFlags) == bind.stageFlags
                && !(meta.sampler_indices.size() ^ itm->sampler_indices.size())) {
                auto end = (::std::max)(bind.binding + bind.descriptorCount, itb->binding + itb->descriptorCount);
                auto begin = (::std::min)(bind.binding, itb->binding);
                bind.binding = begin;
                bind.descriptorCount = end - begin;

                ::std::move(itm->sampler_indices.begin(), itm->sampler_indices.end(), meta.sampler_indices.begin());
                ::std::move(itm->ori_indices.begin(), itm->ori_indices.end(), meta.ori_indices.begin());

                itb = binds.erase(itb);
                itm = metas.erase(itm);
                return true;
            }
            else {
                return false;
            }
        }

        constexpr void modify_left(auto begin, auto count,
            auto& binds, auto& metas, auto& itb, auto& itm, auto& bind, auto& meta) {
            temp = *itb;

            itb->binding = begin;
            itb->descriptorCount = count;

            ::std::move(itm->ori_indices.begin() + count, itm->ori_indices.end(),
                this->meta.ori_indices.begin());
            itm->ori_indices.resize(count);

            if (itm->sampler_indices.size()) {
                ::std::move(itm->sampler_indices.begin() + count, itm->sampler_indices.end(),
                    this->meta.sampler_indices.begin());
                itm->sampler_indices.resize(count);
            }
        }

        constexpr void erase_left(auto& binds, auto& metas,
            auto& itb, auto& itm, auto& bind, auto& meta) {
            temp = ::std::move(*itb);
            meta = ::std::move(*itm);
            itb = binds.erase(itb);
            itm = metas.erase(itm);
        }

        constexpr void insert_overlap(bool at_back, auto begin, auto count,
            auto& binds, auto& metas, auto& itb, auto& itm, auto& bind, auto& meta) {
            auto slice = ::std::move(temp);
            slice.binding = begin;
            slice.descriptorCount = count;
            if (meta == invalid
                && bind.descriptorType == VK_ VK_DESCRIPTOR_TYPE_SAMPLER
                && temp.descriptorType == VK_ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                slice.descriptorType = bind.descriptorType;
            }
            slice.stageFlags |= temp.stageFlags;

            itb = binds.insert(itb + at_back, ::std::move(slice));

            binding_block_info meta_info{};
            ::std::move(meta.ori_indices.begin(), meta.ori_indices.begin() + count,
                meta_info.ori_indices.end());
            this->meta.ori_indices.erase(this->meta.ori_indices.begin(), this->meta.ori_indices.begin() + count);
            meta.ori_indices.erase(meta.ori_indices.begin(), meta.ori_indices.begin() + count);

            if (this->meta.sampler_indices.size()) {
                this->meta.sampler_indices.erase(this->meta.sampler_indices.begin(),
                    this->meta.sampler_indices.begin() + count);
            }

            if (meta.sampler_indices.size()) {
                auto begin = meta.sampler_indices.begin(),
                    end = meta.sampler_indices.begin() + count;
                ::std::move(begin, end, meta_info.sampler_indices.end());
                meta.sampler_indices.erase(begin, end);
            }

            itm = metas.insert(itm + at_back, ::std::move(meta_info));
        }

        constexpr void insert_right(auto begin, auto count,
            auto& binds, auto& metas, auto& itb, auto& itm, auto& bind, auto& meta) {
            auto slice = ::std::move(temp);
            slice.binding = begin;
            slice.descriptorCount = count;
            binds.insert(itb + 1u, slice);

            metas.insert(itm + 1u, ::std::move(this->meta));
        }

        static constexpr void modify_right(auto begin, auto count,
            auto& binds, auto& metas, auto& itb, auto& itm, auto& bind, auto& meta) {
            bind.binding = begin;
            bind.descriptorCount = count;
            itb++; itm++;
        }
    };

    using default_semaphore_info = staged<unattached<>>;
    using default_event_info = staged<unattached<>>;

    template<typename T>
    struct basic_time_point : T {

        constexpr basic_time_point(auto&&...infos)
            : T{ forward_(infos)... } {
        }

        constexpr void append_wait(default_semaphore_info wait) {
            receive(wait_semaphores, ::std::move(wait));
        }
        constexpr void append_emit(default_semaphore_info emit) {
            receive(emit_semaphores, ::std::move(emit));
        }
        constexpr void erase_wait(::std::uint16_t index) {
            VK_ vkCmdWaitEvents;
            erase(wait_semaphores, index);
        }
        constexpr void erase_emit(::std::uint16_t index) {
            erase(emit_semaphores, index);
        }

    protected:
        ::std::vector<default_semaphore_info>
            wait_semaphores, emit_semaphores;

    protected:
        template<typename U>
        constexpr void init_from_others(U&& val) {
            wait_semaphores = forward_like<U>(val.wait_semaphores);
            emit_semaphores = forward_like<U>(val.emit_semaphores);
            if constexpr (requires { T::init_from_others(val); }) {
                T::init_from_others(val);
            }
        }

    private:
        constexpr void erase(auto& vec, auto index) {
            for (auto it = wait_semaphores.begin(); it != wait_semaphores.end(); it++) {
                if (it->index == index) {
                    vec.erase(it);
                    return;
                }
            }
        }
        constexpr void receive(auto& vec, auto val) {
            auto it = vec.begin();
            while (it != vec.end() && it->index < val.index) { it++; }
            if (it == vec.end() || it->index != val.index) {
                vec.emplace(it, val);
            }
            else {
                it->stages |= val.stages;
            }
        }
    };

    template<typename T>
    struct basic_event_point : T {
        constexpr void append_reset(default_event_info e) {
            receive(reset_events, ::std::move(e));
        }
        constexpr void append_set(default_event_info e) {
            receive(set_events, ::std::move(e));
        }
    protected:
        ::std::vector<default_event_info> reset_events, set_events;
        ::std::vector<staged<unattached<>>> wait_events;

    private:
        constexpr void receive(auto& vec, default_event_info e) {
            auto it = vec.begin();
            while (it != vec.end() && it->index < e.index) { it++; }
            if (it == vec.end() || it->index != e.index) {
                vec.insert(it, ::std::move(e));
            }
        }
    };
}

VKTL_EXPORT_ namespace vktl::detail {
    template<typename...Ts>
    struct meta_of<ref<Ts...>> {
        static_assert(!(::std::is_const_v<Ts> || ...), "Cannot ref on const object.");

        static constexpr auto type_id = make_type_id(COMMON_SCOPE, 0x2f1u);
        static constexpr auto name = fixed_string{ "ref<>" };
        using extend = extend_any;
        using order = order::at_front;
        using types = type_list<Ts...>;

        static constexpr auto num_parents = sizeof...(Ts);

        using parents = types;

        template<typename N>
        struct basic : N {
            using parents_by_ref = types;

            template<typename>
            friend struct basic;

            constexpr basic(auto&& value, auto&&...others)
                : N{ forward_(others)... }
                , parents_{ forward_(value) }
            {
            }

            template<typename E>
            bool handle_error(E error) const
                requires(requires{ this->T::parent()->handle_error(error); }) {
                return ((get<Ts* const>(parents_)->handle_error(error)) || ...);
            }

            template<typename T>
            constexpr auto parent() const noexcept {
                return parent_direct<T>(this);
            }

            template<::std::size_t index>
            constexpr tuple_at_t<index, parents>* parent() const noexcept {
                return::std::get<index>(parents_);
            }
            constexpr tuple_at_t<0u, parents>* parent() const noexcept {
                return::std::get<0u>(parents_);
            }

        private:
            template<typename T, ::std::size_t index = 0u>
            static constexpr auto parent_direct(auto pthis) noexcept {
                if constexpr (index < num_parents) {
                    if constexpr (contains<tuple_at_t<index, parents>, T>) {
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
        struct info : basic<N> {

            template<typename>
            friend struct info;

            constexpr info(auto&& infos)
                : basic<N>{ get_by<N>(forward_(infos)), forward_(infos) }
            {}
        };

        template<typename N>
        struct make : basic<N> {
            using base = basic<N>;

            constexpr void send(auto&&...values) {
                base::send(forward_(values)...);
                send_impl<0u>(forward_(values)...);
            }

            constexpr void receive(auto&&...values) {
                base::receive(forward_(values)...);
                receive_impl<0u>(forward_(values)...);
            }

        private:
            template<::std::size_t index, typename...Args>
            constexpr void send_impl(Args&&...args) {
                if constexpr (index < base::num_parents) {
                    if constexpr (::std::invocable<tuple_at_t<index, parents>, Args&&...>) {
                        parent<index>()->send(static_cast<Args&&>(args)...);
                    }
                    else {
                        send_impl<index + 1u>(static_cast<Args&&>(args)...);
                    }
                }
            }
            template<::std::size_t index, typename...Args>
            constexpr void receive_impl(Args&&...args) {
                if constexpr (index < base::num_parents) {
                    if constexpr (::std::invocable<tuple_at_t<index, parents>, Args&&...>) {
                        parent<index>()->receive(static_cast<Args&&>(args)...);
                    }
                    else {
                        receive_impl<index + 1u>(static_cast<Args&&>(args)...);
                    }
                }
            }
        };
    };

    using namespace extensions;

    template<> struct meta_of<allocator> {
        static constexpr auto type_id = make_type_id(COMMON_SCOPE, 0x2ffu);
        static constexpr auto name = fixed_string{ "allocator" };
        using interface = typename allocator::interface;
        using extend = extend_any;
        using order = order::at_front;
        using no_transform_tag = void;

        template<typename T>
        struct make : T {
            template<typename Tuples>
            constexpr make(Tuples const& infos) : T{ infos } {
                auto&& info = get_by<struct allocator>(infos);

                if (info.move) {
                    callbacks_.pUserData = owned_interface_.value = info.move;
                }
                else {
                    callbacks_.pUserData = info.view;
                }

                callbacks_.pfnAllocation = [](void* pUserData, size_t size, size_t alignment, VK_ VkSystemAllocationScope) -> void* {
                    return static_cast<interface*>(pUserData)->allocate(size, alignment);
                    };
                callbacks_.pfnReallocation = [](void* pUserData, void* pOriginal, size_t size, size_t alignment, VK_ VkSystemAllocationScope) -> void* {
                    return static_cast<interface*>(pUserData)->reallocate(pOriginal, size, alignment);
                    };
                callbacks_.pfnFree = [](void* pUserData, void* pMemory) {
                    static_cast<interface*>(pUserData)->free(pMemory);
                    };
                callbacks_.pfnInternalAllocation = [](void* pUserData, size_t size, VK_ VkInternalAllocationType, VK_ VkSystemAllocationScope) {
                    static_cast<interface*>(pUserData)->internal_allocate(size, "unknown");
                    };
                callbacks_.pfnInternalFree = [](void* pUserData, size_t size, VK_ VkInternalAllocationType, VK_ VkSystemAllocationScope) {
                    static_cast<interface*>(pUserData)->internal_free(size, "unknown");
                    };
            }

            ~make() {
                if (owned_interface_.value) {
                    delete owned_interface_.value;
                }
            }

            constexpr auto allocator() { return callbacks_.pfnAllocation ? &callbacks_ : nullptr; }

        private:
            move_only<interface*> owned_interface_{ nullptr };
            VK_ VkAllocationCallbacks callbacks_{};
        };
    };

    template<> struct meta_of<graphics> {
        static constexpr auto type_id = make_type_id(COMMON_SCOPE, 0x2e0u);
        static constexpr auto name = fixed_string{ "graphics" };

        using extend = extend_any;

        template<typename T>
        using info = T;
        template<typename T>
        using make = T;
    };

    template<> struct meta_of<compute> {
        static constexpr auto type_id = make_type_id(COMMON_SCOPE, 0x2e1u);
        static constexpr auto name = fixed_string{ "compute" };
        using extend = extend_any;

        template<typename T>
        using info = T;
        template<typename T>
        using make = T;
    };

    template<>
    struct meta_of<is_static> {
        static constexpr auto type_id = make_type_id(COMMON_SCOPE, 0x2f0u);
        static constexpr auto name = fixed_string{ "is_static" };
        using extend = extend_any;
        using order = order::at_middle;


        template<typename T>
        struct info; // not all the object can have `is_static` qualifier. 

        template<typename T>
        using make = T;
    };

    template<>
    struct meta_of<repeat> {
        static constexpr auto type_id = make_type_id(COMMON_SCOPE, 0x2f1u);
        static constexpr auto name = fixed_string{ "repeat" };

        using extend = extend_any;

        template<typename T>
        struct info;
    };
}

#include "math.hpp"
#include "device.hpp"
#include "execution.hpp"
#include "bind_points.hpp"
#include "pass.hpp"
