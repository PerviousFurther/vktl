#pragma once

// Interface style: `object` builds fluent expressions into a compile-time
// chain of small mixins, with parent objects retained by explicit references.
// Implementation: `m<Tag, Next>` is the extension point; unknown aggregate
// descriptors are transparent layers so data-only declarations compose safely.

VKTL_EXPORT_ namespace vktl::detail {
  using error = vktl::error;

  struct popup {
    template <typename T> struct closure {
      friend auto operator|(VK_ VkResult result, closure self) noexcept(false) {
        if (!self.value_->handle_error(result))
          VKTL_UNLIKELY { throw error{result, self.error_}; }
      }

      T *value_;
      const char *error_;
    };

    template <typename T> constexpr auto operator()(T &handler) {
      return closure<T>{&handler};
    }

    VKTL_MAYBE_UNUSED
    friend void operator|(VK_ VkResult result, popup self) noexcept(false) {
      if (result != VK_ VK_SUCCESS)
        VKTL_UNLIKELY { throw error{result, self.error_}; }
    }

    const char *error_;
  };

  // node.

  template <typename First, typename Next> struct m;

  // consume.

  template <typename State, typename Types> struct c;

  template <size_t index> struct i : ::std::integral_constant<size_t, index> {};

  template <typename T> struct express;
  // template<typename T>
  // struct skip;
  // template<typename T>
  // struct ignore;

  template <typename T>
  concept can_express = ::std::default_initializable<express<T>>;
  // template<typename T>
  // concept can_skip = ::std::default_initializable<skip<T>>;
  // template<typename T>
  // concept can_ignore = ::std::default_initializable<ignore<T>>;

  template <typename T>
  concept on_chain = !can_express<T> /*&& !can_skip<T> && !can_ignore<T>*/;

  template <typename State, typename Types> struct c;

  template <size_t index, typename Types>
    requires(index < tuple_size_v<Types>)
  struct c<i<index>, Types>
      : m<tuple_at_t<index, Types>, c<i<index + 1u>, Types>> {
    using base = m<tuple_at_t<index, Types>, c<i<index + 1u>, Types>>;

    template <typename T> friend struct express;

    template <typename First, typename... Args>
    constexpr c(First &&first, Args &&...infos)
      requires(on_chain<::std::remove_cvref_t<First>>)
        : base{forward_(first), forward_(infos)...} {}

    template <typename Ignored, typename... Args>
    constexpr c(Ignored &&first, Args &&...infos)
      requires(can_express<::std::remove_cvref_t<Ignored>>)
        : c(forward_(infos)...) {
      express<::std::remove_cvref_t<Ignored>>::invoke(forward_(first), *this);
    }

    // template<typename Ignored, typename...Args>
    // constexpr c(Ignored&& first, Args&&...infos)
    // 	requires(can_skip<::std::remove_cvref_t<Ignored>>)
    // 	: c(forward_(first), forward_(infos)...) {
    // }
    //
    // template<typename Ignored, typename...Args>
    // constexpr c(Ignored&&, Args&&...infos)
    // 	requires(can_ignore<::std::remove_cvref_t<Ignored>>)
    // 	: c(forward_(infos)...) {
    // }
  };
  template <size_t index, typename Types>
    requires(index >= tuple_size_v<Types>)
  struct c<i<index>, Types> : Types {
    using base = Types;
    constexpr c(auto &&...infos) : base{forward_(infos)...} {}
  };

  template <typename T> struct object;

  template <typename T, typename C>
  struct is_queryable
      : ::std::is_same<::std::remove_cvref_t<T>, ::std::remove_cvref_t<C>> {};

  struct empty {};
  struct always_one {
    operator uint32_t() const noexcept { return 1u; }
  };

  inline /* constinit */ struct lock_duck_ {
    static constexpr void lock() noexcept {}
    static constexpr bool try_lock() noexcept { return true; }
    static constexpr void unlock() noexcept {}
  } lock_duck{};

  inline constexpr struct call_duck_ {
    void operator()(auto &&...) {}
  } call_duck{};

  template <typename T> struct o {
    static constexpr void init() noexcept {}
    static constexpr void reset() noexcept {}
  };

  template <typename C, typename... Ts> struct b : C {
    constexpr b() = default;
    constexpr b(auto &&...) {}

    static constexpr always_one add_ref() noexcept { return {}; }
    static constexpr always_one release() noexcept { return {}; }

    template <typename...> static constexpr auto parent() noexcept {
      return nullptr;
    }
    template <typename...> static constexpr auto all_parent() noexcept {
      return::std::tuple();
    }
    static constexpr auto parent() noexcept { return nullptr; }

    template <typename Q>
    static constexpr bool query(::std::in_place_t) noexcept {
      return ((::std::same_as<Ts, Q>) || ...);
    }
    template <typename... Qs>
      requires(sizeof...(Qs) > 1)
    static constexpr bool query(::std::in_place_t v) noexcept {
      return ((query<Qs>(v)) && ...);
    }
    template <typename Q> static constexpr bool query() noexcept {
      return ((is_queryable<Ts, Q>::value) || ...);
    }
    template <typename... Qs>
      requires(sizeof...(Qs) > 1)
    static constexpr bool query() noexcept {
      return ((query<Qs>()) && ...);
    }

    static constexpr lock_duck_ &get_lock() noexcept { return lock_duck; }

    static constexpr auto allocator() noexcept { return nullptr; }

  protected:
    using self = c<i<1u>, b<C, Ts...>>;

    static constexpr void relocate() noexcept {}
    static constexpr void finalize() noexcept {}

    constexpr self &as_self() noexcept { return *as_this(); }
    constexpr self const &as_self() const noexcept { return *as_this(); }
    constexpr self *as_this() noexcept { return static_cast<self *>(this); }
    constexpr self const *as_this() const noexcept {
      return static_cast<self const *>(this);
    }
  };
  template <typename C, typename... Ts>
  struct tuple_like<b<C, Ts...>> : ::std::true_type {};

  template <typename... Args> struct s_;

  template <typename B, typename... Args> struct f_;
  template <typename B, typename... Args>
    requires(!tuple_specialization_of<B, b>)
  struct f_<B, Args...> : f_<b<B>, Args...> {
    using next = f_<b<B>, Args...>;

    static constexpr auto make_tuple(auto &&...values) {
      return next::make_tuple(::std::tuple(), forward_(values)...);
    }

    template <typename... RArgs>
    static constexpr auto make_tuple(s_<RArgs...> &&v) {
      return ::std::apply(
          [](auto &&...values) { return make_tuple(forward_(values)...); },
          v.as_tuple());
    }
  };

  template <typename... Cs, typename C, typename First, typename... Args>
    requires(on_chain<First>)
  struct f_<b<C, Cs...>, First, Args...> : f_<b<C, First, Cs...>, Args...> {
    using next = f_<b<C, First, Cs...>, Args...>;
    using first = First;

    static constexpr auto make_tuple(auto first, auto &&fir, auto &&...values) {
      return next::make_tuple(
          ::std::tuple_cat(::std::move(first),
                           ::std::forward_as_tuple(forward_(fir))),
          forward_(values)...);
    }
  };
  template <typename... Cs, typename First, typename... Args>
    requires(!on_chain<First>)
  struct f_<b<Cs...>, First, Args...> : f_<b<Cs...>, Args...> {
    using next = f_<b<Cs...>, Args...>;
    static constexpr auto make_tuple(auto first, auto &&, auto &&...values) {
      return next::make_tuple(::std::move(first), forward_(values)...);
    }
  };

  template <typename... Ts> struct from : ::std::tuple<Ts *...> {
    using base = ::std::tuple<Ts *...>;

    static_assert(((!::std::is_reference_v<Ts>) && ...));

    constexpr from(Ts &...value) : base{&value...} {}

    constexpr auto &&as_tuple() & { return *static_cast<base &&>(*this); }
  };
  template <typename... Ts> from(Ts & ...) -> from<Ts...>;

  template <typename C> struct use_base {};

  template <typename T> struct express<use_base<T>> {
    static constexpr void invoke(auto &&...) noexcept {}
  };

  // already have base.
  template <typename C, typename... Cs, typename T, typename... Us,
            typename... Args>
  struct f_<b<C, from<Us...>, Cs...>, object<T>, Args...>
      : f_<b<C, from<Us..., object<T>>, Cs...>, Args...> {
    using next = f_<b<C, from<Us..., object<T>>, Cs...>, Args...>;

    static constexpr auto make_tuple(auto first, object<T> &fir,
                                     auto &&...args) {
      return make_tuple_impl(::std::index_sequence_for<Cs...>(), first, fir,
                             forward_(args)...);
    }
    template <::std::size_t... ids>
    static constexpr auto make_tuple_impl(::std::index_sequence<ids...>,
                                          auto &first, object<T> &fir,
                                          auto &&...args) {
      return next::make_tuple(
          ::std::forward_as_tuple(
              get<ids>(::std::move(first))...,
              ::std::apply([&](auto... ptr) { return from{*ptr..., fir}; },
                           get<sizeof...(ids)>(first).as_tuple())),
          forward_(args)...);
    }
    static constexpr auto make_tuple(auto, object<T> &&, auto &&...) =
        delete; // not allow right value reference on object.
  };
  template <typename C, typename... Cs, typename T, typename... Args>
  struct f_<b<C, Cs...>, object<T>, Args...>
      : f_<b<C, from<object<T>>, Cs...>, Args...> {
    using next = f_<b<C, from<object<T>>, Cs...>, Args...>;

    static constexpr auto make_tuple(auto first, object<T> &fir,
                                     auto &&...args) {
      return next::make_tuple(
          ::std::tuple_cat(::std::move(first), ::std::forward_as_tuple(fir)),
          forward_(args)...);
    }
    static constexpr auto make_tuple(auto, object<T> &&, auto &&...) =
        delete; // not allow right value reference on object.
  };

  template <typename C, typename... Cs, typename T, typename... Args>
  struct f_<b<C, Cs...>, use_base<T>, Args...> : f_<b<T, Cs...>, Args...> {
    using next = f_<b<T, Cs...>, Args...>;
    static constexpr auto make_tuple(auto first, use_base<T>, auto &&...args) {
      return next::make_tuple(::std::move(first), forward_(args)...);
    }
  };

  // template<typename T>
  // struct is_host : ::std::false_type {};

  // this will change in future.
  template <typename T>
  concept creation_tag = ::std::is_aggregate_v<T>;

  template <typename T, typename... Qs>
  concept object_of = []() constexpr {
    if constexpr (requires { T::template query<Qs...>(); }) {
      return T::template query<Qs...>();
    } else {
      return false;
    }
  }();

  template <typename T, typename... Qs>
  concept inside_object = []() constexpr {
    if constexpr (requires { T::template query<Qs...>(); }) {
      return T::template query<Qs...>(::std::in_place);
    } else {
      return false;
    }
  }();

  // the object contain a lock help operation thread safe.
  inline constexpr struct lockable_ {
  } lockable;
  // cross thread shared use reference counter (not thread safe).
  inline constexpr struct shared_ {
  } shared;
  // cross thread shared use atomic reference counter.
  inline constexpr struct cross_thread_shared_ {
  } cross_thread_shared;

  template <typename N> constexpr auto &lock_of(N * pthis) noexcept {
    if constexpr (object_of<N, lockable_>) {
      return pthis->get_lock();
    } else {
      return lock_duck;
    }
  }

  template <typename N> constexpr auto locker_of(N * pthis) {
    if constexpr (object_of<N, lockable_>) {
      return ::std::unique_lock(lock_of(pthis));
    } else {
      return nullptr;
    }
  }

  template <typename T> struct object : T::type {
    using object_tag = void;
    using tag = T;
    using base = typename T::type;

    template <typename... Args>
      requires(((tuple_specialization_of<Args, object> ||
                 tuple_specialization_of<Args, s_> || creation_tag<Args>) &&
                ...))
    constexpr object(Args &&...args)
        : object{T::make_tuple(static_cast<Args &&>(args)...)} {
      base::finalize();
    }

    ~object() { base::reset(); }

    constexpr object(object const &object)
      requires(::std::copy_constructible<base>)
        : base{static_cast<base const &>(object)} {
      base::relocate();
    }
    constexpr object &operator=(object const &object)
      requires(::std::is_copy_assignable_v<base>)
    {
      static_cast<base>(*this) = static_cast<base const &>(object);
      base::relocate();
      return *this;
    }

    constexpr object(object &&object)
      requires(::std::move_constructible<base>)
        : base{static_cast<base &&>(object)} {
      base::relocate();
    }
    constexpr object &operator=(object &&object)
      requires(::std::is_move_assignable_v<base>)
    {
      static_cast<base>(*this) = static_cast<base &&>(object);
      base::relocate();
      return *this;
    }

    void init() {
      auto _ = locker_of(this);
      base::relocate();
      base::init();
    }

    void reset() {
      auto _ = locker_of(this);
      base::reset();
    }

    template <typename O>
      requires(creation_tag<O>)
    constexpr auto operator|(O other) & {
      return s_<object &, O>{*this, other};
    }
    template <typename O> constexpr auto operator|(object<O> &other) & {
      return s_<object &, object<O> &>{*this, other};
    }

  private:
    template <typename Tuple, size_t... ids>
      requires(tuple_like_v<Tuple> && !tuple_specialization_of<Tuple, s_>)
    constexpr object(::std::index_sequence<ids...>, Tuple &&tuple)
        : base{get<ids>(static_cast<Tuple &&>(tuple))...} {}
    template <typename Tuple>
      requires(tuple_like_v<Tuple> && !tuple_specialization_of<Tuple, s_>)
    constexpr object(Tuple &&tuple)
        : object{
              ::std::make_index_sequence<tuple_size_v<Tuple>>(),
              static_cast<Tuple &&>(tuple),
          } {}
  };
  template <typename... Args>
  object(Args && ...) -> object<f_<o<empty>, ::std::remove_cvref_t<Args>...>>;
  template <typename... Args>
  object(s_<Args...> &&)
      -> object<f_<o<empty>, ::std::remove_reference_t<Args>...>>;

  template <typename... Cs> struct f_<b<Cs...>> {
    using type = c<i<1u>, b<Cs...>>;
    static constexpr auto make_tuple(auto first) {
      return ::std::apply(
          [&](auto &&...ref) {
            return ::std::forward_as_tuple(forward_(ref)...);
          },
          ::std::move(first));
    }
  };

  template <typename C, typename... Rs> struct l_ : C {
  protected:
    static constexpr void begin() noexcept {}
    static constexpr void end() noexcept {}
    static constexpr void finalize() noexcept {}
  };

  template <typename T>
  concept command_tag = ::std::is_class_v<T>;

  template <typename T> struct commands : T::type {
    using base = T::type;
    template <typename... Args>
      requires(((tuple_specialization_of<Args, commands> ||
                 tuple_specialization_of<Args, s_> || command_tag<Args>) &&
                ...))
    constexpr commands(Args &&...vals)
        : commands{T::make_tuple(static_cast<Args &&>(vals)...)} {
      base::finalize();
    }

    void invoke() {
      base::begin();
      base::end();
    }

    template <typename O>
      requires(command_tag<O>)
    constexpr auto operator|(O other) & {
      return s_<commands &, O>{*this, other};
    }
    template <typename O> constexpr auto operator|(commands<O> &other) & {
      return s_<commands &, commands<O> &>{*this, other};
    }

  private:
    template <typename... Args, ::std::size_t... ids>
    constexpr commands(::std::index_sequence<ids...>,
                       ::std::tuple<Args...> vals)
        : base{static_cast<Args>(get<ids>(::std::move(vals)))...} {}
    template <typename... Args>
    constexpr commands(::std::tuple<Args...> vals)
        : commands{::std::make_index_sequence<sizeof...(Args)>(),
                   ::std::move(vals)} {}
  };
  template <typename... Args>
  commands(Args && ...)
      -> commands<f_<l_<empty>, ::std::remove_cvref_t<Args>...>>;
  template <typename... Args>
  commands(s_<Args...> &&)
      -> commands<f_<l_<empty>, ::std::remove_cvref_t<Args>...>>;

  template <typename... Ts> struct combine : ::std::tuple<Ts *...> {
    using base = ::std::tuple<Ts *...>;
    combine(Ts &...vals) : base{&vals...} {}
    combine(::std::tuple<Ts *...> const &vals) : base{vals} {}
  };

  template <typename C, typename F, typename... Rs, typename... Args>
  struct f_<b<C, Rs...>, commands<F>, Args...>
      : f_<b<C, combine<commands<F>>, Rs...>, Args...> {
    using next = f_<b<C, combine<commands<F>>, Rs...>, Args...>;
    static constexpr auto make_tuple(auto f, commands<F> &first,
                                     auto &&...values) {
      return next::make_tuple(
          ::std::tuple_cat(::std::move(f),
                           ::std::forward_as_tuple(combine{first})),
          forward_(values)...);
    }
  };

  template <typename C, typename F, typename... Cs, typename... Rs,
            typename... Args>
  struct f_<b<C, combine<Cs...>, Rs...>, commands<F>, Args...>
      : f_<b<C, combine<Cs..., commands<F>>, Rs...>, Args...> {
    using next = f_<b<C, combine<Cs..., commands<F>>, Rs...>, Args...>;
    static constexpr auto make_tuple(auto f, commands<F> &first,
                                     auto &&...values) {
      return make_tuple_impl(::std::make_index_sequence<sizeof...(Rs)>(), first,
                             ::std::move(f), forward_(values)...);
    }
    template <::std::size_t... ids>
    static constexpr auto make_tuple_impl(::std::index_sequence<ids...>,
                                          commands<F> &first, auto f, auto s,
                                          auto &&...values) {
      return next::make_tuple(
          ::std::forward_as_tuple(
              ::std::apply(
                  [&](auto &&...vals) { return combine{first, *vals...}; },
                  get<0u>(::std::move(f))),
              get<ids + 1>(::std::move(f))...),
          ::std::move(s), forward_(values)...);
    }
  };

  template <typename... Ts> struct s_ : ::std::tuple<Ts...> {
    using base = ::std::tuple<Ts...>;

    template <typename... Args>
    constexpr s_(Args &&...args) : base{static_cast<Args &&>(args)...} {}

    template <typename T> struct ref_if_object : ::std::remove_cvref<T> {};
    template <typename T>
      requires(tuple_specialization_of<T, object> ||
               tuple_specialization_of<T, commands>)
    struct ref_if_object<T> : ::std::type_identity<::std::remove_cvref_t<T> &> {
    };

    template <typename... Args, size_t... ids>
    static constexpr auto make(::std::tuple<Args...> tuple,
                               ::std::index_sequence<ids...>) {
      return s_<typename ref_if_object<Args>::type...>{
          get<ids>(::std::move(tuple))...};
    }

    constexpr auto &&as_tuple() { return static_cast<base &&>(*this); }
    template <creation_tag O> constexpr auto operator|(O value) && noexcept {
      return make(
          ::std::tuple_cat(as_tuple(), ::std::tuple(::std::move(value))),
          ::std::make_index_sequence<sizeof...(Ts) + 1>());
    }
    template <typename O>
    constexpr auto operator|(object<O> &value) && noexcept {
      return make(::std::tuple_cat(as_tuple(), ::std::forward_as_tuple(value)),
                  ::std::make_index_sequence<sizeof...(Ts) + 1>());
    }
    template <typename... Os>
    constexpr auto operator|(s_<Os...> &&value) && noexcept {
      return make(::std::tuple_cat(as_tuple(), value.as_tuple()),
                  ::std::make_index_sequence<sizeof...(Ts) + sizeof...(Os)>());
    }

    constexpr
    operator object<f_<::std::remove_reference_t<Ts>...>>() && noexcept {
      return ::std::apply(
          [](auto &&...vals) { return object{forward_(vals)...}; }, as_tuple());
    }
    constexpr
    operator commands<f_<::std::remove_reference_t<Ts>...>>() && noexcept {
      return ::std::apply(
          [](auto &&...vals) { return commands{forward_(vals)...}; },
          as_tuple());
    }
  };
  template <typename... Ts> struct tuple_like<s_<Ts...>> : ::std::true_type {};

  template <typename T, typename O>
    requires(creation_tag<T> && creation_tag<O>)
  constexpr auto operator|(T value, O other) noexcept {
    return s_<T, O>{::std::move(value), ::std::move(other)};
  }

  template <typename... Qs, typename N>
  constexpr auto parent_of(N * pthis) noexcept {
    return pthis->template parent<Qs...>();
  }
  template <typename... Qs, typename N>
  constexpr auto parent_of(N & ref) noexcept {
    return ref.template parent<Qs...>();
  }

  template <typename N, typename... Qs>
  using parent_t = ::std::remove_pointer_t<
      decltype(::std::declval<N &>().template parent<Qs...>())>;

  template <typename N, typename... Qs>
  concept have_parent_of = []() constexpr {
    if constexpr (requires(N &v) { v.template parent<Qs...>(); }) {
      return !::std::is_null_pointer_v<parent_t<N, Qs...>>;
    } else {
      return false;
    }
  }();

  template <typename N, typename P, typename... Qs>
  concept inside_parent = []() constexpr {
    if constexpr (requires(N &v) { v.template parent<P>(); }) {
      return parent_t<N, P>::template query<Qs...>(::std::in_place);
    } else {
      return false;
    }
  }();

  template <typename... Qs, typename N>
  constexpr auto handle_of(N * pthis) noexcept {
    return parent_of<Qs...>(pthis)->handle();
  }

  template <typename C, typename B>
  concept bindable_from =
      requires { B::template apply<vptr_base>::template rebind<C>(); };
}