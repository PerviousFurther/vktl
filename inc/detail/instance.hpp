#pragma once

// Interface style: an instance descriptor composes layers, extensions, and
// diagnostics while exposing loader and physical-device queries to children.
// Implementation: string lists are version-partitioned and Vulkan creation is
// deferred until the complete object chain has relocated its pointers.

#if defined(VK_API_VERSION_1_4)
#define VKTL_MAX_API_VERSION VK_API_VERSION_1_4
#elif defined(VK_API_VERSION_1_3)
#define VKTL_MAX_API_VERSION VK_API_VERSION_1_3
#elif defined(VK_API_VERSION_1_2)
#define VKTL_MAX_API_VERSION VK_API_VERSION_1_2
#elif defined(VK_API_VERSION_1_1)
#define VKTL_MAX_API_VERSION VK_API_VERSION_1_1
#else
#define VKTL_MAX_API_VERSION VK_API_VERSION_1_0
#endif

VKTL_EXPORT_ namespace vktl::detail {

  using namespace instance_extensions;

  namespace api {
  inline constexpr uint16_t max_minor =
      VK_API_VERSION_MINOR(VKTL_MAX_API_VERSION);
  inline constexpr uint16_t max_major =
      VK_API_VERSION_MAJOR(VKTL_MAX_API_VERSION);
  } // namespace api

  template <typename N> struct basic_cstr_ext : N {
    constexpr basic_cstr_ext(auto &&...values) : N{forward_(values)...} {}

  protected:
    bool append_cstr(const char *value) {
      VKTL_ASSERT(value && "Value pointer must not be null");
      constexpr auto make_sv = [](char const *elem) { return::std::string_view{elem}; };
      auto it = ::std::ranges::lower_bound(strings_, value, {}, make_sv);
      if (it == strings_.end() || make_sv(*it) != make_sv(value)) {
        strings_.insert(it, value);
        return true;
      } else {
        return false;
      }
    }

    auto cstr_span() const noexcept {
      return::std::span{strings_};
    }

  protected:
    vector<const char*> strings_;
  };

  template <typename N> struct basic_layers : basic_cstr_ext<N> {
    using base = basic_cstr_ext<N>;
    constexpr basic_layers(auto &&...values) : base{forward_(values)...} {}

    bool append_layers(const char *layer) {
      return this->append_cstr(layer, disabled_minor);
    }

  protected:
    auto layers() const noexcept {
      return this->cstr_span();
    }
  };

  template <typename N> struct basic_extensions : basic_cstr_ext<N> {
    using base = basic_cstr_ext<N>;
    constexpr basic_extensions(auto &&...values) : base{forward_(values)...} {}

    bool append_extensions(const char *extension) {
      return this->append_cstr(extension);
    }

  protected:
    auto extensions() const noexcept {
      return this->cstr_span();
    }
  };

  template <typename N>
  struct m<instance, N> : basic_extensions<basic_layers<N>> {
    using base = basic_extensions<basic_layers<N>>;

    constexpr m(instance const &info, auto &&...others)
        : base{forward_(others)...},
          info{.sType = VK_ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO},
          app{
              .sType = VK_ VK_STRUCTURE_TYPE_APPLICATION_INFO,
              .pApplicationName = info.name,
              .applicationVersion = info.version,
              .pEngineName = "VKTL@MaoQWE",
              .engineVersion = VKTL_VERSION,
          } {}

    ~m() {
      if (instance_) {
        VK_ vkDestroyInstance(instance_, N::allocator());
      }
    }

    void relocate() {
      N::relocate();
      auto layers = base::layers();
      auto extensions = base::extensions();
      info.enabledLayerCount = uint32_t(layers.size());
      info.ppEnabledLayerNames = layers.data();
      info.enabledExtensionCount = uint32_t(extensions.size());
      info.ppEnabledExtensionNames = extensions.data();
      info.pApplicationInfo = &app;
    }

    void init() {
      N::init();
      if (!instance_) {
        VK_ vkCreateInstance(&info, N::allocator(), &instance_) |
            popup{"[INSTANCE] create instance failure."};
      }
    }

    uint32_t max_api_version() noexcept {
      uint32_t version;
      VK_ vkEnumerateInstanceVersion(&version) |
          popup{"[INSTANCE] Vulkan driver or loader is not supported on this "
                "system."};
      return version;
    }

    void api_version_minor(uint32_t minor) noexcept {
      app.apiVersion = ::std::clamp(VK_MAKE_API_VERSION(0, 1, minor, 0),
                                    VK_API_VERSION_1_0, max_api_version());
    }
    uint32_t api_version_minor() noexcept {
      return VK_API_VERSION_MINOR(app.apiVersion);
    }

    auto handle() const noexcept { return instance_; }

    VK_ VkPhysicalDevice physical_device(uint32_t index) const {
      uint32_t count = 0u;
      VK_ vkEnumeratePhysicalDevices(instance_, &count, nullptr) |
          popup{"[INSTANCE] Enumerate physical devices failure."};
      if (index >= count) {
        throw error{int(VK_ VK_ERROR_INITIALIZATION_FAILED),
                    "[INSTANCE] Physical device index is out of range."};
      }
      vector<VK_ VkPhysicalDevice> devices(count);
      VK_ vkEnumeratePhysicalDevices(instance_, &count, devices.data()) |
          popup{"[INSTANCE] Enumerate physical devices failure."};

      return devices[index];
    }

  protected:
    VK_ VkInstanceCreateInfo info;
    VK_ VkApplicationInfo app;

    copyable_if_null<VK_ VkInstance> instance_{VK_NULL_HANDLE};
  };

  using namespace instance_extensions;

#define VKTL_INS_FN_(name, instance)                                           \
  ((VK_ PFN_##name)VK_ vkGetInstanceProcAddr(instance, #name))

  template <typename N> struct m<debug_utils, N> : N {
    constexpr m(debug_utils const &utils, auto &&...others)
        : N{forward_(others)...} {
      N::append_extensions(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    void relocate() {
      N::relocate();
      debug_messanger.pNext = ::std::exchange(N::info.pNext, &debug_messanger);
    }

  protected:
    VK_ VkDebugUtilsMessengerCreateInfoEXT debug_messanger{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  };

  template <typename N> struct m<validation_layer_, N> : N {
    constexpr m(validation_layer_, auto &&...others) : N{forward_(others)...} {
      N::append_layers("VK_LAYER_KHRONOS_validation");
    }
  };

  // other object might query from them, thus must use m<xxx>.

#if defined(VK_VERSION_1_1)
  template <typename N> struct m<version1_1, N> : N {
    constexpr m(version1_1, auto &&...others) : N{forward_(others)...} {
      N::app.apiVersion = VK_API_VERSION_1_1;
    }
  };
#else
  template <typename N> struct m<version1_1, N> : N {
    static_assert(always_false<N>, "Update sdk to support version 1.1.");
  };
#endif
#if defined(VK_VERSION_1_2)
  template <> struct is_queryable<version1_2, version1_1> : ::std::true_type {};
  template <typename N> struct m<version1_2, N> : N {
    constexpr m(version1_2, auto &&...others) : N{forward_(others)...} {
      N::app.apiVersion = VK_API_VERSION_1_2;
    }
  };
#else
  template <typename N> struct m<version1_2, N> : N {
    static_assert(always_false<N>, "Update sdk to support version 1.2.");
  };
#endif
#if defined(VK_VERSION_1_3)
  template <> struct is_queryable<version1_3, version1_1> : ::std::true_type {};
  template <> struct is_queryable<version1_3, version1_2> : ::std::true_type {};
  template <typename N> struct m<version1_3, N> : N {
    constexpr m(version1_3, auto &&...others) : N{forward_(others)...} {
      N::app.apiVersion = VK_API_VERSION_1_3;
    }
  };
#else
  template <typename N> struct m<version1_3, N> : N {
    static_assert(always_false<N>, "Update sdk to support version 1.3.");
  };
#endif

#if defined(VK_VERSION_1_4)
  template <> struct is_queryable<version1_4, version1_1> : ::std::true_type {};
  template <> struct is_queryable<version1_4, version1_2> : ::std::true_type {};
  template <> struct is_queryable<version1_4, version1_3> : ::std::true_type {};
  template <typename N> struct m<version1_4, N> : N {
    constexpr m(version1_4, auto &&...others) : N{forward_(others)...} {
      N::app.apiVersion = VK_API_VERSION_1_4;
    }
  };
#else
  template <typename N> struct m<version1_4, N> : N {
    static_assert(always_false<N>, "Update sdk to support version 1.4.");
  };
#endif
}
