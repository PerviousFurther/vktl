#pragma once

// Interface style: small value wrappers, handle policies, ranges, and access
// helpers provide reusable building blocks for the composable object layers.
// Implementation: ownership behavior is encoded in wrapper copy/move rules;
// overloaded handle address operators are reserved for Vulkan out parameters.
// Agents specification: move_only transfers its value and clears the source;
// assigning a live destination is rejected to avoid silently leaking a handle.

VKTL_EXPORT_ namespace vktl::detail {

  template <typename T> using vector = ::std::vector<T>;
  template <typename T> using list = ::std::list<T>;
  template <typename T> using deque = ::std::deque<T>;

  template <typename Fn> struct defer {
    ~defer() { fn_(); }
    Fn fn_;
  };

  template <typename T, size_t size> using array = ::std::array<T, size>;
  template <typename K> using set = ::std::set<K>;
  template <typename K, typename T> using map = ::std::map<K, T>;
  template <typename K, typename T> using umap = ::std::unordered_map<K, T>;

  template <typename T, typename U>
  concept similiar_to =
      ::std::same_as<::std::remove_cvref_t<T>, ::std::remove_cvref_t<U>>;

  template <typename T> struct tuple_like : ::std::false_type {};
  template <typename... Ts>
  struct tuple_like<::std::tuple<Ts...>> : ::std::true_type {};

  template <typename T>
  constexpr bool tuple_like_v = tuple_like<::std::remove_cvref_t<T>>::value;

  template <typename T> struct tuple_size {};
  template <template <typename...> typename Tp, typename... Ts>
    requires(tuple_like<Tp<Ts...>>::value)
  struct tuple_size<Tp<Ts...>> {
    static constexpr auto value{sizeof...(Ts)};
  };

  template <typename T>
    requires(tuple_like<T>::value)
  constexpr auto tuple_size_v = tuple_size<::std::remove_cvref_t<T>>::value;

  template <size_t index, typename T> struct tuple_at {};
  template <size_t index, template <typename...> typename Tp, typename... Ts>
    requires(tuple_like_v<Tp<Ts...>> && index < sizeof...(Ts))
  struct tuple_at<index, Tp<Ts...>>
      : ::std::tuple_element<index, ::std::tuple<Ts...>> {};

  template <size_t index, typename T>
  using tuple_at_t = typename tuple_at<index, T>::type;

  template <typename T, typename... Args>
  consteval ::std::size_t get_index() noexcept {
    constexpr bool matches[] = {::std::same_as<T, Args>...};
    for (::std::size_t i = 0; i < sizeof...(Args); ++i) {
      if (matches[i]) {
        return i;
      }
    }
    return invalid;
  }

  template <typename T, typename Tuple> struct find_if_same;
  template <typename T, template <typename...> typename Tp, typename... Args>
    requires(tuple_like<Tp<Args...>>::value)
  struct find_if_same<T, Tp<Args...>>
      : ::std::integral_constant<::std::size_t, get_index<T, Args...>()> {};
  template <typename T, typename Tuple>
  constexpr size_t find_if_same_v = find_if_same<T, Tuple>::value;

  template <typename T> constexpr T align_up(T value, T alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
  }
  template <typename T> constexpr T align_down(T value, T alignment) noexcept {
    return value & ~(alignment - 1);
  }
  template <typename T>
  constexpr T align_up_generic(T value, T alignment) noexcept {
    return ((value + alignment - 1) / alignment) * alignment;
  }
  template <typename T>
  constexpr T align_down_generic(T value, T alignment) noexcept {
    return (value / alignment) * alignment;
  }

  template <typename T, typename Spec, typename = void>
  struct is_specialization_of : ::std::false_type {};
  template <typename T, typename C>
  struct is_specialization_of<T, C,
                              ::std::void_t<typename C::template apply<T>>>
      : C::template apply<T> {};

  template <template <typename...> typename Tp> struct tuple_specialization {
    template <typename T> struct apply : ::std::false_type {};
    template <typename... Ts> struct apply<Tp<Ts...>> : ::std::true_type {};
  };

  template <typename T, template <typename...> typename Tp>
  concept tuple_specialization_of =
      is_specialization_of<::std::remove_cvref_t<T>,
                           tuple_specialization<Tp>>::value;

  template <typename... Ts> struct ts {
    template <template <typename...> typename Apply> using apply = Apply<Ts...>;
  };
  template <typename... Ts> struct tuple_like<ts<Ts...>> : ::std::true_type {};

  template <template <typename...> typename Tp> struct quote {
    template <typename... Ts> using apply = Tp<Ts...>;
  };

  template <template <typename...> typename Tp, typename... Ts>
  struct bind_back {
    template <typename... Us> using apply = Tp<Us..., Ts...>;
  };

  template <template <typename...> typename Tp, typename... Ts>
  struct bind_front {
    template <typename... Us> using apply = Tp<Ts..., Us...>;
  };

  template <typename T> constexpr auto always_false = false;

  bool invoke_by_index(size_t index, auto &&fn) { return false; }
  bool invoke_by_index(size_t index, auto &&fn, auto &&first,
                       auto &&...values) {
    if (index == 0u) {
      forward_(fn)(forward_(first));
      return true;
    } else {
      return invoke_by_index(index - 1u, forward_(fn), forward_(values)...);
    }
  }

  template <typename T> struct is_handle : ::std::is_pointer<T> {};
  template <typename T>
  constexpr auto is_handle_v = is_handle<::std::remove_cvref_t<T>>::value;

  template <typename T> struct default_value {
    template <typename... Args>
    constexpr default_value(Args &&...values)
        : value{static_cast<Args &&>(values)...} {}

    constexpr operator T &() noexcept { return value; }
    constexpr operator T const &() const noexcept { return value; }

    constexpr auto operator&() noexcept { return &value; }
    constexpr auto operator&() const noexcept { return &value; }

    friend auto exchange(default_value &self, T value) noexcept {
      return ::std::exchange(self.value, ::std::move(value));
    }
    friend auto exchange(T &value, default_value &self) noexcept {
      return ::std::exchange(value, self.value);
    }
    friend auto exchange(T &value, default_value &&self) noexcept {
      return ::std::exchange(value, ::std::move(self.value));
    }

    friend void swap(default_value &self, T &value) noexcept {
      ::std::swap(self.value, value);
    }
    friend void swap(T &value, default_value &self) noexcept {
      ::std::swap(value, self.value);
    }

    T value;
  };

  template <typename T> struct handle_ : default_value<T> {
    constexpr T *operator->() noexcept { return &this->value; }
    constexpr T const *operator->() const noexcept { return &this->value; }
  };

  template <typename T>
    requires(is_handle_v<T>)
  struct handle_<T> : default_value<T> {
    using default_value<T>::default_value;

    constexpr bool operator<(T other) const noexcept {
      return this->value < other;
    }
    constexpr bool operator>(T other) const noexcept {
      return this->value > other;
    }
    constexpr bool operator<=(T other) const noexcept {
      return this->value <= other;
    }
    constexpr bool operator>=(T other) const noexcept {
      return this->value >= other;
    }
    constexpr bool operator==(T other) const noexcept {
      return this->value == other;
    }
    constexpr bool operator!=(T other) const noexcept {
      return this->value != other;
    }
  };

  template <typename T> struct trait;

  // only move will operate on value, otherwise do reset.
  template <typename T> struct reset_if_copy : handle_<T> {
    using base = handle_<T>;
    constexpr reset_if_copy() = default;
    template <typename... Args>
    constexpr reset_if_copy(Args &&...args)
        : base{static_cast<Args &&>(args)...} {}

    constexpr reset_if_copy(reset_if_copy const &) : base{} {}
    constexpr reset_if_copy &operator=(reset_if_copy const &) {
      VKTL_ASSERT(
          !this->value); // directly discard value maybe cause memory leakage.
      return *this;
    };

    constexpr reset_if_copy(reset_if_copy &&other) noexcept {
      this->value = ::std::exchange(other.value, {});
    }
    constexpr reset_if_copy &operator=(reset_if_copy &&other) noexcept {
      if (::std::addressof(other) != this) {
        VKTL_ASSERT(
            !this->value); // directly discard maybe cause memory leakage.
        this->value = ::std::exchange(other.value, {});
      }
      return *this;
    }
  };

  // template <typename T>
  // struct handle_range_ {
  //	T* value = nullptr;

  //	constexpr handle_range_() noexcept = default;

  //	constexpr handle_range_(uint32_t count) noexcept {
  //		value = new T[count];
  //	}

  //	constexpr handle_range_(uint32_t count, T const& value) noexcept {
  //		for (auto& v : span(this->value = new T[count], count)) {
  //			v = value;
  //		}
  //	}

  //	VKTL_NODISCARD constexpr T& operator[](size_t index) const noexcept {
  //		return value[index];
  //	}

  //	constexpr void reset() noexcept {
  //		value = nullptr;
  //	}

  //	constexpr bool all_null() {

  //	}
  //};

  // template<typename T>
  // struct reset_if_copy_range : handle_range_<T> {
  //	using base = handle_range_<T>;

  //	constexpr reset_if_copy_range() = default;

  //	template<typename... Args>
  //	constexpr reset_if_copy_range(Args&&... args)
  //		: base{ static_cast<Args&&>(args)... }
  //	{
  //	}

  //	constexpr reset_if_copy_range(reset_if_copy_range const&) : base{} {}

  //	constexpr reset_if_copy_range& operator=(reset_if_copy_range const&) {
  //		VKTL_ASSERT(!this->first && !this->second);
  //		return *this;
  //	}

  //	constexpr reset_if_copy_range(reset_if_copy_range&& other) noexcept {
  //		this->first = ::std::exchange(other.first, {});
  //		this->second = ::std::exchange(other.second, {});
  //	}

  //	constexpr reset_if_copy_range& operator=(reset_if_copy_range&& other)
  //noexcept { 		if (&other != this) { 			VKTL_ASSERT(this->empty()); 			this->first =
  //::std::exchange(other.first, {}); 			this->second =
  //::std::exchange(other.second, {});
  //		}
  //		return *this;
  //	}
  //};

  // runtime VKTL_ASSERT on copy when not null, no other usage.
  template <typename T> struct copyable_if_null : reset_if_copy<T> {
    using base = reset_if_copy<T>;

    constexpr copyable_if_null() = default;
    template <typename... Args>
    constexpr copyable_if_null(Args &&...args)
        : base{static_cast<Args &&>(args)...} {}

    constexpr copyable_if_null(copyable_if_null const &) {
      VKTL_ASSERT(!this->value);
    }
    constexpr copyable_if_null &operator=(copyable_if_null const &) {
      VKTL_ASSERT(!this->value);
      return *this;
    }

    constexpr copyable_if_null(copyable_if_null &&other) noexcept = default;
    constexpr copyable_if_null &
    operator=(copyable_if_null &&other) noexcept = default;
  };

  template <typename T> struct move_only : handle_<T> {
    using base = handle_<T>;

    constexpr move_only() = default;
    template <typename... Args>
    constexpr move_only(Args &&...args) : base{static_cast<Args &&>(args)...} {}

    move_only(move_only const &) = delete;
    move_only &operator=(move_only const &) = delete;

    constexpr move_only(move_only &&other) noexcept
        : base{::std::exchange(other.value, {})} {}

    constexpr move_only &operator=(move_only &&other) noexcept {
      if (::std::addressof(other) != this) {
        VKTL_ASSERT(!this->value); // Overwriting a live handle would leak it.
        this->value = ::std::exchange(other.value, {});
      }
      return *this;
    }
  };

  template <typename T> struct range {
    T offset;
    T size;
  };

  template <typename T> struct scope {
    T begin;
    T end;
  };

  struct image_size {
    uint32_t width;
    uint32_t height;
    uint16_t depth;
  };

  struct image_slice {
    range<uint32_t> width;
    range<uint32_t> height;
    range<uint16_t> depth;
  };

  struct image_scope {
    scope<uint32_t> width;
    scope<uint32_t> height;
    scope<uint16_t> depth;
  };

  inline constexpr struct subres_ {
    static constexpr auto remaining_mip = VK_REMAINING_MIP_LEVELS;
    static constexpr auto remaining_arr = VK_REMAINING_ARRAY_LAYERS;

    // for image.
    static constexpr bool
    adjacent_intersect(VK_ VkImageSubresourceRange const &first,
                       VK_ VkImageSubresourceRange const &second) noexcept {
      if (first.aspectMask != second.aspectMask)
        VKTL_UNLIKELY { return false; }
      const bool same_layers =
                     (first.baseArrayLayer == second.baseArrayLayer) &&
                     (first.layerCount == second.layerCount),
                 adjacent_mips =
                     same_layers &&
                     intersected(first.baseArrayLayer, first.layerCount,
                                 second.baseArrayLayer, second.layerCount),
                 same_mips = (first.baseMipLevel == second.baseMipLevel) &&
                             (first.levelCount == second.levelCount),
                 adjacent_layers =
                     same_mips &&
                     intersected(first.baseArrayLayer, first.layerCount,
                                 second.baseArrayLayer, second.layerCount);
      return adjacent_mips || adjacent_layers;
    }

    template <typename T>
    static constexpr bool adjacent_intersect(T first_offset, T first_size,
                                             T second_offset,
                                             T second_size) noexcept {
      const T first_end = first_offset + first_size;
      const T second_end = second_offset + second_size;
      return first_offset <= second_end && second_offset <= first_end;
    }

    template <typename T>
    static constexpr bool adjacent_intersect(range<T> const &first,
                                             range<T> const &second) noexcept {
      return adjacent_intersect(first.offset, first.size, second.offset,
                                second.size);
    }

    static constexpr auto
    merge(VK_ VkImageSubresourceRange const &first,
          VK_ VkImageSubresourceRange const &second) noexcept {
      VK_ VkImageSubresourceRange result{};
      result.aspectMask = first.aspectMask | second.aspectMask;
      result.baseMipLevel =
          (::std::min)(first.baseMipLevel, second.baseMipLevel);
      auto end1 = add(first.levelCount, first.baseMipLevel),
           end2 = add(second.levelCount, second.baseMipLevel),
           max_end = (::std::max)(end1, end2);
      result.levelCount = max_end - result.baseMipLevel;
      result.baseArrayLayer =
          (::std::min)(first.baseArrayLayer, second.baseArrayLayer);
      end1 = first.baseArrayLayer + first.layerCount;
      end2 = second.baseArrayLayer + second.layerCount;
      max_end = (::std::max)(end1, end2);
      result.layerCount = max_end - result.baseArrayLayer;
      return result;
    }

    template <typename T>
    static constexpr auto merge(range<T> first, range<T> second) noexcept {
      T left = (::std::min)(first.offset, second.offset),
        first_end = add(no_zero(first.size), first.offset),
        second_end = add(no_zero(second.size), second.offset),
        right = (::std::max)(first_end, second_end);
      return range<T>{left, sub(right, left)};
    }

    static constexpr bool
    intersected(VK_ VkImageSubresourceRange const &first,
                VK_ VkImageSubresourceRange const &second) noexcept {
      if (!(first.aspectMask & second.aspectMask))
        VKTL_UNLIKELY { return false; }

      const auto first_level_end = add(first.levelCount, first.baseMipLevel),
                 second_level_end = add(second.levelCount, second.baseMipLevel),
                 first_layer_end = add(first.layerCount, first.baseArrayLayer),
                 second_layer_end =
                     add(second.layerCount, second.baseArrayLayer);
      const auto level_intersects = (first.baseMipLevel < second_level_end) &&
                                    (second.baseMipLevel < first_level_end),
                 layer_intersects = (first.baseArrayLayer < second_layer_end) &&
                                    (second.baseArrayLayer < first_layer_end);
      return level_intersects && layer_intersects;
    }

    static constexpr auto
    get_intersect(VK_ VkImageSubresourceRange const &left,
                  VK_ VkImageSubresourceRange const &right) noexcept {

      const auto base_mip = (left.baseMipLevel > right.baseMipLevel)
                                ? left.baseMipLevel
                                : right.baseMipLevel,
                 end_mip1 = (left.levelCount == remaining_mip)
                                ? remaining_mip
                                : left.baseMipLevel + left.levelCount,
                 end_mip2 = (right.levelCount == remaining_mip)
                                ? remaining_mip
                                : right.baseMipLevel + right.levelCount,
                 end_mip = (end_mip1 < end_mip2) ? end_mip1 : end_mip2;

      const auto base_arr = (left.baseArrayLayer > right.baseArrayLayer)
                                ? left.baseArrayLayer
                                : right.baseArrayLayer,
                 end_arr1 = (left.layerCount == remaining_arr)
                                ? remaining_arr
                                : left.baseArrayLayer + left.layerCount,
                 end_arr2 = (right.layerCount == remaining_arr)
                                ? remaining_arr
                                : right.baseArrayLayer + right.layerCount,
                 end_arr = (end_arr1 < end_arr2) ? end_arr1 : end_arr2;

      return VK_ VkImageSubresourceRange{
          .aspectMask = left.aspectMask & right.aspectMask,
          .baseMipLevel = base_mip,
          .levelCount =
              (end_mip == remaining_mip) ? remaining_mip : (end_mip - base_mip),
          .baseArrayLayer = base_arr,
          .layerCount = (end_arr == remaining_arr) ? remaining_arr
                                                   : (end_arr - base_arr)};
    }

    struct diff_img : VK_ VkImageSubresourceRange {
      using base = VK_ VkImageSubresourceRange;
      constexpr diff_img() = default;
      constexpr diff_img(VK_ VkImageAspectFlags mask, uint32_t base_mip,
                         uint32_t level_count, uint32_t base_layer,
                         uint32_t layer_count, bool is_first = true)
          : base{
                .aspectMask = mask,
                .baseMipLevel = base_mip,
                .levelCount = level_count,
                .baseArrayLayer = base_layer,
                .layerCount = layer_count,
            },
            is_first{is_first} {}

      bool is_first;
    };
    template <typename T, size_t size = 4> struct diffs : array<T, size> {
      size_t count;

      constexpr auto end() noexcept { return this->begin() + count; }
      constexpr auto end() const noexcept { return this->begin() + count; }
    };

    static constexpr auto empty(VK_ VkImageSubresourceRange const &value) {
      return !value.layerCount && value.levelCount;
    }

    template <typename T> static constexpr auto empty(range<T> const &value) {
      return !value.size;
    }

    static constexpr auto
    get_not_intersected(VK_ VkImageSubresourceRange const &first,
                        VK_ VkImageSubresourceRange const &second) noexcept {
      diffs<diff_img, 4> result{};
      const auto first_right =
                     add(no_zero(first.levelCount), first.baseMipLevel),
                 second_right =
                     add(no_zero(second.levelCount), second.baseMipLevel),
                 left = (::std::min)(first.baseMipLevel, second.baseMipLevel),
                 right = (::std::max)(first_right, second_right),
                 first_bottom =
                     add(no_zero(first.layerCount), first.baseArrayLayer),
                 second_bottom =
                     add(no_zero(second.layerCount), second.baseArrayLayer),
                 top =
                     (::std::min)(first.baseArrayLayer, second.baseArrayLayer),
                 bottom = (::std::max)(first_bottom, second_bottom),
                 intersect_base_mip =
                     (::std::max)(first.baseMipLevel, second.baseMipLevel),
                 intersect_end_mip = (::std::min)(first_right, second_right),
                 intersect_base_arr =
                     (::std::max)(first.baseArrayLayer, second.baseArrayLayer),
                 intersect_end_arr = (::std::min)(second_bottom, first_bottom);

      if (left < intersect_base_mip) {
        auto is_first = left == first.baseMipLevel;
        auto const &value = is_first ? first : second;
        result[result.count++] = {value.aspectMask,
                                  left,
                                  intersect_base_mip - value.baseMipLevel,
                                  value.baseArrayLayer,
                                  value.layerCount,
                                  is_first};
      }

      if (right > intersect_end_mip) {
        auto is_first = first_right == right;
        auto const &value = is_first ? first : second;
        result[result.count++] = {value.aspectMask,
                                  intersect_end_mip,
                                  sub(right, intersect_end_mip),
                                  value.baseArrayLayer,
                                  value.layerCount,
                                  is_first};
      }

      const auto intersect_mip_count =
          sub(intersect_end_mip, intersect_base_mip);
      if (top < intersect_base_arr) {
        auto is_first = first.baseArrayLayer == top;
        auto const &value = is_first ? first : second;
        result[result.count++] = {value.aspectMask,
                                  intersect_base_mip,
                                  intersect_mip_count,
                                  value.baseArrayLayer,
                                  intersect_base_arr - value.baseArrayLayer,
                                  is_first};
      }
      if (bottom > intersect_end_arr) {
        auto is_first = first_bottom == bottom;
        auto const &value = is_first ? first : second;
        result[result.count++] = {value.aspectMask,
                                  intersect_base_mip,
                                  intersect_mip_count,
                                  intersect_end_arr,
                                  sub(bottom, intersect_end_arr),
                                  is_first};
      }

      return result;
    }

    static constexpr bool
    same(VK_ VkImageSubresourceRange const &left,
         VK_ VkImageSubresourceRange const &right) noexcept {
      return left.aspectMask == right.aspectMask &&
             left.baseMipLevel == right.baseMipLevel &&
             left.levelCount == right.levelCount &&
             left.baseArrayLayer == right.baseArrayLayer &&
             left.layerCount == right.layerCount;
    }

    template <typename T>
    static constexpr bool same(range<T> const &left,
                               range<T> const &right) noexcept {
      return left.offset = right.offset && left.size == right.size;
    }

    template <typename T>
    static constexpr bool intersected(T left_offset, T left_size,
                                      T right_offset, T right_size) noexcept {
      return intersected(range<T>{{left_offset}, {left_size}},
                         range<T>{{right_offset}, {right_size}});
    }

    template <typename T>
    static constexpr bool intersected(range<T> left, range<T> right) noexcept {
      left.size = no_zero(left.size);
      right.size = no_zero(right.size);
      const T left_end = add(left.size, left.offset);
      const T right_end = add(right.size, right.offset);
      return (left.offset < right_end) && (right.offset < left_end);
    }

    template <::std::integral U>
    static constexpr bool adjacent(U first_offset, U first_size,
                                   U second_offset, U second_size) noexcept {
      return (first_offset + first_size == second_offset) ||
             (second_offset + second_size == first_offset);
    }

    template <typename U>
    static constexpr bool adjacent(range<U> first, range<U> second) noexcept {
      return adjacent(first.offset, first.size, second.offset, second.size);
    }

    static constexpr bool
    adjacent(VK_ VkImageSubresourceRange const &first,
             VK_ VkImageSubresourceRange const &second) noexcept {
      if (first.aspectMask != second.aspectMask) {
        return false;
      }
      const bool same_mips = (first.baseMipLevel == second.baseMipLevel) &&
                             (first.levelCount == second.levelCount);
      const bool same_layers =
          (first.baseArrayLayer == second.baseArrayLayer) &&
          (first.layerCount == second.layerCount);
      const bool mips_adjacent =
          adjacent(first.baseMipLevel, first.levelCount, second.baseMipLevel,
                   second.levelCount);
      const bool layers_adjacent =
          adjacent(first.baseArrayLayer, first.layerCount,
                   second.baseArrayLayer, second.layerCount);
      return (mips_adjacent && same_layers) || (layers_adjacent && same_mips);
    }

    // left right only indicate left or right position of parameter, not
    // indicate order.
    template <typename T>
    static constexpr auto get_intersect(range<T> left,
                                        range<T> right) noexcept {
      return get_intersect(left.offset, left.size, right.offset, right.size);
    }

    template <typename T>
    static constexpr auto get_intersect(T left_offset, T left_size,
                                        T right_offset, T right_size) noexcept {
      const T intersect_offset = (::std::max)(left_offset, right_offset),
              left_end = add(no_zero(left_size), left_offset),
              right_end = add(no_zero(right_size), right_offset),
              intersect_end = (::std::min)(left_end, right_end),
              intersect_size = sub(intersect_end, intersect_offset);
      return range<T>{intersect_offset, intersect_size};
    }

    template <typename T> struct diff_buf : range<T> {
      constexpr diff_buf() = default;
      constexpr diff_buf(T offset, T size, bool is_first)
          : range<T>{offset, size}, is_first{is_first} {}

      bool is_first;
    };

    // return check is_first to judge the value is left or right.
    template <typename T>
    static constexpr auto get_not_intersected(T first_offset, T first_size,
                                              T second_offset, T second_size) {
      diffs<diff_buf<T>, 2u> result{};
      const T left = (::std::min)(first_offset, second_offset),
              intersect_offset = (::std::max)(first_offset, second_offset),
              left_end = add(no_zero(first_size), first_offset),
              right_end = add(no_zero(second_size), second_offset),
              right = (::std::max)(left_end, right_end),
              intersect_end = (::std::min)(left_end, right_end);

      if (left < intersect_offset) {
        auto is_first = left == first_offset;
        // auto value = is_first ? left : right;
        result[result.count++] = {left, intersect_offset - left, is_first};
      }

      if (right > intersect_end) {
        auto is_first = right == left_end;
        // auto value = is_first ? left : right;
        result[result.count++] = {intersect_end, sub(right, intersect_end),
                                  is_first};
      }

      return result;
    }

    template <typename T>
    static constexpr auto get_not_intersected(range<T> first,
                                              range<T> second) noexcept {
      return get_not_intersected(first.offset, first.size, second.offset,
                                 second.size);
    }

    template <typename T> static constexpr T no_zero(T value) noexcept {
      if (value == 0u) {
        return maximum;
      } else {
        return value;
      }
    }

    template <typename T, typename F>
    static constexpr T sub(T left, F right) noexcept {
      return no_zero(right) == maximum  ? 0u
             : no_zero(left) == maximum ? maximum
                                        : left - right;
    }
    template <typename T, typename F>
    static constexpr T add(T left, F right) noexcept {
      return no_zero(left) == maximum || no_zero(right) == maximum
                 ? maximum
                 : left + right;
    }
  } subres{};

  struct default_access {
    VK_ VkAccessFlags accesses = 0u;
    VK_ VkDependencyFlags dependencies = 0u;
    VK_ VkPipelineStageFlags stages = 0u;

    constexpr bool operator==(default_access const &other) const noexcept {
      return stages == other.stages && accesses == other.accesses &&
             dependencies == other.dependencies;
    }

    constexpr auto &operator|=(default_access const &other) noexcept {
      if (&other != this) {
        stages |= other.stages;
        accesses |= other.accesses;
        dependencies |= other.dependencies;
      }
      return *this;
    }
  };

  struct default_buffer_access : range<VK_ VkDeviceSize> {
    using range_type = range<VK_ VkDeviceSize>;

    default_access access;

    constexpr bool
    operator==(default_buffer_access const &other) const noexcept {
      return access == other.access;
    }

    constexpr auto &operator|=(default_buffer_access const &other) noexcept {
      if (&other != this) {
        access |= other.access;
      }
      return *this;
    }
  };

  struct default_image_access : VK_ VkImageSubresourceRange {
    using range_type = VK_ VkImageSubresourceRange;

    VK_ VkImageLayout layout;
    default_access access;

    constexpr bool
    operator==(default_image_access const &other) const noexcept {
      return access == other.access;
    }

    constexpr auto &operator|=(default_image_access const &other) noexcept {
      if (&other != this) {
        if (layout == VK_ VK_IMAGE_LAYOUT_UNDEFINED) {
          layout = other.layout;
        } else if (other.layout == VK_ VK_IMAGE_LAYOUT_GENERAL ||
                   layout != other.layout) {
          layout = VK_ VK_IMAGE_LAYOUT_GENERAL;
        }

        access |= other.access;
      }
      return *this;
    }
  };

  template <typename... VPtrs> using box_list = vector<box<VPtrs...>>;

  // only for getter.

  template <typename T> struct trait_vkfn {};
  template <typename R, typename... Args>
    requires(sizeof...(Args) >= 2)
  struct trait_vkfn<R (*)(Args...)> {
    static constexpr auto num_params = sizeof...(Args);
    using params_list = ts<Args...>;
    using return_type = R;
    static constexpr auto nothrow = ::std::is_void_v<R>;
  };

  inline constexpr struct vkget_ {
    template <typename Fn>
    static constexpr auto is_vkget_v =
        (tuple_size_v<typename trait_vkfn<Fn>::params_list>) > 2;

    template <typename Fn>
    using result_of_t = ::std::remove_pointer_t<tuple_at_t<
        trait_vkfn<Fn>::num_params - 1u, typename trait_vkfn<Fn>::params_list>>;
    template <typename Fn>
    using count_of_t = ::std::remove_pointer_t<tuple_at_t<
        trait_vkfn<Fn>::num_params - 2u, typename trait_vkfn<Fn>::params_list>>;

    template <typename VFn, typename Fn, typename... Ts>
    VK_ VkResult operator()(VFn &&fn, Fn call, Ts... val) const noexcept {
      constexpr bool return_void = trait_vkfn<Fn>::nothrow;
      count_of_t<Fn> count;

      VK_ VkResult result = VK_ VK_SUCCESS;
      if constexpr (return_void) {
        call(val..., &count, nullptr);
      } else {
        result = call(val..., &count, nullptr);
      }

      if (result == VK_ VK_SUCCESS) {
        auto &vec = fn(count);
        if constexpr (return_void) {
          call(val..., &count, vec.data());
        } else {
          result = call(val..., &count, vec.data());
        }
      }
      return result;
    }

    template <::std::ranges::contiguous_range Vec, typename Fn, typename... Ts>
    VK_ VkResult operator()(Vec &vec, Fn call, Ts... vals) const noexcept {
      return operator()(
          [&](auto count) -> Vec & {
            vec.resize(count);
            return vec;
          },
          call, ::std::move(vals)...);
    }
    template <::std::ranges::contiguous_range Vec, typename Fn, typename... Ts>
    VK_ VkResult operator()(Vec &vec, ::std::ranges::range_value_t<Vec> def,
                            Fn call, Ts... vals) const noexcept {
      return operator()(
          [&](auto count) -> Vec & {
            vec.resize(count, def);
            return vec;
          },
          call, ::std::move(vals)...);
    }
  } vkget{};

  // return last, which pNext == nullptr.
  struct vkconnect_ {
    static constexpr auto &impl(auto &first, auto &second, auto &...rest) {
      first.pNext = &second;
      return impl(second, rest...);
    }
    static constexpr auto &impl(auto &first) { return first; }

    constexpr auto &operator()(auto &...rest) const noexcept {
      return impl(rest...);
    }
    template <typename... Ts>
    constexpr auto &operator()(::std::tuple<Ts...> &ts) const noexcept {
      return ::std::apply(vkconnect_(), ts);
    }

  } vkconnect{};

  // vector might not return nullptr if empty(), vulkan have some pointer not
  // have them own size.
  template <typename T>
  constexpr T const *data_or_null(vector<T> const &values) noexcept {
    return values.size() ? values.data() : nullptr;
  }
  template <typename T> constexpr T *data_or_null(vector<T> & values) noexcept {
    return values.size() ? values.data() : nullptr;
  }
}
