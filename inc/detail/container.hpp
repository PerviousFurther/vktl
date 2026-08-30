#pragma once

// Agents specification:
// - mumap is a red-black tree ordered by hash value and stores links as void
//   pointers so head() can recover node pointers with static_cast. Equal hashes
//   use a collision chain, with one fat node per distinct key.
// - A fat node has no declared trailing member. Its dynamically growing,
//   contiguous element_type storage begins immediately after the aligned
//   header; value_type exposes that storage as pair<const Key, span<T>>.
// - KeyEqual resolves real hash collisions. Growing one key's element storage
//   replaces only that key's allocation and preserves its tree/chain position.
// - vectors owns its heterogeneous SoA allocation as void*. Confine byte-wise
//   addressing to offset_address() and recover typed column pointers from void*.
// - spans carries element constness in its Ts pack. A const spans object does
//   not add constness; const vectors access is exposed through spans<const Ts...>.

VKTL_EXPORT_ namespace vktl::detail {

  // currently standard c++ is not support *provenance* object.
  // thus object emplace inside this list should inherit from poly_list::node.
  class poly_list {
  public:
    struct node {
      friend poly_list;

      node() = default;

      node(const node &) = delete;
      node &operator=(const node &) = delete;
      node(node &&) noexcept = delete;
      node &operator=(node &&) noexcept = delete;

      template <::std::derived_from<node> T>
      constexpr auto &as() const noexcept {
        return *static_cast<const T *>(this);
      }
      template <::std::derived_from<node> T> constexpr auto &as() noexcept {
        return *static_cast<T *>(this);
      }

      template <::std::derived_from<node> T> constexpr operator T &() noexcept {
        return as<T>();
      }
      template <::std::derived_from<node> T>
      constexpr operator T const &() const noexcept {
        return as<T>();
      }

    private:
      void (*deleter)(node const *) noexcept = nullptr;
      node *prev = nullptr;
      node *next = nullptr;
    };

    using size_type = ::std::size_t;
    using difference_type = ::std::ptrdiff_t;

  private:
    template <bool is_const> struct basic_iterator {
      friend poly_list;

      using value_type = ::std::conditional_t<is_const, node const, node>;
      using difference_type = ::std::ptrdiff_t;
      using reference = value_type &;
      using pointer = value_type *;
      using iterator_category = ::std::bidirectional_iterator_tag;
      using iterator_concept = ::std::bidirectional_iterator_tag;

      basic_iterator() noexcept = default;
      basic_iterator(pointer p) noexcept : ptr_(p) {}

      basic_iterator(const basic_iterator &) = default;
      basic_iterator &operator=(const basic_iterator &) = default;

      template <bool other_const>
        requires(is_const && !other_const)
      basic_iterator(const basic_iterator<other_const> &other) noexcept
          : ptr_(other.ptr_) {}

      reference operator*() const noexcept { return *ptr_; }
      pointer operator->() const noexcept { return ptr_; }

      basic_iterator &operator++() noexcept {
        ptr_ = ptr_->next;
        return *this;
      }
      basic_iterator operator++(int) noexcept {
        auto tmp = *this;
        ptr_ = ptr_->next;
        return tmp;
      }
      basic_iterator &operator--() noexcept {
        ptr_ = ptr_->prev;
        return *this;
      }
      basic_iterator operator--(int) noexcept {
        auto tmp = *this;
        ptr_ = ptr_->prev;
        return tmp;
      }

      friend bool operator==(const basic_iterator &a,
                             const basic_iterator &b) noexcept {
        return a.ptr_ == b.ptr_;
      }

    private:
      pointer ptr_ = nullptr;
    };

  public:
    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

    poly_list() noexcept { reset_sentinel(); }
    ~poly_list() { clear(); }

    poly_list(const poly_list &) = delete;
    poly_list &operator=(const poly_list &) = delete;

    poly_list(poly_list &&other) noexcept {
      reset_sentinel();
      swap(other);
    }

    poly_list &operator=(poly_list &&other) noexcept {
      if (this != &other) {
        clear();
        swap(other);
      }
      return *this;
    }

    template <::std::derived_from<node> T, typename... Args>
    T &emplace(const_iterator where, Args &&...args)
      requires(::std::constructible_from<T, Args && ...>)
    {
      auto *obj = new T(static_cast<Args &&>(args)...);
      auto *node_ptr = static_cast<node *>(obj);
      insert_before(const_cast<node *>(where.ptr_), node_ptr);
      node_ptr->deleter = [](node const *ptr) noexcept {
        delete static_cast<T const *>(ptr);
      };
      return *obj;
    }

    template <::std::derived_from<node> T, typename... Args>
    T &emplace_back(Args &&...args) {
      return emplace<T>(&root_, static_cast<Args &&>(args)...);
    }

    template <::std::derived_from<node> T, typename... Args>
    T &emplace_front(Args &&...args) {
      return emplace<T>(root_.next, static_cast<Args &&>(args)...);
    }

    iterator begin() noexcept { return iterator(root_.next); }
    iterator end() noexcept { return iterator(&root_); }
    const_iterator begin() const noexcept { return const_iterator(root_.next); }
    const_iterator end() const noexcept { return const_iterator(&root_); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    node &front() noexcept {
      VKTL_ASSERT(this->root_.next != &this->root_);
      return *this->root_.next;
    }
    node const &front() const noexcept {
      VKTL_ASSERT(this->root_.next != &this->root_);
      return *this->root_.next;
    }
    node &back() noexcept {
      VKTL_ASSERT(this->root_.prev != &this->root_);
      return *this->root_.prev;
    }
    node const &back() const noexcept {
      VKTL_ASSERT(this->root_.prev != &this->root_);
      return *this->root_.prev;
    }

    iterator erase(const_iterator pos) noexcept {
      auto *n = pos.ptr_;
      node *next_node = n->next;

      n->prev->next = n->next;
      n->next->prev = n->prev;
      --size_;

      VKTL_ASSERT(n->deleter); // might be sential, it is not allowed.
      n->deleter(n);

      return iterator(next_node);
    }

    template <::std::derived_from<node> T> void erase(T &value) noexcept {
      auto *node_ptr = static_cast<node *>(&value);
      erase(const_iterator(node_ptr));
    }

    void pop_back() noexcept {
      if (!empty())
        erase(const_iterator(root_.prev));
    }
    void pop_front() noexcept {
      if (!empty())
        erase(const_iterator(root_.next));
    }

    void clear() noexcept {
      while (!empty())
        pop_front();
    }

    VKTL_NODISCARD size_type size() const noexcept { return size_; }
    VKTL_NODISCARD bool empty() const noexcept { return size_ == 0; }

    void swap(poly_list &other) noexcept {
      if (this == &other)
        return;

      ::std::swap(root_.next, other.root_.next);
      ::std::swap(root_.prev, other.root_.prev);
      ::std::swap(size_, other.size_);

      if (size_ == 0u) {
        reset_sentinel();
      } else {
        root_.next->prev = &root_;
        root_.prev->next = &root_;
      }

      if (other.size_ == 0u) {
        other.reset_sentinel();
      } else {
        other.root_.next->prev = &other.root_;
        other.root_.prev->next = &other.root_;
      }
    }

  private:
    void reset_sentinel() noexcept {
      root_.next = &root_;
      root_.prev = &root_;
    }

    void insert_before(node *pos, node *n) noexcept {
      n->next = pos;
      n->prev = pos->prev;
      pos->prev->next = n;
      pos->prev = n;
      ++size_;
    }

  private:
    node root_;
    size_type size_ = 0u;
  };

  template <typename... Ts> struct vectors;

  template <typename... Ts> struct iterators {
    template <typename... Ts> friend struct iterators;

    static_assert(sizeof...(Ts) > 0, "Requires at least one type.");
    static_assert(((!::std::is_reference_v<Ts>) && ...),
                  "Not allow reference.");
    static_assert(((::std::is_const_v<Ts>) && ...) ||
                      ((!::std::is_const_v<Ts>) && ...),
                  "Must all const or non const.");
    static_assert(((::std::is_volatile_v<Ts>) && ...) ||
                      ((!::std::is_volatile_v<Ts>) && ...),
                  "Must all volatile or non const.");

    static constexpr auto is_const = ((::std::is_const_v<Ts>) || ...);
    using size_type = size_t;

    using iterator_category = ::std::random_access_iterator_tag;
    using iterator_concept = ::std::random_access_iterator_tag;
    using difference_type = ::std::ptrdiff_t;
    using value_type = ::std::tuple<Ts...>;
    using reference = ::std::tuple<Ts &...>;

    constexpr iterators() = default;
    constexpr explicit iterators(::std::tuple<Ts *...> pointers) noexcept
        : pointers_(pointers) {}
    template <typename... Os>
      requires(sizeof...(Os) == sizeof...(Ts) &&
               ((::std::convertible_to<Os (*)[], Ts (*)[]>) && ...))
    constexpr iterators(iterators<Os...> const &other)
        : pointers_{other.pointers_} {}
    constexpr iterators(iterators const &) noexcept = default;
    constexpr iterators &operator=(iterators const &) noexcept = default;

    template <size_type I = 0u> constexpr decltype(auto) get() const noexcept {
      return *::std::get<I>(pointers_);
    }

    constexpr reference operator*() const noexcept {
      return dereference(::std::index_sequence_for<Ts...>{});
    }
    constexpr reference operator[](difference_type offset) const noexcept {
      return dereference_at(offset, ::std::index_sequence_for<Ts...>{});
    }
    constexpr iterators &operator++() noexcept {
      advance(1);
      return *this;
    }
    constexpr iterators operator++(int) noexcept {
      auto copy = *this;
      ++*this;
      return copy;
    }

    constexpr iterators &operator--() noexcept {
      advance(-1);
      return *this;
    }
    constexpr iterators operator--(int) noexcept {
      auto copy = *this;
      --*this;
      return copy;
    }
    constexpr iterators &operator+=(difference_type offset) noexcept {
      advance(offset);
      return *this;
    }
    constexpr iterators &operator-=(difference_type offset) noexcept {
      advance(-offset);
      return *this;
    }
    constexpr iterators operator+(difference_type offset) const noexcept {
      auto copy = *this;
      return copy += offset;
    }
    constexpr iterators operator-(difference_type offset) const noexcept {
      auto copy = *this;
      return copy -= offset;
    }
    friend constexpr iterators operator+(difference_type offset,
                                         iterators self) noexcept {
      return self += offset;
    }
    constexpr difference_type operator-(const iterators &other) const noexcept {
      return ::std::get<0>(pointers_) - ::std::get<0>(other.pointers_);
    }
    constexpr bool operator==(const iterators &other) const noexcept {
      return ::std::get<0>(pointers_) == ::std::get<0>(other.pointers_);
    }
    constexpr auto operator<=>(const iterators &other) const noexcept {
      return ::std::get<0>(pointers_) <=> ::std::get<0>(other.pointers_);
    }

  private:
    template <::std::size_t... Is>
    constexpr reference
    dereference(::std::index_sequence<Is...>) const noexcept {
      return {*::std::get<Is>(pointers_)...};
    }

    template <::std::size_t... Is>
    constexpr reference
    dereference_at(difference_type offset,
                   ::std::index_sequence<Is...>) const noexcept {
      return {::std::get<Is>(pointers_)[offset]...};
    }

    constexpr void advance(difference_type offset) noexcept {
      ::std::apply(
          [offset](auto *&...pointers) { ((pointers += offset), ...); },
          pointers_);
    }

    ::std::tuple<Ts *...> pointers_{};
  };

  template <typename... Ts> struct spans {
    static_assert(sizeof...(Ts) > 0, "multispan requires at least one type.");
    static_assert(((!::std::is_reference_v<Ts>) && ...),
                  "Spans not allow reference.");
    static_assert(((::std::is_const_v<Ts>) && ...) ||
                      ((!::std::is_const_v<Ts>) && ...),
                  "Must all const or non const.");
    static_assert(((::std::is_volatile_v<Ts>) && ...) ||
                      ((!::std::is_volatile_v<Ts>) && ...),
                  "Must all volatile or non const.");

    template <typename...> friend struct spans;

    using types = ts<Ts...>;
    using size_type = ::std::size_t;
    using difference_type = ::std::ptrdiff_t;

    template <size_type I> using element_t = tuple_at_t<I, types>;
    static constexpr size_type num_types = sizeof...(Ts);

    constexpr spans() noexcept = default;
    constexpr spans(const spans &) noexcept = default;
    constexpr spans &operator=(const spans &) noexcept = default;

    // ---- adopt raw pointers -------------------------------------------------
    constexpr spans(size_type count, Ts *...pointers) noexcept
        : pointers_{pointers...}, size_(count) {}

    // ---- adopt N contiguous containers (vector, array, span, C array, ...)
    // ---
    template <typename... Rs>
      requires(sizeof...(Rs) == num_types &&
               (!::std::same_as<::std::remove_cvref_t<Rs>, spans> && ...) &&
               (::std::ranges::contiguous_range<Rs> && ...) &&
               (::std::ranges::sized_range<Rs> && ...) &&
               (::std::convertible_to<::std::ranges::range_value_t<Rs> (*)[],
                                      Ts (*)[]> &&
                ...))
    constexpr explicit spans(Rs &...ranges)
        : pointers_{::std::ranges::data(ranges)...},
          size_(::std::min({size_type(::std::ranges::size(ranges))...})) {}

    // ---- adopt a multivec ---------------------------------------------------
    template <typename... Us>
      requires(sizeof...(Us) == num_types &&
               (::std::convertible_to<Us (*)[], Ts (*)[]> && ...))
    constexpr spans(vectors<Us...> &owner) noexcept {
      adopt(owner);
    }

    template <typename... Us>
      requires(sizeof...(Us) == num_types &&
               (::std::convertible_to<const Us (*)[], Ts (*)[]> && ...))
    constexpr spans(const vectors<Us...> &owner) noexcept {
      adopt(owner);
    }

    // ---- qualification conversion: spans<T...> -> spans<const T...> ---------
    template <typename... Us>
      requires(sizeof...(Us) == num_types && !(::std::same_as<Us, Ts> && ...) &&
               (::std::convertible_to<Us (*)[], Ts (*)[]> && ...))
    constexpr spans(const spans<Us...> &other) noexcept : size_(other.size()) {
      [&]<size_type... Is>(::std::index_sequence<Is...>) {
        pointers_ = ::std::tuple<Ts *...>{other.template data<Is>()...};
      }(::std::index_sequence_for<Ts...>{});
    }

    constexpr void swap(spans &other) noexcept {
      ::std::swap(pointers_, other.pointers_);
      ::std::swap(size_, other.size_);
    }

    VKTL_NODISCARD constexpr size_type size() const noexcept { return size_; }
    VKTL_NODISCARD constexpr bool empty() const noexcept { return size_ == 0; }
    VKTL_NODISCARD constexpr size_type size_bytes() const noexcept {
      return (size_ * ((sizeof(Ts)) + ...));
    }

    template <size_type I>
    VKTL_NODISCARD constexpr element_t<I> *data() const noexcept {
      return ::std::get<I>(pointers_);
    }
    template <typename T> VKTL_NODISCARD constexpr auto data() const noexcept {
      return data<type_index<T>()>();
    }

    template <size_type I>
    VKTL_NODISCARD constexpr element_t<I> &get(size_type index) const noexcept {
      VKTL_ASSERT(index < size_);
      return data<I>()[index];
    }
    template <typename T>
    VKTL_NODISCARD constexpr auto &get(size_type index) const noexcept {
      return get<type_index<T>()>(index);
    }

    template <size_type I>
    VKTL_NODISCARD constexpr auto column() const noexcept {
      return ::std::span{data<I>(), size_};
    }
    template <typename T>
    VKTL_NODISCARD constexpr auto column() const noexcept {
      return column<type_index<T>()>();
    }
    template <size_type I>
    VKTL_NODISCARD constexpr auto get_span() const noexcept {
      return column<I>();
    }

    VKTL_NODISCARD constexpr ::std::tuple<Ts &...>
    row(size_type index) const noexcept {
      VKTL_ASSERT(index < size_);
      return row_tuple(index, ::std::index_sequence_for<Ts...>{});
    }
    VKTL_NODISCARD constexpr ::std::tuple<Ts &...>
    operator[](size_type index) const noexcept {
      return row(index);
    }
    VKTL_NODISCARD constexpr ::std::tuple<Ts &...> front() const noexcept {
      VKTL_ASSERT(size_);
      return row(0u);
    }
    VKTL_NODISCARD constexpr ::std::tuple<Ts &...> back() const noexcept {
      VKTL_ASSERT(size_);
      return row(size_ - 1u);
    }

    template <::std::size_t I>
    VKTL_NODISCARD constexpr element_t<I> &front() noexcept {
      return column<I>().front();
    }
    template <::std::size_t I>
    VKTL_NODISCARD constexpr element_t<I> &front() const noexcept {
      return column<I>().front();
    }
    template <::std::size_t I>
    VKTL_NODISCARD constexpr element_t<I> &back() noexcept {
      return column<I>().back();
    }
    template <::std::size_t I>
    VKTL_NODISCARD constexpr element_t<I> &back() const noexcept {
      return column<I>().back();
    }

    // ---- sub-views ----------------------------------------------------------
    VKTL_NODISCARD constexpr spans
    subspan(size_type offset,
            size_type count = static_cast<size_type>(-1)) const noexcept {
      VKTL_ASSERT(offset <= size_);
      const auto n = ::std::min(count, size_ - offset);
      return [&]<size_type... Is>(::std::index_sequence<Is...>) {
        return spans{n, (data<Is>() + offset)...};
      }(::std::index_sequence_for<Ts...>{});
    }
    VKTL_NODISCARD constexpr spans first(size_type count) const noexcept {
      return subspan(0, count);
    }
    VKTL_NODISCARD constexpr spans last(size_type count) const noexcept {
      VKTL_ASSERT(count <= size_);
      return subspan(size_ - count, count);
    }
    VKTL_NODISCARD constexpr spans<const Ts...> as_const() const noexcept {
      return spans<const Ts...>{*this};
    }

    VKTL_NODISCARD constexpr bool operator==(const spans &other) const
      requires((::std::equality_comparable<::std::remove_const_t<Ts>> && ...))
    {
      if (other.size_ != size_) {
        return false;
      }
      for (size_type i = 0; i < size_; ++i) {
        const bool equal = [&]<size_type... Is>(::std::index_sequence<Is...>) {
          return ((get<Is>(i) == other.template get<Is>(i)) && ...);
        }(::std::index_sequence_for<Ts...>{});
        if (!equal) {
          return false;
        }
      }
      return true;
    }

    // element constness lives in Ts, so a span view has a single iterator
    // flavour
    using iterator = iterators<Ts...>;
    using const_iterator = iterators<Ts...>;

    VKTL_NODISCARD constexpr iterator begin() const noexcept {
      return iterator{pointers_};
    }
    VKTL_NODISCARD constexpr iterator end() const noexcept {
      return begin() + static_cast<difference_type>(size_);
    }
    VKTL_NODISCARD constexpr const_iterator cbegin() const noexcept {
      return begin();
    }
    VKTL_NODISCARD constexpr const_iterator cend() const noexcept {
      return end();
    }

  private:
    template <typename T> static consteval size_type type_index() noexcept {
      return find_if_same_v<T, types>;
    }

    template <size_type... Is>
    constexpr auto row_tuple(size_type index,
                             ::std::index_sequence<Is...>) const {
      return ::std::tie(get<Is>(index)...);
    }

    template <typename Owner> constexpr void adopt(Owner &owner) noexcept {
      [&]<size_type... Is>(::std::index_sequence<Is...>) {
        pointers_ = ::std::tuple<Ts *...>{owner.template data<Is>()...};
      }(::std::index_sequence_for<Ts...>{});
      size_ = owner.size();
    }

  private:
    ::std::tuple<Ts *...> pointers_{};
    size_type size_ = 0;
  };
  template <::std::ranges::contiguous_range... Rs>
  spans(Rs & ...) -> spans<::std::ranges::range_value_t<Rs>...>;
  template <typename... Us> spans(vectors<Us...> &) -> spans<Us...>;
  template <typename... Us> spans(const vectors<Us...> &) -> spans<const Us...>;

  inline constexpr struct by_default_ {
    template <typename T>
    constexpr operator T() const
        noexcept(::std::is_nothrow_default_constructible_v<T>)
      requires(::std::is_default_constructible_v<T>)
    {
      return T{};
    }
  } by_default{};
  template <typename... Ts> struct vectors {
    static_assert(sizeof...(Ts) > 0, "Vectors requires at least one type.");
    static_assert(((!::std::is_reference_v<Ts>) && ...),
                  "Vectors not allow reference.");
    static_assert(((!::std::is_const_v<Ts>) && ...) &&
                      ((!::std::is_volatile_v<Ts>) && ...),
                  "Vectors not allow const or volatile.");

    using types = ts<Ts...>;
    using size_type = ::std::size_t;
    using view_type = spans<Ts...>;
    using const_view_type = spans<Ts const...>;
    using iterator = iterators<Ts...>;
    using const_iterator = iterators<Ts const...>;
    static constexpr size_type num_types = sizeof...(Ts);

    constexpr vectors() = default;
    explicit vectors(size_type size)
      requires((::std::default_initializable<Ts> && ...))
    {
      resize(size);
    }
    template <typename... Args>
      requires(can_emplace<const Args &...>())
    explicit vectors(size_type size, Args const &...values) {
      resize(size, values...);
    }

    ~vectors() {
      clear();
      this->deallocate_buffer();
    }

    vectors(const vectors &other) {
      reserve(other.size_);
      copy_construct_from(other);
    }
    vectors &operator=(const vectors &other) {
      if (this != &other) {
        auto tmp = other;
        swap(tmp);
      }
      return *this;
    }
    vectors(vectors &&other) noexcept { swap(other); }
    vectors &operator=(vectors &&other) noexcept {
      if (this != &other) {
        clear();
        deallocate_buffer();
        swap(other);
      }
      return *this;
    }

    void swap(vectors &other) noexcept {
      ::std::swap(buffer_, other.buffer_);
      ::std::swap(size_, other.size_);
      ::std::swap(capacity_, other.capacity_);
    }

    VKTL_NODISCARD size_type size() const noexcept { return size_; }
    VKTL_NODISCARD size_type capacity() const noexcept { return capacity_; }
    VKTL_NODISCARD bool empty() const noexcept { return size_ == 0; }

    // ---- the view is now the single source of truth for element access ------

    VKTL_NODISCARD view_type view() noexcept { return raw_view().first(size_); }
    VKTL_NODISCARD const_view_type view() const noexcept {
      return const_view_type{raw_view().first(size_)};
    }
    operator view_type() noexcept { return view(); }
    operator const_view_type() const noexcept { return view(); }

    template <size_type I> VKTL_NODISCARD auto *data() noexcept {
      return raw_view().template data<I>();
    }
    template <size_type I> VKTL_NODISCARD const auto *data() const noexcept {
      return raw_view().template data<I>();
    }

    template <size_type I> VKTL_NODISCARD auto &get(size_type i) noexcept {
      return view().template get<I>(i);
    }
    template <size_type I>
    VKTL_NODISCARD const auto &get(size_type i) const noexcept {
      return view().template get<I>(i);
    }
    template <typename T> VKTL_NODISCARD T &get(size_type i) noexcept {
      return view().template get<type_index<T>()>(i);
    }
    template <typename T>
    VKTL_NODISCARD const T &get(size_type i) const noexcept {
      return view().template get<type_index<T>()>(i);
    }

    template <size_type I> VKTL_NODISCARD auto column() noexcept {
      return view().template column<I>();
    }
    template <size_type I> VKTL_NODISCARD auto column() const noexcept {
      return view().template column<I>();
    }
    template <size_type... Is>
      requires(sizeof...(Is) > 1u && ((Is < num_types) && ...))
    VKTL_NODISCARD auto column() noexcept {
      return spans<tuple_at_t<Is, types>...>{size_, data<Is>()...};
    }
    template <size_type... Is>
      requires(sizeof...(Is) > 1u && ((Is < num_types) && ...))
    VKTL_NODISCARD auto column() const noexcept {
      return spans<const tuple_at_t<Is, types>...>{size_, data<Is>()...};
    }
    template <size_type I> VKTL_NODISCARD auto get_span() noexcept {
      return column<I>();
    }
    template <size_type I> VKTL_NODISCARD auto get_span() const noexcept {
      return column<I>();
    }

    VKTL_NODISCARD::std::tuple<Ts &...> row(size_type i) {
      return view().row(i);
    }
    VKTL_NODISCARD::std::tuple<Ts const &...> row(size_type i) const {
      return view().row(i);
    }
    VKTL_NODISCARD::std::tuple<Ts &...> front() { return view().front(); }
    VKTL_NODISCARD::std::tuple<Ts const &...> front() const {
      return view().front();
    }
    VKTL_NODISCARD::std::tuple<Ts &...> back() { return view().back(); }
    VKTL_NODISCARD::std::tuple<Ts const &...> back() const {
      return view().back();
    }

    VKTL_NODISCARD iterator begin() noexcept { return view().begin(); }
    VKTL_NODISCARD iterator end() noexcept { return view().end(); }
    VKTL_NODISCARD const_iterator begin() const noexcept {
      return view().begin();
    }
    VKTL_NODISCARD const_iterator end() const noexcept { return view().end(); }
    VKTL_NODISCARD const_iterator cbegin() const noexcept { return begin(); }
    VKTL_NODISCARD const_iterator cend() const noexcept { return end(); }

    bool operator==(const vectors &other) const noexcept
      requires((::std::equality_comparable<Ts> && ...))
    {
      return this == &other || view() == other.view();
    }

    void reserve(size_type new_cap) {
      if (new_cap > capacity_)
        reallocate(new_cap);
    }
    void shrink_to_fit() {
      if (size_ < capacity_)
        reallocate(size_);
    }

    template <typename... Args>
      requires(can_emplace<Args...>())
    auto &emplace_back(Args &&...args) {
      return *emplace(end(), static_cast<Args &&>(args)...);
    }
    void push_back(const Ts &...args)
      requires((::std::copy_constructible<Ts> && ...)) {
      emplace_back(args...);
    }
    void push_back(Ts &&...args)
      requires((::std::move_constructible<Ts> && ...)) {
      emplace_back(::std::move(args)...);
    }
    void pop_back() noexcept {
      VKTL_ASSERT(!empty());
      destroy_row(--size_);
    }

    void resize(size_type new_size)
      requires((::std::default_initializable<Ts> && ...)) {
      if (new_size < size_) {
        shrink_to(new_size);
        return;
      }
      reserve(new_size);
      while (size_ < new_size) {
        construct_default_row(size_++);
      }
    }
    template <typename... Args>
      requires(can_emplace<const Args &...>())
    void resize(size_type new_size, const Args &...values) {
      if (new_size < size_) {
        shrink_to(new_size);
        return;
      }
      reserve(new_size);
      while (size_ < new_size) {
        emplace_back(values...);
      }
    }
    void clear() noexcept { shrink_to(0); }

    template <typename... Args>
      requires(can_emplace<Args...>())
    auto &emplace(const_iterator where, Args &&...args) {
      return *emplace_it(where, static_cast<Args &&>(args)...);
    }
    iterator insert(const_iterator where, Ts const &...args)
      requires((::std::copy_constructible<Ts> && ...)) {
      return emplace_it(where, args...);
    }
    iterator insert(const_iterator where, Ts &&...args)
      requires((::std::move_constructible<Ts> && ...)) {
      return emplace_it(where, ::std::move(args)...);
    }

    iterator erase(const_iterator where) {
      const auto pos = checked_index(where, false);
      move_rows_left(pos + 1, pos, size_ - pos - 1);
      destroy_row(--size_);
      return begin() + static_cast<::std::ptrdiff_t>(pos);
    }

  private:
    static constexpr size_type max_alignment = ::std::max({alignof(Ts)...});
    static constexpr ::std::index_sequence_for<Ts...> indices{};

    template <typename... Args>
      requires(can_emplace<Args...>())
    iterator emplace_it(const_iterator where, Args &&...args) {
      const auto pos = checked_index(where, true);
      if (size_ == capacity_) {
        emplace_reallocating(pos, ::std::forward<Args>(args)...);
      } else {
        emplace_in_place(pos, ::std::forward<Args>(args)...);
      }
      return begin() + static_cast<::std::ptrdiff_t>(pos);
    }

    template <typename... Args> static consteval bool can_emplace() {
      if constexpr (sizeof...(Args) > num_types) {
        return false;
      } else {
        using args_type = ::std::tuple<Args &&...>;
        constexpr bool provided =
            []<size_type... Is>(::std::index_sequence<Is...>) {
              return (can_initialize<tuple_at_t<Is, types>,
                                     ::std::tuple_element_t<Is, args_type>>() &&
                      ...);
            }(::std::index_sequence_for<Args...>{});
        constexpr bool omitted =
            []<size_type... Is>(::std::index_sequence<Is...>) {
              return (::std::default_initializable<
                          tuple_at_t<sizeof...(Args) + Is, types>> &&
                      ...);
            }(::std::make_index_sequence<num_types - sizeof...(Args)>{});
        return provided && omitted;
      }
    }
    template <typename T, typename Arg> static consteval bool can_initialize() {
      if constexpr (::std::same_as<::std::remove_cvref_t<Arg>, by_default>) {
        return ::std::default_initializable<T>;
      } else {
        return ::std::constructible_from<T, Arg>;
      }
    }
    template <typename... Args> static auto complete_row_args(Args &&...args) {
      return [&]<size_type... Is>(::std::index_sequence<Is...>) {
        return ::std::tuple_cat(
            ::std::forward_as_tuple(::std::forward<Args>(args)...),
            ::std::tuple{((void)Is, by_default)...});
      }(::std::make_index_sequence<num_types - sizeof...(Args)>{});
    }

    // one offsets computation, all N column pointers; covers uninitialised
    // slots
    view_type raw_view() const noexcept {
      if (!buffer_) {
        return {};
      }
      const auto offsets = get_offsets(capacity_);
      return [&]<size_type... Is>(::std::index_sequence<Is...>) {
        return view_type{
            capacity_, static_cast<tuple_at_t<Is, types> *>(
                           offset_address(buffer_, offsets[Is]))...};
      }(indices);
    }

    static void *offset_address(void *buffer, size_type offset) noexcept {
      return static_cast<::std::byte *>(buffer) + offset;
    }

    static auto get_offsets(size_type cap) noexcept {
      ::std::array<size_type, num_types> offsets{};
      size_type offset = 0, idx = 0;
      ((offset = align_up(offset, alignof(Ts)), offsets[idx++] = offset,
        offset += cap * sizeof(Ts)), ...);
      return offsets;
    }
    static size_type calc_total_bytes(size_type cap) noexcept {
      size_type offset = 0;
      ((offset = align_up(offset, alignof(Ts)), offset += cap * sizeof(Ts)), ...);
      return offset;
    }
    static void *allocate_raw(size_type cap) {
      return cap == 0 ? nullptr
                      : ::operator new(calc_total_bytes(cap),
                                       ::std::align_val_t(max_alignment));
    }
    static void deallocate_raw(void *buffer) noexcept {
      ::operator delete(buffer, ::std::align_val_t(max_alignment));
    }
    template <typename T> static consteval size_type type_index() noexcept {
      return find_if_same_v<T, types>;
    }

    // ---- row lifetime, all expressed against a raw view ---------------------
    template <typename Tuple>
    void construct_row_from(size_type index, Tuple &&args) {
      const auto v = raw_view();
      size_type constructed = 0;
      try {
        [&]<size_type... Is>(::std::index_sequence<Is...>) {
          ((construct_value(v.template data<Is>() + index,
                            ::std::get<Is>(::std::forward<Tuple>(args))),
            ++constructed),
           ...);
        }(indices);
      } catch (...) {
        destroy_row_prefix(v, index, constructed);
        throw;
      }
    }
    template <typename T, typename Arg>
    static void construct_value(T *where, Arg &&arg) {
      if constexpr (::std::same_as<::std::remove_cvref_t<Arg>, by_default>) {
        ::std::construct_at(where);
      } else {
        ::std::construct_at(where, ::std::forward<Arg>(arg));
      }
    }
    template <typename... Args>
    void construct_row(size_type index, Args &&...args) {
      auto complete = complete_row_args(::std::forward<Args>(args)...);
      construct_row_from(index, ::std::move(complete));
    }
    void construct_default_row(size_type index) {
      const auto v = raw_view();
      size_type constructed = 0;
      try {
        [&]<size_type... Is>(::std::index_sequence<Is...>) {
          ((::std::construct_at(v.template data<Is>() + index), ++constructed),
           ...);
        }(indices);
      } catch (...) {
        destroy_row_prefix(v, index, constructed);
        throw;
      }
    }
    static void destroy_row_prefix(const view_type &v, size_type index, size_type constructed) noexcept {
      [&]<size_type... Is>(::std::index_sequence<Is...>) {
        ((Is < constructed ? ::std::destroy_at(v.template data<Is>() + index)
                           : void()),
         ...);
      }(indices);
    }
    void destroy_row(size_type index) noexcept {
      destroy_row_prefix(raw_view(), index, num_types);
    }
    void shrink_to(size_type new_size) noexcept {
      while (size_ > new_size) {
        pop_back();
      }
    }

    void copy_construct_from(const vectors &other) {
      for (auto r : other.view()) {
        ::std::apply([this](const Ts &...vs) { emplace_back(vs...); }, r);
      }
    }
    void move_construct_row_into(vectors &target, size_type index) {
      ::std::apply([&](Ts&...vs) { target.emplace_back(::std::move(vs)...); }, view().row(index));
    }

    void reallocate(size_type new_cap) {
      // TODO:
      relocate_into(new_cap, size_, [](vectors&) {});
    }

    template <typename... Args>
    void emplace_reallocating(size_type pos, Args &&...args) {
      relocate_into(capacity_after_insert(), pos, [&](vectors &tmp) {
        tmp.construct_row(tmp.size_, ::std::forward<Args>(args)...);
        ++tmp.size_;
      });
    }

    // shared body of reallocate / emplace_reallocating
    template <typename Middle>
    void relocate_into(size_type new_cap, size_type split, Middle &&middle) {
      vectors tmp;
      tmp.buffer_ = allocate_raw(new_cap);
      tmp.capacity_ = new_cap;
      try {
        for (size_type i = 0; i < split; ++i) {
          move_construct_row_into(tmp, i);
        }
        middle(tmp);
        for (size_type i = split; i < size_; ++i) {
          move_construct_row_into(tmp, i);
        }
      } catch (...) {
        tmp.clear();
        tmp.deallocate_buffer();
        throw;
      }
      clear();
      deallocate_buffer();
      swap(tmp);
    }

    template <typename... Args>
    void emplace_in_place(size_type pos, Args &&...args) {
      if (pos == size_) {
        construct_row(size_, ::std::forward<Args>(args)...);
        ++size_;
        return;
      }
      [&]<size_type... Is>(::std::index_sequence<Is...>) {
        construct_row(size_, ::std::move(get<Is>(size_ - 1))...);
      }(indices);
      ++size_;
      try {
        move_rows_right(pos, size_ - 2, 1);
        assign_row(pos, ::std::forward<Args>(args)...);
      } catch (...) {
        --size_;
        destroy_row(size_);
        throw;
      }
    }

    void move_rows_right(size_type first, size_type last, size_type distance) {
      for (size_type i = last + 1; i-- > first;) {
        assign_row_from(i + distance, i);
      }
    }
    void move_rows_left(size_type first, size_type dest, size_type count) {
      for (size_type i = 0; i < count; ++i) {
        assign_row_from(dest + i, first + i);
      }
    }
    void assign_row_from(size_type dest, size_type src) {
      const auto v = view();
      [&]<size_type... Is>(::std::index_sequence<Is...>) {
        ((v.template get<Is>(dest) = ::std::move(v.template get<Is>(src))),
         ...);
      }(indices);
    }
    template <typename... Args>
    void assign_row(size_type index, Args &&...args) {
      const auto v = view();
      auto tuple = complete_row_args(::std::forward<Args>(args)...);
      [&]<size_type... Is>(::std::index_sequence<Is...>) {
        ((assign_value(v.template get<Is>(index),
                       ::std::get<Is>(::std::move(tuple)))),
         ...);
      }(indices);
    }
    template <typename T, typename Arg>
    static void assign_value(T &target, Arg &&arg) {
      if constexpr (::std::same_as<::std::remove_cvref_t<Arg>, by_default>) {
        target = T{};
      } else {
        target = ::std::forward<Arg>(arg);
      }
    }

    size_type capacity_after_insert() const noexcept {
      return size_ == capacity_ ? (capacity_ == 0 ? 1 : capacity_ * 2)
                                : capacity_;
    }
    size_type checked_index(const_iterator where,
                            bool allow_end) const noexcept {
      const auto offset = where - cbegin();
      VKTL_ASSERT(offset >= 0);
      const auto index = static_cast<size_type>(offset);
      VKTL_ASSERT(index < size_ || (allow_end && index == size_));
      return index;
    }
    void deallocate_buffer() noexcept {
      if (buffer_) {
        deallocate_raw(buffer_);
        buffer_ = nullptr;
        capacity_ = 0;
      }
    }

    void *buffer_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
  };

  // A hash-ordered red-black tree with a collision chain per hash. Each
  // allocation contains one key's node header followed by its contiguous T
  // objects. Growing one key replaces its allocation and invalidates iterators,
  // spans, and references into that key.
  template <typename Key, typename T, typename Hash = ::std::hash<Key>,
            typename KeyEqual = ::std::equal_to<Key>,
            typename Allocator = ::std::allocator<::std::pair<const Key, T>>>
  class mumap {
  public:
    using key_type = Key;
    using element_type = T;
    using mapped_type = ::std::span<element_type>;
    using value_type = ::std::pair<const key_type, mapped_type>;
    using insert_type = ::std::pair<const key_type, element_type>;
    using size_type = ::std::size_t;
    using difference_type = ::std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using reference = ::std::pair<const key_type &, mapped_type>;
    using const_reference =
        ::std::pair<const key_type &, ::std::span<const element_type>>;
    using pointer = value_type *;
    using const_pointer = const value_type *;

  private:
    struct alignas(element_type) node {
      void *parent{}, *left{}, *right{};
      void *collision_prev{}, *collision_next{};
      size_type hash{}, count{}, capacity{};
      bool red = true;
      key_type key;

      template <typename K>
      explicit node(size_type value_hash, K &&value_key)
          : hash{value_hash}, key{::std::forward<K>(value_key)} {}
    };

    using node_allocator 
      = typename::std::allocator_traits<allocator_type>::template rebind_alloc<node>;
    using node_allocator_traits = ::std::allocator_traits<node_allocator>;
    using element_allocator = typename ::std::allocator_traits<
        allocator_type>::template rebind_alloc<element_type>;
    using element_allocator_traits = ::std::allocator_traits<element_allocator>;
    static_assert(
        ::std::same_as<typename node_allocator_traits::pointer, node*>,
        "mumap requires an allocator whose rebound pointer is a raw pointer.");

    template <bool IsConst> class basic_iterator {
      friend mumap;
      template <bool> friend class basic_iterator;
      using owner_type = ::std::conditional_t<IsConst, const mumap, mumap>;

      constexpr basic_iterator(owner_type *owner, void *current) noexcept
          : owner_{owner}, node_{current} {}

      template <typename Reference> struct arrow_proxy {
        Reference value;
        const Reference *operator->() const noexcept { return ::std::addressof(value); }
      };

    public:
      using iterator_category = ::std::forward_iterator_tag;
      using iterator_concept = ::std::forward_iterator_tag;
      using value_type = typename mumap::value_type;
      using difference_type = typename mumap::difference_type;
      using reference = ::std::conditional_t<
          IsConst, typename mumap::const_reference, typename mumap::reference>;
      using pointer = arrow_proxy<reference>;

      constexpr basic_iterator() noexcept = default;
      constexpr basic_iterator(const basic_iterator &) noexcept = default;
      constexpr basic_iterator &
      operator=(const basic_iterator &) noexcept = default;

      template <bool OtherConst>
        requires(IsConst && !OtherConst)
      constexpr basic_iterator(const basic_iterator<OtherConst> &other) noexcept
          : owner_{other.owner_}, node_{other.node_} {}

      VKTL_NODISCARD reference operator*() const noexcept {
        return owner_->value_at(node_);
      }
      VKTL_NODISCARD pointer operator->() const noexcept { return {operator*()}; }

      basic_iterator &operator++() noexcept {
        owner_->advance(*this);
        return *this;
      }
      basic_iterator operator++(int) noexcept {
        auto result = *this;
        ++*this;
        return result;
      }

      template <bool OtherConst>
      constexpr bool operator==(const basic_iterator<OtherConst> &other) const noexcept {
        return owner_ == other.owner_ && node_ == other.node_;
      }

    private:
      owner_type *owner_ = nullptr;
      void *node_ = nullptr;
    };

  public:
    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

    mumap(const hasher &hash = hasher{}, const key_equal &equal = key_equal{},
          const allocator_type &allocator = allocator_type{})
        : hash_{hash}, equal_{equal}, allocator_{allocator} {}

    explicit mumap(const allocator_type &allocator) : allocator_{allocator} {}

    template <::std::input_iterator InputIt>
    mumap(InputIt first, InputIt last, const hasher &hash = hasher{},
          const key_equal &equal = key_equal{},
          const allocator_type &allocator = allocator_type{})
        : mumap(hash, equal, allocator) {
      try {
        insert(first, last);
      } catch (...) {
        clear();
        throw;
      }
    }

    mumap(::std::initializer_list<insert_type> values,
          const hasher &hash = hasher{}, const key_equal &equal = key_equal{},
          const allocator_type &allocator = allocator_type{})
        : mumap(values.begin(), values.end(), hash, equal, allocator) {}

    mumap(const mumap &other)
        : mumap(other.hash_, other.equal_,
                ::std::allocator_traits<allocator_type>::
                    select_on_container_copy_construction(other.allocator_)) {
      try {
        insert_groups(other);
      } catch (...) {
        clear();
        throw;
      }
    }

    mumap(mumap &&other) noexcept(
        ::std::is_nothrow_move_constructible_v<hasher> &&
        ::std::is_nothrow_move_constructible_v<key_equal> &&
        ::std::is_nothrow_move_constructible_v<allocator_type>)
        : root_{::std::exchange(other.root_, nullptr)},
          size_{::std::exchange(other.size_, 0u)},
          hash_{::std::move(other.hash_)}, equal_{::std::move(other.equal_)},
          allocator_{::std::move(other.allocator_)} {}

    ~mumap() { clear(); }

    mumap &operator=(const mumap &other) {
      if (this == ::std::addressof(other)) return *this;
      if constexpr (::std::allocator_traits<allocator_type>::
                        propagate_on_container_copy_assignment::value) {
        if (allocator_ != other.allocator_) {
          clear();
        }
        allocator_ = other.allocator_;
      }
      hash_ = other.hash_;
      equal_ = other.equal_;
      clear();
      insert_groups(other);
      return *this;
    }

    mumap &operator=(mumap &&other) noexcept(
        (::std::allocator_traits<
             allocator_type>::propagate_on_container_move_assignment::value
             ? ::std::is_nothrow_move_assignable_v<allocator_type>
             : ::std::allocator_traits<
                   allocator_type>::is_always_equal::value) &&
        ::std::is_nothrow_move_assignable_v<hasher> &&
        ::std::is_nothrow_move_assignable_v<key_equal>) {
      if (this == ::std::addressof(other)) return *this;
      if constexpr (::std::allocator_traits<allocator_type>
        ::propagate_on_container_move_assignment::value) {
        clear();
        allocator_ = ::std::move(other.allocator_);
        steal_from(other);
      } else if (allocator_ == other.allocator_) {
        clear();
        steal_from(other);
      } else {
        clear();
        hash_ = ::std::move(other.hash_);
        equal_ = ::std::move(other.equal_);
        for (auto group : other) {
          for (auto &value : group.second) {
            insert(insert_type{group.first, ::std::move(value)});
          }
        }
        other.clear();
      }
      return *this;
    }

    mumap &operator=(::std::initializer_list<insert_type> values) {
      assign(values);
      return *this;
    }

    VKTL_NODISCARD allocator_type get_allocator() const noexcept { return allocator_; }

    VKTL_NODISCARD iterator begin() noexcept {
      return root_ ? iterator{this, minimum(root_)} : end();
    }
    VKTL_NODISCARD const_iterator begin() const noexcept {
      return root_ ? const_iterator{this, minimum(root_)} : end();
    }
    VKTL_NODISCARD const_iterator cbegin() const noexcept { return begin(); }
    VKTL_NODISCARD iterator end() noexcept { return {this, nullptr}; }
    VKTL_NODISCARD const_iterator end() const noexcept { return {this, nullptr}; }
    VKTL_NODISCARD const_iterator cend() const noexcept { return end(); }

    VKTL_NODISCARD bool empty() const noexcept { return size_ == 0u; }
    VKTL_NODISCARD size_type size() const noexcept { return size_; }
    VKTL_NODISCARD size_type max_size() const noexcept {
      return::std::allocator_traits<allocator_type>::max_size(allocator_);
    }

    void clear() noexcept {
      destroy_subtree(root_);
      root_ = nullptr;
      size_ = 0u;
    }

    iterator insert(const insert_type &value) { return insert_owned(insert_type{value}); }
    iterator insert(insert_type &&value) { return insert_owned(::std::move(value)); }

    template <typename P> requires(::std::constructible_from<insert_type, P &&>)
    iterator insert(P &&value) {
      return insert_owned(insert_type{::std::forward<P>(value)});
    }

    iterator insert(const_iterator, const insert_type &value) { return insert(value); }
    iterator insert(const_iterator, insert_type &&value) { return insert(::std::move(value)); }

    template <typename P> requires(::std::constructible_from<insert_type, P &&>)
    iterator insert(const_iterator, P &&value) {
      return insert(::std::forward<P>(value));
    }

    template <::std::input_iterator InputIt>
    void insert(InputIt first, InputIt last) {
      for (; first != last; ++first) insert(*first);
    }

    void insert(::std::initializer_list<insert_type> values) { insert(values.begin(), values.end()); }

    template <typename... Args>
      requires ::std::constructible_from<insert_type, Args && ...>
    iterator emplace(Args &&...args) {
      return insert_owned(insert_type{::std::forward<Args>(args)...});
    }

    template <typename... Args>
      requires ::std::constructible_from<insert_type, Args && ...>
    iterator emplace_hint(const_iterator, Args &&...args) {
      return emplace(::std::forward<Args>(args)...);
    }

    template <typename... Args>
    iterator try_emplace(const key_type &key, Args &&...args) {
      return emplace(::std::piecewise_construct, ::std::forward_as_tuple(key),
                     ::std::forward_as_tuple(::std::forward<Args>(args)...));
    }

    template <typename... Args>
    iterator try_emplace(key_type &&key, Args &&...args) {
      return emplace(::std::piecewise_construct,
                     ::std::forward_as_tuple(::std::move(key)),
                     ::std::forward_as_tuple(::std::forward<Args>(args)...));
    }

    template <typename M>
      requires(::std::constructible_from<element_type, M&&>)
    ::std::pair<iterator, bool> insert_or_assign(const key_type &key,
                                                 M &&mapped) {
      insert_type replacement{key, ::std::forward<M>(mapped)};
      const bool inserted = erase(key) == 0u;
      return {insert_owned(::std::move(replacement)), inserted};
    }

    template <typename M>
      requires(::std::constructible_from<element_type, M &&>)
    ::std::pair<iterator, bool>
    insert_or_assign(key_type &&key, M &&mapped) {
      insert_type replacement{::std::move(key), ::std::forward<M>(mapped)};
      const bool inserted = erase(replacement.first) == 0u;
      return {insert_owned(::std::move(replacement)), inserted};
    }

    template <typename M>
      requires(::std::constructible_from<element_type, M &&>)
    ::std::pair<iterator, bool> assign(const key_type &key, M &&mapped) {
      return insert_or_assign(key, ::std::forward<M>(mapped));
    }

    template <typename M>
      requires(::std::constructible_from<element_type, M&&>)
    ::std::pair<iterator, bool> assign(key_type &&key, M &&mapped) {
      return insert_or_assign(::std::move(key), ::std::forward<M>(mapped));
    }

    template <::std::input_iterator InputIt>
    void assign(InputIt first, InputIt last) {
      mumap replacement{hash_, equal_, allocator_};
      replacement.insert(first, last);
      swap(replacement);
    }

    void assign(::std::initializer_list<insert_type> values) { assign(values.begin(), values.end()); }

    iterator erase(const_iterator where) {
      VKTL_ASSERT(where.owner_ == this && where.node_);
      auto result = where;
      ++result;
      auto *current = where.node_;
      unlink_node(current);
      destroy_node(current);
      --size_;
      return iterator_from(result);
    }

    iterator erase(const_iterator first, const_iterator last) {
      VKTL_ASSERT(first.owner_ == this && last.owner_ == this);
      size_type count_to_erase = 0u;
      for (auto it = first; it != last; ++it) ++count_to_erase;
      auto result = iterator_from(first);
      while (count_to_erase-- != 0u) result = erase(result);
      return result;
    }

    size_type erase(const key_type &key) {
      auto *current = find_node(key);
      if (!current) return 0u;
      unlink_node(current);
      destroy_node(current);
      --size_;
      return 1u;
    }

    void swap(mumap &other) noexcept(
        (::std::allocator_traits<
             allocator_type>::propagate_on_container_swap::value ||
         ::std::allocator_traits<allocator_type>::is_always_equal::value) &&
        (!::std::allocator_traits<
             allocator_type>::propagate_on_container_swap::value ||
         ::std::is_nothrow_swappable_v<allocator_type>) &&
        ::std::is_nothrow_swappable_v<hasher> &&
        ::std::is_nothrow_swappable_v<key_equal>) {
      using::std::swap;
      if constexpr (::std::allocator_traits<allocator_type>::
                        propagate_on_container_swap::value) {
        swap(allocator_, other.allocator_);
      } else VKTL_ASSERT(allocator_ == other.allocator_);
      swap(root_, other.root_);
      swap(size_, other.size_);
      swap(hash_, other.hash_);
      swap(equal_, other.equal_);
    }

    VKTL_NODISCARD iterator find(const key_type &key) {
      return iterator{this, find_node(key)};
    }

    VKTL_NODISCARD const_iterator find(const key_type &key) const {
      return const_iterator{this, find_node(key)};
    }

    VKTL_NODISCARD bool contains(const key_type &key) const { return find_node(key); }
    VKTL_NODISCARD size_type count(const key_type &key) const { return contains(key); }

    VKTL_NODISCARD hasher hash_function() const { return hash_; }
    VKTL_NODISCARD key_equal key_eq() const { return equal_; }

    friend bool operator==(const mumap &a, const mumap &b)
      requires(::std::equality_comparable<key_type> &&
               ::std::equality_comparable<element_type>)
    {
      if (a.size() != b.size()) return false;
      for (auto value : a) {
        const auto found = b.find(value.first);
        if (found == b.end() ||
            !::std::ranges::equal(value.second, found->second)) return false;
      }
      return true;
    }

  private:
    static constexpr size_type node_units(size_type capacity) noexcept {
      const auto bytes = sizeof(node) + capacity * sizeof(element_type);
      return (bytes + sizeof(node) - 1u) / sizeof(node);
    }
    static constexpr node &head(void *storage) noexcept {
      return *::std::launder(static_cast<node *>(storage));
    }
    static constexpr node const &head(const void *storage) noexcept {
      return *::std::launder(static_cast<const node *>(storage));
    }
    static constexpr element_type *storage_pointer(void *current, size_type index) noexcept {
      void *storage = static_cast<node *>(current) + 1;
      return static_cast<element_type *>(storage) + index;
    }
    static constexpr element_type *value_pointer(void *current, size_type index) noexcept {
      return ::std::launder(storage_pointer(current, index));
    }
    static constexpr element_type const* value_pointer(const void *current, size_type index) noexcept {
      return value_pointer(const_cast<void *>(current), index);
    }

    reference value_at(void *current) noexcept {
      auto &current_head = head(current);
      return {current_head.key,
        mapped_type{ value_pointer(current, 0u), current_head.count }};
    }
    const_reference value_at(const void *current) const noexcept {
      const auto &current_head = head(current);
      return { current_head.key, 
        ::std::span<const element_type>{ 
          value_pointer(current, 0u), current_head.count}};
    }

    template <typename K>
    void *allocate_node(size_type hash, K &&key, size_type capacity) {
      node_allocator allocator{allocator_};
      const auto units = node_units(capacity);
      auto *allocation = node_allocator_traits::allocate(allocator, units);
      void *result = allocation;
      try {
        ::std::construct_at(static_cast<node*>(result), 
          hash, ::std::forward<K>(key));
      } catch (...) {
        node_allocator_traits::deallocate(allocator, allocation, units);
        throw;
      }
      head(result).capacity = capacity;
      return result;
    }

    void deallocate_node(void *current) noexcept {
      auto &current_head = head(current);
      const auto units = node_units(current_head.capacity);
      ::std::destroy_at(::std::addressof(current_head));
      node_allocator allocator{allocator_};
      node_allocator_traits::deallocate(allocator, static_cast<node*>(current), units);
    }

    void destroy_node(void *current) noexcept {
      auto &current_head = head(current);
      element_allocator allocator{allocator_};
      while (current_head.count != 0u) {
        --current_head.count;
        element_allocator_traits::destroy(
            allocator, value_pointer(current, current_head.count));
      }
      deallocate_node(current);
    }

    void destroy_subtree(void *current) noexcept {
      if (!current) return;
      auto &current_head = head(current);
      auto *left = current_head.left;
      auto *right = current_head.right;
      destroy_subtree(left);
      destroy_subtree(right);
      auto *collision = current_head.collision_next;
      while (collision) {
        auto *next = head(collision).collision_next;
        destroy_node(collision);
        collision = next;
      }
      destroy_node(current);
    }

    template <typename V>
    void construct_value(void *current, size_type index, V &&value) {
      element_allocator allocator{allocator_};
      element_allocator_traits::construct(
          allocator, storage_pointer(current, index), ::std::forward<V>(value));
    }

    void *create_node(size_type hash, insert_type &&value, size_type capacity = 1u) {
      auto *result = allocate_node(hash, value.first, capacity);
      try {
        construct_value(result, 0u, ::std::move(value.second));
        head(result).count = 1u;
      } catch (...) {
        deallocate_node(result);
        throw;
      }
      return result;
    }

    void *grow_and_append(void *current, element_type &&value) {
      auto &current_head = head(current);
      const auto new_capacity =
          ::std::max(size_type{1u}, current_head.capacity * 2u);
      auto *replacement =
          allocate_node(current_head.hash, current_head.key, new_capacity);
      auto &replacement_head = head(replacement);
      try {
        for (; replacement_head.count < current_head.count;
             ++replacement_head.count) {
          const auto index = replacement_head.count;
          construct_value(replacement, index, 
            ::std::move_if_noexcept(*value_pointer(current, index)));
        }
        construct_value(replacement, replacement_head.count, ::std::move(value));
        ++replacement_head.count;
      } catch (...) {
        destroy_node(replacement);
        throw;
      }
      replace_storage(current, replacement);
      destroy_node(current);
      return replacement;
    }

    iterator insert_owned(insert_type &&value) {
      const auto hash = hash_(value.first);
      if (auto *representative = find_hash(hash)) {
        auto *current = representative;
        while (current) {
          const auto &current_head = head(current);
          if (equal_(current_head.key, value.first)) break;
          current = current_head.collision_next;
        }
        if (!current) {
          current = create_node(hash, ::std::move(value));
          auto &representative_head = head(representative);
          auto *next = representative_head.collision_next;
          auto &current_head = head(current);
          current_head.collision_prev = representative;
          current_head.collision_next = next;
          representative_head.collision_next = current;
          if (next) head(next).collision_prev = current;
          ++size_;
        } else {
          auto &current_head = head(current);
          if (current_head.count == current_head.capacity) {
            current = grow_and_append(current, ::std::move(value.second));
          } else {
            construct_value(current, current_head.count,
                            ::std::move(value.second));
            ++current_head.count;
          }
        }
        return iterator{this, current};
      }
      auto *current = create_node(hash, ::std::move(value));
      insert_tree_node(current);
      ++size_;
      return iterator{this, current};
    }

    void *find_hash(size_type hash) const noexcept {
      auto *current = root_;
      while (current) {
        const auto &current_head = head(current);
        if (hash == current_head.hash) return current;
        current = hash < current_head.hash ? current_head.left
                                           : current_head.right;
      }
      return nullptr;
    }

    void *find_node(const key_type &key) const noexcept {
      auto *current = find_hash(hash_(key));
      while (current) {
        const auto &current_head = head(current);
        if (equal_(current_head.key, key)) break;
        current = current_head.collision_next;
      }
      return current;
    }

    static constexpr bool is_red(void *current) noexcept { return current && head(current).red; }
    static constexpr bool is_black(void *current) noexcept { return !is_red(current); }
    static constexpr void set_red(void *current) noexcept { if (current) head(current).red = true; }
    static constexpr void set_black(void *current) noexcept { if (current) head(current).red = false; }

    void rotate_left(void *current) noexcept {
      auto& cur = head(current);
      auto *pivot = cur.right;
      VKTL_ASSERT(pivot);
      auto& pvt = head(pivot);
      auto *pivot_left = pvt.left;
      cur.right = pivot_left;
      if (pivot_left) head(pivot_left).parent = current;
      pvt.parent = cur.parent;
      if (!cur.parent) root_ = pivot;
      else {
        auto &parent = head(cur.parent);
        (current == parent.left ? parent.left : parent.right) = pivot;
      }
      pvt.left = current;
      cur.parent = pivot;
    }

    void rotate_right(void *current) noexcept {
      auto& cur = head(current);
      VKTL_ASSERT(cur.left);
      auto pivot = cur.left;
      auto& pvt = head(pivot);

      auto *pivot_right = pvt.right;
      cur.left = pivot_right;
      if (pivot_right) head(pivot_right).parent = current;
      pvt.parent = cur.parent;
      if (!cur.parent) root_ = pivot;
      else {
        auto &parent = head(cur.parent);
        (current == parent.right ? parent.right : parent.left) = pivot;
      }
      pvt.right = current;
      cur.parent = pivot;
    }

    void insert_tree_node(void *current) noexcept {
      auto &cur = head(current);
      void *parent = nullptr;
      auto *position = root_;
      while (position) {
        parent = position;
        const auto &pos = head(position);
        VKTL_ASSERT(cur.hash != pos.hash);
        position = cur.hash < pos.hash ? pos.left : pos.right;
      }
      cur.parent = parent;
      if (!parent) root_ = current;
      else {
        auto &parent_head = head(parent);
        (cur.hash < parent_head.hash ? parent_head.left : parent_head.right) =
            current;
      }
      insert_fixup(current);
    }

    void insert_fixup(void *current) noexcept {
      while (true) {
        auto &current_head = head(current);
        auto *parent = current_head.parent;
        if (!is_red(parent)) break;
        auto *grandparent = head(parent).parent;
        auto &grandparent_head = head(grandparent);
        if (parent == grandparent_head.left) {
          auto *uncle = grandparent_head.right;
          if (is_red(uncle)) {
            set_black(parent);
            set_black(uncle);
            set_red(grandparent);
            current = grandparent;
          } else {
            auto &parent_head = head(parent);
            if (current == parent_head.right) {
              current = parent;
              rotate_left(current);
              parent = head(current).parent;
              grandparent = head(parent).parent;
            }
            set_black(parent);
            set_red(grandparent);
            rotate_right(grandparent);
          }
        } else {
          auto *uncle = grandparent_head.left;
          if (is_red(uncle)) {
            set_black(parent);
            set_black(uncle);
            set_red(grandparent);
            current = grandparent;
          } else {
            auto &parent_head = head(parent);
            if (current == parent_head.left) {
              current = parent;
              rotate_right(current);
              parent = head(current).parent;
              grandparent = head(parent).parent;
            }
            set_black(parent);
            set_red(grandparent);
            rotate_left(grandparent);
          }
        }
      }
      set_black(root_);
    }

    void transplant(void *old_node, void *replacement) noexcept {
      auto &old = head(old_node);
      if (!old.parent) root_ = replacement;
      else {
        auto &parent = head(old.parent);
        (old_node == parent.left ? parent.left : parent.right) = replacement;
      }
      if (replacement) head(replacement).parent = old.parent;
    }

    static void *minimum(void *current) noexcept {
      while (auto *left = head(current).left) {
        current = left;
      }
      return current;
    }

    static void *successor(void *current) noexcept {
      auto *representative = current;
      auto &current_head = head(current);
      if (current_head.collision_next) return current_head.collision_next;
      while (auto *previous = head(representative).collision_prev) {
        representative = previous;
      }
      auto &representative_head = head(representative);
      if (representative_head.right) return minimum(representative_head.right);
      auto *parent = representative_head.parent;
      while (parent) {
        const auto &parent_head = head(parent);
        if (representative != parent_head.right) break;
        representative = parent;
        parent = parent_head.parent;
      }
      return parent;
    }

    void unlink_node(void *current) noexcept {
      auto &cur = head(current);
      if (cur.collision_prev) {
        auto &previous = head(cur.collision_prev);
        previous.collision_next = cur.collision_next;
        if (cur.collision_next) {
          head(cur.collision_next).collision_prev = cur.collision_prev;
        }
      } else if (cur.collision_next) {
        auto *replacement = cur.collision_next;
        auto &replacement_head = head(replacement);
        replacement_head.collision_prev = nullptr;
        replacement_head.parent = cur.parent;
        replacement_head.left = cur.left;
        replacement_head.right = cur.right;
        replacement_head.red = cur.red;
        if (replacement_head.left) {
          head(replacement_head.left).parent = replacement;
        }
        if (replacement_head.right) {
          head(replacement_head.right).parent = replacement;
        }
        if (!replacement_head.parent) {
          root_ = replacement;
        } else {
          auto &parent_head = head(replacement_head.parent);
          (parent_head.left == current ? parent_head.left : parent_head.right) =
              replacement;
        }
      } else {
        erase_tree_node(current);
      }
      cur.parent = cur.left = cur.right = nullptr;
      cur.collision_prev = cur.collision_next = nullptr;
    }

    void erase_tree_node(void *current) noexcept {
      auto *removed = current;
      auto &cur = head(current);
      bool removed_was_red = cur.red;
      void *child = nullptr;
      void *child_parent = nullptr;
      if (!cur.left) {
        child = cur.right;
        child_parent = cur.parent;
        transplant(current, child);
      } else if (!cur.right) {
        child = cur.left;
        child_parent = cur.parent;
        transplant(current, child);
      } else {
        removed = minimum(cur.right);
        auto &removed_head = head(removed);
        removed_was_red = removed_head.red;
        child = removed_head.right;
        if (removed_head.parent == current) {
          child_parent = removed;
          if (child) head(child).parent = removed;
        } else {
          child_parent = removed_head.parent;
          transplant(removed, child);
          removed_head.right = cur.right;
          head(removed_head.right).parent = removed;
        }
        transplant(current, removed);
        removed_head.left = cur.left;
        head(removed_head.left).parent = removed;
        removed_head.red = cur.red;
      }
      if (!removed_was_red) erase_fixup(child, child_parent);
      cur.parent = cur.left = cur.right = nullptr;
    }

    void erase_fixup(void *current, void *parent) noexcept {
      while (current != root_ && is_black(current)) {
        if (!parent) break;
        auto &parent_head = head(parent);
        if (current == parent_head.left) {
          auto *sibling = parent_head.right;
          if (is_red(sibling)) {
            set_black(sibling);
            set_red(parent);
            rotate_left(parent);
            sibling = parent_head.right;
          }
          void *sibling_left = nullptr;
          void *sibling_right = nullptr;
          if (sibling) {
            const auto &sibling_head = head(sibling);
            sibling_left = sibling_head.left;
            sibling_right = sibling_head.right;
          }
          if (is_black(sibling_left) && is_black(sibling_right)) {
            set_red(sibling);
            current = parent;
            parent = parent_head.parent;
          } else {
            if (is_black(sibling_right)) {
              set_black(sibling_left);
              set_red(sibling);
              rotate_right(sibling);
              sibling = parent_head.right;
            }
            auto &sibling_head = head(sibling);
            sibling_head.red = parent_head.red;
            set_black(parent);
            set_black(sibling_head.right);
            rotate_left(parent);
            current = root_;
            parent = nullptr;
          }
        } else {
          auto *sibling = parent_head.left;
          if (is_red(sibling)) {
            set_black(sibling);
            set_red(parent);
            rotate_right(parent);
            sibling = parent_head.left;
          }
          void *sibling_right = nullptr;
          void *sibling_left = nullptr;
          if (sibling) {
            const auto &sibling_head = head(sibling);
            sibling_right = sibling_head.right;
            sibling_left = sibling_head.left;
          }
          if (is_black(sibling_right) && is_black(sibling_left)) {
            set_red(sibling);
            current = parent;
            parent = parent_head.parent;
          } else {
            if (is_black(sibling_left)) {
              set_black(sibling_right);
              set_red(sibling);
              rotate_left(sibling);
              sibling = parent_head.left;
            }
            auto &sibling_head = head(sibling);
            sibling_head.red = parent_head.red;
            set_black(parent);
            set_black(sibling_head.left);
            rotate_right(parent);
            current = root_;
            parent = nullptr;
          }
        }
      }
      set_black(current);
    }

    void replace_storage(void *current, void *replacement) noexcept {
      auto &rpl = head(replacement);
      auto &cur = head(current);
      rpl.parent = cur.parent;
      rpl.left = cur.left;
      rpl.right = cur.right;
      rpl.collision_prev = cur.collision_prev;
      rpl.collision_next = cur.collision_next;
      rpl.red = cur.red;
      if (rpl.collision_prev) head(rpl.collision_prev).collision_next = replacement;
      if (rpl.collision_next) head(rpl.collision_next).collision_prev = replacement;
      if (rpl.left) head(rpl.left).parent = replacement;
      if (rpl.right) head(rpl.right).parent = replacement;
      if (rpl.collision_prev) return;
      if (!rpl.parent) root_ = replacement;
      else {
        auto &parent = head(rpl.parent);
        VKTL_ASSERT(parent.left == current || parent.right == current);
        (parent.left == current ? parent.left : parent.right) = replacement;
      }
    }

    template <bool IsConst>
    void advance(basic_iterator<IsConst> &iterator) const noexcept {
      VKTL_ASSERT(iterator.node_);
      iterator.node_ = successor(iterator.node_);
    }

    iterator iterator_from(const_iterator source) noexcept {
      return iterator{this, source.node_};
    }

    void insert_groups(const mumap &other) {
      for (auto group : other) {
        for (const auto &value : group.second) {
          insert(insert_type{group.first, value});
        }
      }
    }

    void steal_from(mumap &other) {
      root_ = ::std::exchange(other.root_, nullptr);
      size_ = ::std::exchange(other.size_, 0u);
      hash_ = ::std::move(other.hash_);
      equal_ = ::std::move(other.equal_);
    }

    friend void swap(mumap &a, mumap &b)
      noexcept(noexcept(a.swap(b))) {
      a.swap(b);
    }

  private:
    void *root_ = nullptr;
    size_type size_ = 0u;
    VKTL_NO_UNIQUE_ADDRESS hasher hash_{};
    VKTL_NO_UNIQUE_ADDRESS key_equal equal_{};
    VKTL_NO_UNIQUE_ADDRESS allocator_type allocator_{};
  };

  template <typename T> struct access_list {
    using range_type = typename T::range_type;

    constexpr access_list() = default;

    void insert(T access) {
      auto &vec = this->accesses_;
      auto itb = vec.end() - 1u;
      while (itb != vec.begin() - 1u && itb->stages != access.stages) {
        itb--;
      }
      if (itb == vec.begin() - 1u) {
        vec.emplace_back(::std::move(access));
        return;
      }
      auto ite = itb - 1u;
      while (ite != vec.begin() - 1u && ite->stages == access.stages) {
        ite--;
      }

      list<T> stack;
      stack.emplace_front(::std::move(access));
      for (; itb != ite;) {
        auto c = ::std::move(stack.front());
        stack.erase(stack.begin());
        if (c.stages == itb->stages && itb->dependencies == c.dependencies) {
          auto &it_range = static_cast<range_type const &>(*itb);
          auto &c_range = static_cast<range_type const &>(c);
          if (*itb == c && subres.adjacent_intersect(it_range, c_range)) {
            static_cast<range_type &>(c) = subres.merge(it_range, c_range);
            itb = vec.erase(itb);
          } else if (subres.intersected(it_range, c_range)) {
            auto intersected = subres.get_intersect(it_range, c_range);

            auto &a = *itb;
            static_cast<range_type &>(a) = intersected;
            a |= c;

            bool can_break = !stack.size();
            for (auto non_intersected :
                 subres.get_not_intersected(it_range, c_range)) {
              auto slice = non_intersected.is_first ? *itb : c;
              static_cast<range_type &>(slice) =
                  static_cast<range_type &&>(non_intersected);
              if (!non_intersected.is_first && !subres.empty(slice)) {
                vec.insert(itb, ::std::move(slice));
                can_break = false;
              } else {
                itb = vec.insert(itb, slice);
              }
            }
            if (can_break) {
              break;
            }
          }
          itb--;
        }
      }
      itb++; // if at ite, then itb is not stage, inc to make insert operation
             // correct.
      for (auto &&v : ::std::move(stack)) {
        itb = vec.insert(itb, ::std::move(v));
      }
    }

  private:
    vector<T> accesses_;
  };

  struct lock_duck_;
  template <typename T> struct locked : move_only<T> {
    using base = move_only<T>;

    // these for handles.

    locked(T *&value, uint32_t index, lock_duck_ const &lock)
        : base{value[index]}, plock_{nullptr} {}
    locked(T *&value, uint32_t index, ::std::mutex &lock)
        : locked{this->lock(&lock), index, value} {}

    locked(T &value, uint32_t index, ::std::mutex *lock)
        : locked{this->lock(lock), index, value} {}

    // these for handle.
    locked(T &value, lock_duck_ const &lock) : base{value}, plock_{nullptr} {}

    locked(T &value, ::std::mutex &lock) : locked{this->lock(&lock), value} {}
    locked(T &value, ::std::mutex *lock) : locked{this->lock(&lock), value} {}

    locked(locked const &) = delete;
    locked &operator=(locked const &) = delete;

    locked(locked &&other)
        : base{static_cast<base &&>(other)},
          plock_{::std::exchange(other.plock_, nullptr)} {}
    locked &operator=(locked &&other) {
      static_cast<base &>(*this) = static_cast<base &&>(other);
      plock_ = ::std::exchange(other.plock_, nullptr);
    }

    ~locked() {
      if (plock_) {
        plock_->unlock();
      }
    }

    ::std::mutex *detach() noexcept { return ::std::exchange(plock_, nullptr); }

  private:
    ::std::mutex *lock(::std::mutex *ptr) {
      if (ptr) {
        ptr->lock();
      }
      return ptr;
    }

    locked(::std::mutex *lock, T &value) : base{value}, plock_{lock} {}
    locked(::std::mutex *lock, uint32_t index, T *&value)
        : base{value[index]}, plock_{lock} {}

    ::std::mutex *plock_;
  };

  template <typename T> struct locked_range {
    locked_range() = default;
    ~locked_range() {
      for (auto &[_, lock] : v_) {
        if (lock) {
          lock->unlock();
        }
      }
    }

    locked_range(locked_range const &) = delete;
    locked_range &operator=(locked_range const &) = delete;
    locked_range(locked_range &&) noexcept = default;
    locked_range &operator=(locked_range &&) noexcept = default;

    void append(locked<T> &&value) {
      v_.emplace_back(value.value, value.detach());
    }
    void append(T value) { v_.emplace_back(value, nullptr); }
    template <typename U, typename Proj>
    void append(locked<U> &&value, Proj &&proj)
      requires(requires {
        {
          static_cast<Proj &&>(proj)(::std::declval<U>())
        } -> ::std::convertible_to<T>;
      })
    {
      v_.emplace_back(static_cast<Proj &&>(proj)(value.value), value.detach());
    }

    void reserve(size_t size) { v_.reserve(size); }

    constexpr auto operator[](size_t index) { return span()[index]; }
    constexpr auto span() { return v_.template column<0u>(); }

  private:
    vectors<T, ::std::mutex *> v_;
  };
}
