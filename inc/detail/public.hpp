#pragma once

// --- Agents specification -------------------------------------------------
// this scope should not contain vulkan objects.
// Resource-usage tags identify a logical bind-set slot with `index` only.
// Attachment tags use that same `index` as their Vulkan attachment slot; do
// not add a second attachment coordinate.
// Backend binding coordinates belong to focused tags in
// `resource_usage_extensions`; never add set, buffer, or heap coordinates to
// a resource-usage tag.
// Resource-usage and binding-coordinate tags normally use `express` and stay
// out of the inheritance chain. Use an `m<Tag, N>` component only when a tag
// must retain independent state or behavior.
// Command-pool policy is selected per command unit. `reset_pool` is the
// default; transient and individual-reset are explicit policy tags
// and must never be confused with command-buffer usage flags.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl {
  using size_t = ::std::size_t;
  using uint64_t = ::std::uint64_t;
  using uint32_t = ::std::uint32_t;
  using uint16_t = ::std::uint16_t;
  using uint8_t = ::std::uint8_t;
  using frame_scope_id = uint32_t;
}

VKTL_EXPORT_ namespace vktl::detail {
  inline constexpr struct invalid_ {
    template <::std::unsigned_integral T> static constexpr T value = T(-1);

    template <::std::unsigned_integral T>
    constexpr operator T() const noexcept {
      return value<T>;
    }

    // c++ dont allow convert to T and then use bultin operator==.
    // thus manually use it.

    template <::std::unsigned_integral T>
    friend constexpr bool operator==(T left, const invalid_) noexcept {
      return left == value<T>;
    }
    template <::std::unsigned_integral T>
    friend constexpr bool operator!=(T left, const invalid_) noexcept {
      return left != value<T>;
    }
    template <::std::unsigned_integral T>
    friend constexpr bool operator==(const invalid_, T left) noexcept {
      return left == value<T>;
    }
    template <::std::unsigned_integral T>
    friend constexpr bool operator!=(const invalid_, T left) noexcept {
      return left != value<T>;
    }
  } invalid{}, maximum{};

  // template<template<typename>typename...Ts>
  // struct compose {
  // 	template<typename C>
  // 	using apply = C;
  // };
  // template<template<typename>typename First>
  // struct compose<First> {
  // 	template<typename C>
  // 	using apply = First<C>;
  // };
  // template<template<typename>typename First, template<typename>typename
  // Second> struct compose<First, Second> { 	template<typename C> 	using apply =
  // First<Second<C>>;
  // };
  // template<template<typename>typename First, template<typename>typename
  // Second, template<typename>typename...Ts> struct compose<First, Second,
  // Ts...> : compose<compose<First, Second>::template apply, Ts...> {};

  template <typename... Ts> struct compose {
    template <typename C> using apply = C;
  };
  template <typename F, typename... Ts> struct compose<F, Ts...> {
    template <typename C>
    using apply =
        typename compose<Ts...>::template apply<typename F::template apply<C>>;
  };

  template <typename C, typename... Ts>
  using apply_compose = typename compose<Ts...>::template apply<C>;

  struct vptr_base {
  protected:
    template <typename> static constexpr void rebind() noexcept {}
    static constexpr void rebind(auto const &) noexcept {}
    constexpr auto get_this() const noexcept { return pthis; }

  protected:
    void *pthis = nullptr;
  };

  template <typename T> struct virtual_fn {};
  template <typename Rt, typename... Args>
  struct virtual_fn<Rt(Args...)>
      : ::std::type_identity<Rt (*)(void *, Args...)> {};
  template <typename Rt, typename... Args>
  struct virtual_fn<Rt(Args...) const>
      : ::std::type_identity<Rt (*)(void const *, Args...)> {};
  template <typename Rt, typename... Args>
  struct virtual_fn<Rt(Args...) noexcept>
      : ::std::type_identity<Rt (*)(void *, Args...) noexcept> {};
  template <typename Rt, typename... Args>
  struct virtual_fn<Rt(Args...) const noexcept>
      : ::std::type_identity<Rt (*)(void const *, Args...) noexcept> {};

  template <typename T> using vfn = typename virtual_fn<T>::type;

  template <typename T> struct share;
  template <typename> struct unique;

  // type at left usally at top of inheritance chain.
  template <typename... VPtrs> struct box : apply_compose<vptr_base, VPtrs...> {
    template <typename...> friend struct box;

    using base = apply_compose<vptr_base, VPtrs...>;
    constexpr box() noexcept = default;

    template <typename T> constexpr box(T *pthis) {
      this->pthis = pthis;
      rebind<T>();
    }

    template <typename T> constexpr box(T &pthis) {
      this->pthis = &pthis;
      rebind<T>();
    }

    template <typename T> constexpr box(share<T> &self) {
      this->pthis = self.get();
      rebind<T>();
    }

    template <typename T> constexpr box(unique<T> &self) {
      this->pthis = self.get();
      rebind<T>();
    }

    constexpr box(box const &other)
        : base{static_cast<base const &>(other)}, add_ref_{other.add_ref_},
          release_{other.release_} {
      VKTL_ASSERT(!other.unique()); // unique box cannot copy.
      this->pthis = other.pthis;
      if (add_ref_ && this->pthis) {
        add_ref_(this->pthis);
      }
    }
    constexpr box &operator=(box const &other) {
      if (&other != this) {
        VKTL_ASSERT(!other.unique()); // unique box cannot copy assign.

        reset();

        add_ref_ = other.add_ref_;
        release_ = other.release_;

        static_cast<base &>(*this) = other;
        if (this->pthis && add_ref_) {
          add_ref_(this->pthis);
        }
      }
      return *this;
    }

    template <typename... OtherVPtrs>
    constexpr box(box<OtherVPtrs...> const &other) {
      VKTL_ASSERT(!other.unique()); // unique box cannot copy.
      copy_vptrs<base>(
          static_cast<typename box<OtherVPtrs...>::base const &>(other));
      this->pthis = other.pthis;
      add_ref_ = other.add_ref_;
      release_ = other.release_;
      if (this->pthis && add_ref_) {
        add_ref_(this->pthis);
      }
    }

    template <typename... OtherVPtrs>
    constexpr box &operator=(box<OtherVPtrs...> const &other) {
      VKTL_ASSERT(!other.unique()); // unique box cannot copy.

      reset();
      copy_vptrs<base>(
          static_cast<typename box<OtherVPtrs...>::base const &>(other));
      this->pthis = other.pthis;
      add_ref_ = other.add_ref_;
      release_ = other.release_;
      if (this->pthis && add_ref_) {
        add_ref_(this->pthis);
      }
      return *this;
    }

    constexpr box(box &&other) noexcept
        : base{static_cast<base &&>(other)},
          add_ref_{::std::exchange(other.add_ref_, nullptr)},
          release_{::std::exchange(other.release_, nullptr)} {
      this->pthis = ::std::exchange(other.pthis, nullptr);
    }

    constexpr box &operator=(box &&other) noexcept {
      if (this != &other) {
        reset();
        static_cast<base &>(*this) = static_cast<base &&>(other);
        this->pthis = ::std::exchange(other.pthis, nullptr);
        add_ref_ = ::std::exchange(other.add_ref_, nullptr);
        release_ = ::std::exchange(other.release_, nullptr);
      }
      return *this;
    }

    template <typename... OtherVPtrs>
    constexpr box(box<OtherVPtrs...> &&other) noexcept {
      copy_vptrs<base>(
          static_cast<typename box<OtherVPtrs...>::base const &>(other));
      this->pthis = ::std::exchange(other.pthis, nullptr);
      add_ref_ = ::std::exchange(other.add_ref_, nullptr);
      release_ = ::std::exchange(other.release_, nullptr);
    }

    template <typename... OtherVPtrs>
    constexpr box &operator=(box<OtherVPtrs...> &&other) noexcept {
      reset();
      copy_vptrs<base>(
          static_cast<typename box<OtherVPtrs...>::base const &>(other));
      this->pthis = ::std::exchange(other.pthis, nullptr);
      add_ref_ = ::std::exchange(other.add_ref_, nullptr);
      release_ = ::std::exchange(other.release_, nullptr);
      return *this;
    }

    ~box() { reset(); }

    constexpr bool is_shared() const noexcept { return add_ref_; }
    constexpr bool is_unique() const noexcept { return !add_ref_ && release_; }
    constexpr bool is_view() const noexcept { return !add_ref_ && !release_; }

    constexpr bool empty() const noexcept { return !this->pthis; }

    constexpr void reset() noexcept {
      auto *pthis = ::std::exchange(this->pthis, nullptr);
      auto release = ::std::exchange(release_, nullptr);
      add_ref_ = nullptr;
      if (release && pthis) {
        release(pthis);
      }
    }

    constexpr void *get() const noexcept { return this->pthis; }

    constexpr bool operator==(box const &other) const noexcept {
      return other.pthis == this->pthis;
    }

  private:
    template <typename T> constexpr void rebind() {
      rebind<T, base>();
      constexpr bool share = requires { typename T::shared_tag; };
      constexpr bool unique = requires { typename T::unique_tag; };
      static_assert(!(share && unique),
                    "a box target cannot be both shared and unique"); // test.

      if constexpr (share) {
        static_assert(
            requires(T &v) {
              { v.add_ref() } -> ::std::same_as<uint32_t>;
              { v.release() } -> ::std::same_as<uint32_t>;
            }, "shared_tag requires uint32_t add_ref() and release()");
        add_ref_ = [](void *ptr) -> uint32_t {
          return static_cast<T *>(ptr)->add_ref();
        };
        release_ = [](void *ptr) -> uint32_t {
          return static_cast<T *>(ptr)->release();
        };
      } else if constexpr (unique) {
        release_ = [](void *ptr) -> uint32_t {
          delete static_cast<T *>(ptr);
          return 0u;
        };
      }
    }

  private:
    template <typename N, typename O>
    static constexpr void copy_vptr(N &target, O const &source) noexcept {
      if constexpr (!::std::same_as<O, vptr_base>) {
        if constexpr (requires { target.vptr = source.vptr; }) {
          target.vptr = source.vptr;
        } else {
          copy_vptr(target, static_cast<typename O::base const &>(source));
        }
      }
    }

    template <typename N, typename O>
    constexpr void copy_vptrs(O const &source) noexcept {
      if constexpr (!::std::same_as<N, vptr_base>) {
        auto &target = static_cast<N &>(*this);
        copy_vptr(target, source);
        copy_vptrs<typename N::base>(source);
      }
    }

    template <typename T, typename N> constexpr void rebind() noexcept {
      if constexpr (!::std::same_as<N, vptr_base>) {
        N::vptr.template rebind<T>();
        rebind<T, typename N::base>();
      }
    }

  private:
    using base::vptr;
    vfn<uint32_t()> add_ref_ = nullptr;
    vfn<uint32_t()> release_ = nullptr;
  };

  struct byte_view : ::std::span<const ::std::byte> {
    using base = span<const ::std::byte>;

    using base = ::std::span<const ::std::byte>;

    using base::base;

    constexpr byte_view() noexcept = default;
    constexpr byte_view(void const *handle, size_t size) noexcept
        : base{static_cast<::std::byte const *>(handle), size} {}

    template <typename T>
    constexpr byte_view(T const *handle, size_t count) noexcept
        : base{reinterpret_cast<::std::byte const *>(handle),
               count * sizeof(T)} {}

    constexpr byte_view(void const *begin, void const *end) noexcept
        : base{static_cast<::std::byte const *>(begin),
               static_cast<::std::byte const *>(end)} {}

    template <typename T>
    constexpr byte_view(T const *begin, T const *end) noexcept
        : base{reinterpret_cast<::std::byte const *>(begin),
               static_cast<size_t>(end - begin) * sizeof(T)} {}

    // template <typename T, size_t N>
    // constexpr byte_view(T const (&arr)[N]) noexcept
    // 	: base{ reinterpret_cast<::std::byte const*>(arr), N * sizeof(T) } {
    // }

    template <typename T>
      requires(!::std::is_pointer_v<T> && ::std::is_trivially_copyable_v<T>)
    explicit constexpr byte_view(T const &obj) noexcept
        : base{reinterpret_cast<::std::byte const *>(::std::addressof(obj)),
               sizeof(T)} {}

    template <typename T>
      requires(::std::is_rvalue_reference_v<T &&>)
    byte_view(T &&) = delete;

    // template <typename R>
    // 	requires(::std::ranges::contiguous_range<R>)
    // 	&& ::std::ranges::sized_range<R>
    // 	&& (!::std::same_as<::std::remove_cvref_t<R>, byte_view>)
    // 	&& (!::std::same_as<::std::remove_cvref_t<R>, base>)
    // constexpr byte_view(R&& range) noexcept
    // 	: base{ reinterpret_cast<::std::byte
    // const*>(::std::ranges::data(range)),
    // 			::std::ranges::size(range) *
    // sizeof(std::ranges::range_value_t<R>) } {
    // }
  };
}

VKTL_EXPORT_ namespace vktl::vptr {
  using detail::apply_compose;
  using detail::compose;
  using detail::vfn;

  struct handle_allocator {
    template <typename C> struct apply;

    template <typename T> constexpr void rebind() noexcept {
      allocate_ = [](void *ptr, size_t size, size_t alignment) -> void * {
        auto *self = static_cast<T *>(ptr);
        if constexpr (requires {
                        {
                          self->allocate(size, std::align_val_t{alignment})
                        } -> ::std::convertible_to<void *>;
                      }) {
          return self->allocate(size, std::align_val_t{alignment});
        } else {
          return self->allocate(size, alignment);
        }
      };

      if constexpr (
          requires(T &value, void *ptr, size_t size, size_t alignment) {
            {
              value.reallocate(ptr, size, alignment)
            } -> ::std::convertible_to<void *>;
          } ||
          requires(T &value, void *ptr, size_t size,
                   std::align_val_t alignment) {
            {
              value.reallocate(ptr, size, alignment)
            } -> ::std::convertible_to<void *>;
          }) {
        reallocate_ = [](void *ptr, void *re, size_t size,
                         size_t alignment) -> void * {
          auto *self = static_cast<T *>(ptr);
          if constexpr (requires {
                          {
                            self->reallocate(re, size,
                                             std::align_val_t{alignment})
                          } -> ::std::convertible_to<void *>;
                        }) {
            return self->reallocate(re, size, std::align_val_t{alignment});
          } else {
            return self->reallocate(re, size, alignment);
          }
        };
      } else {
        reallocate_ = nullptr;
      }

      free_ = [](void *ptr, void *p) { static_cast<T *>(ptr)->free(p); };
    }

    vfn<void *(size_t, size_t)> allocate_ = nullptr;
    vfn<void *(void *, size_t, size_t)> reallocate_ = nullptr;
    vfn<void(void *)> free_ = nullptr;
  };
  template <typename C> struct handle_allocator::apply : C {
    using base = C;

    void *allocate(size_t size, size_t alignment) {
      VKTL_ASSERT(vptr.allocate_);
      return vptr.allocate_(C::get_this(), size, alignment);
    }
    bool reallocable() const noexcept { return vptr.reallocate_; }
    void *reallocate(void *ptr, size_t size,
                     size_t alignment = alignof(max_align_t)) {
      VKTL_ASSERT(vptr.reallocate_);
      return vptr.reallocate_(C::get_this(), ptr, size, alignment);
    }
    void free(void *ptr) noexcept {
      VKTL_ASSERT(vptr.free_);
      if (ptr) {
        vptr.free_(C::get_this(), ptr);
      }
    }

    template <typename T> constexpr void rebind() noexcept {
      vptr.template rebind<T>();
    }

    handle_allocator vptr;
  };

  struct debug_callback {
    template <typename C> struct apply;

    template <typename T> constexpr void rebind() noexcept {
      messager_ = [](void *ptr, const char *message) {
        static_cast<T *>(ptr)->send(message);
      };
    }

    vfn<void(const char *)> messager_ = nullptr;
  };
  template <typename C> struct debug_callback::apply : C {
    using base = C;

    template <typename T> constexpr void rebind() noexcept {
      vptr.template rebind<T>();
    }

    void send(const char *message) { vptr.messager_(C::get_this(), message); }

    debug_callback vptr;
  };
}

VKTL_EXPORT_ namespace vktl::detail {
  struct shader_handle_tag
#if defined(VKTL_NO_COMPILER)
  {
    byte_view compiled_code;
  }
#endif
  ;
  struct memory_handle_tag;
  struct descriptor_handle_tag;
}

VKTL_EXPORT_ namespace vktl {
  template <typename T> using span = ::std::span<T>;
  template <typename T> using cspan = span<::std::add_const_t<T>>;

  using detail::byte_view;
  using detail::invalid;
  using detail::maximum;

  struct error {
    int code;            // error code.
    const char *message; // ansi zero end string.
  };

  namespace extensions {
  // use for internal api's handle allocation.
  // not for gpu memory allocation.
  struct allocate_from {
    detail::box<vptr::handle_allocator> allocator;
  };

  // some of the object must contain at least one of these object.
  // this describe task's type.

  // debug named will also enable in release build.
  // use it is convenient for gpu debug.
  struct debug_named {
    ::std::string_view name;
  };

  // WARNING: currently not supported.
  struct named {
    ::std::string_view name;
  };

  // repeat cannot not use at every object.
  struct repeat {
    size_t count;
  };
  } // namespace extensions

  struct instance {
    const char *name{"app-name-here:3"}; // app name.
    uint32_t version{1};                 // app version.
  };
  namespace instance_extensions {
  struct debug_utils {
    detail::box<vptr::debug_callback> callback;
  };
  struct version1_1 {};
  struct version1_2 {};
  struct version1_3 {};
  struct version1_4 {};
  inline constexpr struct validation_layer_ {
  } validation_layer;
  } // namespace instance_extensions

  namespace windows_extensions {
  using extensions::debug_named;
  }
  struct window {
    // library will not modify the handle.
    void *handle;
  };

  namespace queue_duty {
  using type = uint16_t;
  inline constexpr type none = 0x0;
  inline constexpr type compute = 0x1 << 0;
  inline constexpr type transfer = 0x1 << 1;
  inline constexpr type graphics = 0x1 << 2;
  inline constexpr type present = 0x1 << 3;
  inline constexpr type bind_sparse = 0x1 << 4;

  // inline constexpr type generic = queue_duty::graphics | queue_duty::compute
  // | queue_duty::transfer; inline constexpr type compute = queue_duty::compute
  // | queue_duty::transfer; inline constexpr type compute =
  // queue_duty::graphics | queue_duty::compute | queue_duty::transfer;
  } // namespace queue_duty

  namespace device_extensions {
  using extensions::debug_named;
  }
  struct device {
    uint16_t index; // index of device.
  };

#if !defined(VKTL_NO_COMPILER)
  namespace compiler_extensions {
    inline constexpr struct glsl_ {
    } glsl{};
    inline constexpr struct hlsl_ {
    } hlsl{};
    struct optimize {
      uint8_t level;
      bool reserve_unused_bindings = false;
      bool reserve_unused_spec_constants = false;
      bool validate = true;
    };
    inline constexpr struct contain_debug_info_ {
    } contain_debug_info{};
    inline constexpr struct customize_include_ {
    } customize_include{};
  } // namespace compiler_extensions
  using shader_handle = detail::shader_handle_tag*;
  // complier need instance as parent.
  // should select atleast glsl or hlsl in inherit chain.
  inline constexpr struct compiler_ {
  } compiler;
#endif

  // BEGIN EXECUTION.

  namespace execution_extensions {
  using extensions::allocate_from;
  using extensions::debug_named;

  inline constexpr struct sync2_ {
  } sync2{};
  } // namespace execution_extensions
  // execution is thread related.
  // each queue will represent a thread.
  struct execution {
    // required, must specified thread count.
    uint16_t thread_count;
  };

  namespace semaphore_extensions {
  using extensions::debug_named;
  using extensions::repeat;
  } // namespace semaphore_extensions
  struct semaphore {};

  namespace event_extensions {
  using extensions::debug_named;
  using extensions::repeat;
  } // namespace event_extensions
  struct event {};

  namespace fence_extensions {
    using extensions::debug_named;
    using extensions::repeat;
  } // namespace fence_extensions
  struct fence {};

  namespace queue_extensions {
    using extensions::debug_named;
    // local priority in family.
    struct priority {
      float value;
    };
  } // namespace queue_extensions
  inline constexpr struct queue_ {
    uint32_t family = 0u;
  } queue{}; // since family 0 is always most generic.

  template <typename Fn> struct task {
    Fn func;
  };

  namespace split_extensions {
    using extensions::debug_named;
  }
  struct split {};

  // BEGIN RESOURCE

  namespace image_bits_type {
    using type = uint16_t;
    inline constexpr type undefined = 0x0u;
    
    inline constexpr type unorm = 0x1u;
    inline constexpr type uint = unorm + 0x1;
    
    inline constexpr type snorm = 0x100u;
    inline constexpr type sint = snorm + 0x1;
    inline constexpr type sfloat = snorm + 0x3;
  } // namespace image_bits_type

  namespace image_bits_order {
    using type = uint16_t;
    
    inline constexpr type rgba = 0x1;
    inline constexpr type rbga = 0x2;
    inline constexpr type bgra = 0x3;
    inline constexpr type abgr = 0x4;
    
    inline constexpr type depth_stencil = 0x0u;
  } // namespace image_bits_order

  namespace image_format {
    struct color {
      uint16_t order = image_bits_order::rgba;
    
      // atleast one of the value.
    
      uint16_t rbits = invalid;
      uint16_t gbits = invalid;
      uint16_t bbits = invalid;
      uint16_t abits = invalid;
    
      uint16_t rtype = image_bits_type::undefined;
      uint16_t gtype = image_bits_type::undefined;
      uint16_t btype = image_bits_type::undefined;
      uint16_t atype = image_bits_type::undefined;
    };
    
    struct depth {
      uint16_t order = image_bits_order::depth_stencil;
    
      uint16_t dbits = invalid;
      uint16_t sbits = invalid;
      uint16_t xbits = invalid;
    
      uint16_t dtype = image_bits_type::undefined;
      uint16_t stype = image_bits_type::undefined;
      uint16_t xtype = image_bits_type::undefined;
    };
  } // namespace image_format

  namespace resource_attrs {
    using type = uint64_t;
    
    // indicate image reosurce's dependency is frame global.
    inline constexpr auto global_frame = type(0x1u) << 33;
    // indicate the dependency is device group local.
    inline constexpr auto device_local = type(0x1u) << 34;
    // indicate reosurce allow alias.
    inline constexpr auto allow_alias = type(0x1u) << 35;
    inline constexpr auto sparse = type(0x1u) << 36;
  } // namespace resource_attrs

  namespace resource_extensions {
    using extensions::named;
    using extensions::repeat;
    // using extensions::use_existing;
    
    // not implmented yet.
    // inline constexpr struct allow_sparse_ {} allow_sparse {};
    // only image can use this value.
    // inline constexpr struct allow_alias_ {} allow_alias {};
    // inline constexpr struct lazy_allocated_ {} lazy_allocated {};
    
    struct mappable {
      bool cache = false;
      bool coherent = false;
    };
  } // namespace resource_extensions

  namespace buffer_extensions {
    using namespace resource_extensions;
  }
  // create buffer with specified size.
  struct buffer {
    size_t size;
  };

  namespace image_extensions {
    using namespace resource_extensions;
    using namespace image_format;

    // by default, the library will set to 1.
    struct image_miplevel {
      uint16_t count = 1u;
    };
    // by default, the library will set to 1.
    struct image_array_layers {
      uint16_t count = 1u;
    };
  }
  // it must atleast combines `color` or `depth` and other customized format
  // descriptor.
  struct image {
    uint32_t width;            // required.
    uint32_t height = maximum; // invalid means the image is image 1D, and depth
                               // will be ignored.
    uint16_t depth = maximum;  // invalid means the image is image 2D.
  };

  namespace sampler_id {
    using type = uint32_t;
    inline constexpr type 
      linear_clamp = 0u,            // UI, post-processing, non-tiling textures
      linear_repeat = 1u,           // Basic 2D material textures (tiling)
      nearest_clamp = 2u,           // Pixel art, G-Buffer, depth map point sampling
      nearest_repeat = 3u,          // Pixel art tiling textures
      trilinear_repeat = 4u,        // High-quality tiling textures with Mipmaps
      anisotropic_16x_repeat = 5u,  // Anisotropic filtering (oblique ground/walls)
      shadow_pcf = 6u,              // Shadow map depth comparison sampling (PCF)
      linear_border_black = 7u;     // Black border padding (screen-space effects / decals)
  } // namespace sampler_id
  
  struct sampler {
    sampler_id::type id; // value in namespace sampler type.
  };

  namespace swapchain_extensions {
    using extensions::named;
    inline constexpr struct adopt_image_ {} adopt_image {};
  } // namespace swapchain_extensions
  struct swapchain {
    uint16_t min_frame_count;
    // if width is maximum, then get size from window.
    uint32_t width = maximum;
    // if height is maximum, then get size from window.
    uint32_t height = maximum;
  };

  // allocator_extensions is for memory allocator and descriptor allocator.
  namespace allocator_extensions {
    using extensions::allocate_from;
    using extensions::debug_named;
    } // namespace allocator_extensions
    namespace memory_allocator_extensions {
    using namespace allocator_extensions;
    
    // `budget` should place directly below of `memory_allocator`
    inline constexpr struct budget_ {
    } budget{};
    // `dedicated` should place upper of any other algorithm. (and below of
    // `budget`).
    inline constexpr struct dedicated_ {
    } dedicated{};
    
    // allocation strategy.
    
    inline constexpr struct arena_ {
    } arena{};
    inline constexpr struct buddy_ {
    } buddy{};
    inline constexpr struct best_fit_ {
    } best_fit{};
    
    // allow_xxx should place after any allocate algorithm.
    
    inline constexpr struct allow_buffer_ {
    } allow_buffer{};
    inline constexpr struct allow_image_ {
    } allow_image{};
    } // namespace memory_allocator_extensions
    // memory allocator should at least have:
    // 1. Any single `allow_xxx` field (e.g., `allow_buffer`, `allow_image`) or
    // any combination thereof.
    // 2. One allocate algorithm. (e.g., `buddy`, `linear` or `best_fit`).
    // to allow memory allocation at next.
    inline constexpr struct memory_allocator_ {
  } memory_allocator{};
  using memory_handle = detail::memory_handle_tag*;

  namespace descriptor_allocator_extensions {
    using namespace allocator_extensions;
    
    // Not finished yet.
    // inline constexpr struct from_heap_ {} from_heap {};

    inline constexpr struct from_set_ {
    } from_set{};
  } // namespace descriptor_allocator_extensions
  // at least have `descriptor_allocator_extensions::from_set` or other.
  inline constexpr struct descriptor_allocator_ {
  } descriptor_allocator{};
  using descriptor_handle = detail::descriptor_handle_tag *;

  // bindings.

  namespace resource_view_extensions {
    using extensions::repeat;
  } // namespace resource_view_extensions

  namespace buffer_view_extensions {
    using namespace resource_view_extensions;
    using extensions::debug_named;
    // for texel buffer.
    using image_format::color; 
  } // namespace buffer_view_extensions
  // if no buffer range, then view the whole buffer as this view.
  struct buffer_view {
    size_t offset = 0u;
    size_t size = maximum;
  };

  namespace image_view_extensions {
    using namespace resource_view_extensions;
    using extensions::allocate_from;
    using extensions::debug_named;
    using image_format::color;
    using image_format::depth;
  }
  struct image_view {
    uint32_t mip_offset = 0u;
    uint32_t mip_range = maximum;
    uint32_t array_offset = 0u;
    uint32_t array_range = maximum;
  };

  struct uniform_buffer {
    uint16_t index;
  };
  struct storage_buffer {
    uint16_t index;
  };
  struct texel_buffer {
    uint16_t index;
  };
  struct storage_image {
    uint16_t index;
  };
  struct texel_image {
    uint16_t index;
  };
  struct sampled_image {
    uint16_t index;
    uint16_t sampler_index = invalid;
  };
  struct standalone_sampler {
    uint16_t index;
  };

  namespace attachment_attribute {
    using type = uint16_t;
    // since integral prompt make it int, we need `type()` here.
    inline constexpr auto load = type(1u << 0u);
    inline constexpr auto load_stencil = type(1u << 1u);
    inline constexpr auto clear = type(1u << 2u);
    inline constexpr auto clear_stencil = type(1u << 3u);
    inline constexpr auto store = type(1u << 4u);
    inline constexpr auto store_stencil = type(1u << 5u);
    // inline constexpr auto may_alias = type(1u << 6u);
  } // namespace attachment_attribute
  struct input_attachment {
    uint16_t index = invalid; // also the attachment slot; invalid appends.
    attachment_attribute::type attribute = attachment_attribute::load;
  };
  struct depth_stencil_attachment {
    uint16_t index = invalid; // also the attachment slot; invalid appends.
    attachment_attribute::type attribute =
        attachment_attribute::clear | attachment_attribute::store;
  };
  struct color_attachment {
    uint16_t index = invalid; // also the attachment slot; invalid appends.
    attachment_attribute::type attribute =
        attachment_attribute::clear | attachment_attribute::store;
  };
  struct resolve_attachment {
    uint16_t index = invalid; // also the attachment slot; invalid appends.
    attachment_attribute::type attribute =
        attachment_attribute::load | attachment_attribute::store;
  };

  namespace bind_set_extensions {
  using extensions::allocate_from;
  using extensions::debug_named;
  inline constexpr struct allow_buffer_ {
  } allow_buffer{};
  inline constexpr struct allow_image_ {
  } allow_image{};
  inline constexpr struct allow_sampler_ {
  } allow_sampler{};
  inline constexpr struct allow_mutable_ {
  } allow_mutable{};
  } // namespace bind_set_extensions
  inline constexpr struct bind_set_ {
  } bind_set{};

  namespace framebuffer_extensions {
  using extensions::allocate_from;
  }
  inline constexpr struct framebuffer_ {
  } framebuffer{};

  // BEGIN PASS.

  namespace stage_extensions {
  struct stage_is_dynamic {};
  } // namespace stage_extensions

  namespace resource_usage_extensions {
    // bind on descriptor set.
    struct bind_on_set {
      uint32_t set;
      uint32_t binding;
    };
    // bind on descriptor heap.
    struct bind_on_heap {
      uint64_t offset;
    };
    struct with_count {
      uint32_t count;
    };
  } // namespace resource_usage_extensions
  namespace pipe_extensions {
  using extensions::debug_named;

  struct pipe_bytes {
    byte_view bytes;
  };

  struct inheritance {
    // pipe index inside pass.
    uint32_t index;
  };
  } // namespace pipe_extensions
  // if graphics pipeline and need render pass, it default is subpass 0,
  // else use subpass extension.
  inline constexpr struct pipe_ {
  } pipe;

  namespace push_constants_extensions {}
  // push constants usually small.
  struct push_constants {
    uint16_t size;
    uint16_t offset = invalid;
  };

  namespace vertex_input_extensions {
    using namespace stage_extensions;
    
    struct vertex_binding {
      // if invalid, then automatically as the final one.
      ::std::uint16_t binding = invalid;
      // set true to change input rate as per instance change.
      bool each_instance = false;
    };
  } // namespace vertex_input_extensions
  inline constexpr struct vertex_input_ {
  } vertex_input{};

  namespace input_assembly_extensions {
  using namespace stage_extensions;
  }

  namespace input_assembly_attributes {
    inline constexpr uint16_t point = 0x1u;
    inline constexpr uint16_t line = 0x1u << 1u;
    inline constexpr uint16_t triangle = 0x1u << 2u;
    inline constexpr uint16_t patch = 0x1u << 3u;
    inline constexpr uint16_t strip = 0x1u << 4u;
    inline constexpr uint16_t fan = 0x1u << 5u;
    inline constexpr uint16_t adjacent = 0x1u << 6u;
  } // namespace input_assembly_attributes
  struct input_assembly {
    uint16_t attributes = 0x14u;
  };

  namespace shader_extensions {
    struct customized_entry_point {
      char const *name; // must cstr.
    };
  } // namespace shader_extensions

  namespace vertex_shader_extensions {
    using namespace shader_extensions;
  }
  struct vertex_shader {
    shader_handle handle;
  };

  namespace fragment_shader_extensions {
    using namespace shader_extensions;
  }
  struct fragment_shader {
    shader_handle handle;
  };

  namespace blend_state_extensions {}
  struct blend_state {
    uint32_t index;
  };

  namespace compute_shader_extensions {
  using namespace shader_extensions;
  }
  struct compute_shader {
    shader_handle code;
  };

  namespace command_extensions {}

  namespace pass_extensions {
  using extensions::allocate_from;
  using extensions::debug_named;
  using image_format::color;
  using image_format::depth;
  inline constexpr struct compute_ {
  } compute{};
  inline constexpr struct rendering_ {
  } rendering{};
  inline constexpr struct render_pass_ {
  } render_pass{};
  inline constexpr struct split_subpass_ {
  } split_subpass{};
  } // namespace pass_extensions
  inline constexpr struct pass_ {
  } pass;
}
