#pragma once

// --- Agents specification -------------------------------------------------
// Vulkan 1.1+ device features are stored in a compile-time tuple selected
// from the parent instance version. relocate() must rebuild their pNext chain.
// --------------------------------------------------------------------------

// VKTL_EXPORT_ namespace vktl::vptr {
// 	using sampler = handle_owner<VK_ VkSampler>;
// }

namespace VK_NAMESPACE {
inline constexpr bool
operator==(VK_ VkDescriptorSetLayoutBinding const &a,
           VK_ VkDescriptorSetLayoutBinding const &b) noexcept {
  return a.binding == b.binding && a.descriptorType == b.descriptorType &&
         a.descriptorCount == b.descriptorCount && a.stageFlags == b.stageFlags;
}
inline constexpr bool operator==(VK_ VkPushConstantRange const &a,
                                 VK_ VkPushConstantRange const &b) noexcept {
  return a.offset == b.offset && a.size == b.size &&
         a.stageFlags == b.stageFlags;
}
inline constexpr VK_ VkDescriptorSetLayoutBinding
operator|(VK_ VkDescriptorSetLayoutBinding const &left,
          VK_ VkDescriptorSetLayoutBinding const &right) noexcept {
  auto copy = left;
  copy.stageFlags |= right.stageFlags;
  return copy;
}
} // namespace VK_NAMESPACE

VKTL_EXPORT_ namespace vktl::detail {

  struct default_descriptor_set_layout {
    using layouts_type =
        vectors<VK_ VkDescriptorSetLayoutBinding, vector<VK_ VkSampler>
#if defined(VK_EXT_descriptor_indexing)
                ,
                VK_ VkDescriptorBindingFlagsEXT
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
                ,
                vector<VK_ VkDescriptorType>
#endif
                >;
    using element_type = ::std::ranges::range_value_t<layouts_type>;

    struct scope {
      uint32_t index = 0u;     // descriptor range index.
      uint32_t element = 0u;   // index inside descriptor range.
      bool is_mutable = false; // descriptor type is mutable.
      bool is_indexing = false;
      bool have_inline_uniform = false;
    };

    VK_ VkDescriptorSetLayoutCreateFlags flags = 0u;
    layouts_type layouts;

    // AGENT SPECIFICATION: DO NOT TOUCH THIS FUNCTION.
    scope add(VK_ VkDescriptorSetLayoutBinding const &binding,
              vector<VK_ VkSampler> sampler = {}
#if defined(VK_EXT_descriptor_indexing)
              , VK_ VkDescriptorBindingFlagsEXT flags = 0u
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
              , vector<VK_ VkDescriptorType> types = {}
#endif
    ) {
      // VKTL_ASSERT(binding.descriptorCount == sampler.size());
      uint32_t bind_offset = binding.binding;
      bool is_mutable = false;
      bool is_indexing = false;
      auto it = layouts.begin();
      while (it != layouts.end()) {
        auto &value = it.get<0u>();
        if (value.binding + value.descriptorCount < binding.binding) {
          it++;
        } else {
          break;
        }
      }
      // need merge. DO NOT MODIFY.
      while (true) {
        if (it == layouts.end() ||
            subres.intersected(it.get<0u>().binding,
                               it.get<0u>().descriptorCount, binding.binding,
                               binding.descriptorCount)) {
          bool can_break = false;
          auto left_binding = it.get<0u>();
          auto intersected = subres.get_intersect(
              left_binding.binding, left_binding.descriptorCount,
              binding.binding, binding.descriptorCount);

          it.get<0u>().binding = intersected.offset;
          it.get<0u>().descriptorCount = intersected.size;
          it.get<0u>().stageFlags |= binding.stageFlags;

          auto left_samplers = ::std::move(it.get<1u>());
          ::std::move(sampler.begin(), sampler.begin() + intersected.size,
                      it.get<1u>().end());
          sampler.erase(sampler.begin(), sampler.begin() + intersected.size);

#if defined(VK_EXT_descriptor_indexing)
          auto left_flags = it.get<2u>();
          auto binding_flags = left_flags | flags;
          it.get<2u>() |= flags;
          if (binding_flags != 0u)
            is_indexing = true;
          if ((binding_flags &
               VK_ VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT) != 0u) {
            this->flags |= VK_
                VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
          }
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
          auto left_types = ::std::move(it.get<3u>());
          auto copy = left_types;
          copy.insert(copy.end(), types.begin(), types.end());
          if (left_binding.descriptorType != binding.descriptorType) {
            if (left_binding.descriptorType == VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM) {
              left_binding.descriptorType = binding.descriptorType;
            } else {
              it.get<0u>().descriptorType =
                  VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE;
              if (left_binding.descriptorType !=
                  VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE) {
                insert(copy, left_binding.descriptorType);
              }
              if (binding.descriptorType !=
                  VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE) {
                insert(copy, binding.descriptorType);
              }
              is_mutable = true;
            }
          }
          
          it.get<3u>() = ::std::move(copy);
#else
          VKTL_ASSERT(
              left_binding.descriptorType ==
              bindings
                  .descriptorType); // update sdk to support mutable descriptor.
#endif
          auto not_intersected = subres.get_not_intersected(
              left_binding.binding, left_binding.descriptorCount,
              binding.binding, binding.descriptorCount);
          can_break = not_intersected.count == 2u;
          for (auto c : ::std::move(not_intersected)) {
            if (c.is_first) {
              it = layouts.insert(it, left_binding, ::std::move(left_samplers)
#if defined(VK_EXT_descriptor_indexing)
                                                        ,
                                  left_flags
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
                                  ,
                                  ::std::move(left_types)
#endif
              );
            } else {
              it = layouts.insert(it, binding, ::std::move(sampler)
#if defined(VK_EXT_descriptor_indexing)
                                                   ,
                                  flags
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
                                  ,
                                  ::std::move(types)
#endif
              );
            }
          }
          if (can_break) {
            break;
          }
        } else if (binding.binding + binding.descriptorCount == it.get<0u>().binding) {
          it = layouts.insert(it, binding, ::std::move(sampler)
#if defined(VK_EXT_descriptor_indexing)
          , flags
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
          , ::std::move(types)
#endif
          );
          break;
        }
      }
      return {
          .index = uint32_t(::std::distance(layouts.begin(), it)),
          .element = it.get<0u>().binding - bind_offset,
          .is_mutable = is_mutable,
          .is_indexing = is_indexing,
      };
    }

    scope add(element_type const &value) {
      return add(get<0u>(value), get<1u>(value),
#if defined(VK_EXT_descriptor_indexing)
                 get<2u>(value),
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
                 get<3u>(value)
#endif
      );
    }

    scope add(element_type &&value) {
      return add(get<0u>(value), ::std::move(get<1u>(value)),
#if defined(VK_EXT_descriptor_indexing)
                 get<2u>(value),
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
                 ::std::move(get<3u>(value))
#endif
      );
    }

    void add(VK_ VkSampler sampler, uint32_t binding) {
      auto it = ::std::ranges::find_if(layouts, [&](auto const &value) -> bool {
        VK_ VkDescriptorSetLayoutBinding const &val = get<0u>(value);
        VKTL_ASSERT(!subres.intersected(
          val.binding, val.descriptorCount, binding, 1u)); // Append sampler is already exist is not allowed.
        return subres.adjacent(val.binding, val.descriptorCount, binding, 1u) || val.binding > binding + 1u;
      });
      if (it == layouts.end() || it.template get<0u>().binding > binding + 1u) {
        it = layouts.insert(it, VK_ VkDescriptorSetLayoutBinding{
            .descriptorType = VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM,
          }, {sampler}, by_default, by_default);
        it.template get<0u>().pImmutableSamplers = it.template get<1u>().data();
      } else {
        auto &samplers = it.template get<1u>();
        if (samplers.size() < binding) {
          samplers.resize(binding + 1u);
        }
        samplers[binding] = sampler;
        it.template get<0u>().pImmutableSamplers = samplers.data();
      }
    }

    bool operator==(const default_descriptor_set_layout &other) const noexcept {
      return flags == other.flags && layouts == other.layouts;
    }

  private:
    static constexpr void insert(vector<VK_ VkDescriptorType> &vec, VK_ VkDescriptorType type) {
      auto it = ::std::ranges::find(vec, type);
      if (it == vec.end()) {
        vec.emplace_back(type);
      }
    }
  };

  struct default_pipeline_layout {
    VK_ VkPipelineLayoutCreateFlags flags = 0u;
    vector<VK_ VkDescriptorSetLayout> layouts;
    vector<VK_ VkPushConstantRange> push_constants;

    constexpr bool
    operator==(default_pipeline_layout const &other) const noexcept {
      return flags == other.flags && layouts == other.layouts &&
             push_constants == other.push_constants;
    }
  };

  namespace queues {
  queue_duty::type to_duty(VK_ VkQueueFlags vk_flags) {
    queue_duty::type result = queue_duty::none;
    if (vk_flags & VK_ VK_QUEUE_COMPUTE_BIT)
      result |= queue_duty::compute;
    if (vk_flags & VK_ VK_QUEUE_TRANSFER_BIT)
      result |= queue_duty::transfer;
    if (vk_flags & VK_ VK_QUEUE_GRAPHICS_BIT)
      result |= queue_duty::graphics;
    if (vk_flags & VK_ VK_QUEUE_SPARSE_BINDING_BIT)
      result |= queue_duty::bind_sparse;
    return result;
  }
  } // namespace queues

  struct queue_duty_family {
    uint16_t family;
    queue_duty::type duty;
  };

  template <typename T> struct device_querion;

  template <typename N> struct basic_device : basic_extensions<N> {
    using base = basic_extensions<N>;

    constexpr basic_device(device const &d, auto &&...others)
        : base{forward_(others)...} {
      device_index_ = d.index;
    }

    constexpr void relocate() noexcept {
      N::relocate();

      auto extensions = base::extensions();
      info.enabledExtensionCount = uint32_t(extensions.size());
      info.ppEnabledExtensionNames = extensions.data();
      info.enabledLayerCount = 0u;
      info.ppEnabledLayerNames = nullptr;

      for (auto &[queue, priorities] : queue_infos) {
        queue.pQueuePriorities = priorities.data();
      }
      info.queueCreateInfoCount = uint32_t(queue_infos.size());
      info.pQueueCreateInfos = queue_infos.data<0u>();
    }

    constexpr bool append_extensions(const char *layer, uint16_t disabled_minor = api::max_minor + 1u) {
      VKTL_ASSERT(!handle_); // cannot append extensions after initialized.
      VKTL_ASSERT(
          parent_of<instance>(this)->api_version_minor() >=
          enable_minor); // Specified minor not support specified extensions.
      return base::append_extensions(layer, disabled_minor);
    }

    constexpr auto handle() const noexcept { return handle_.value; }
    constexpr auto physical_device() const noexcept { return phydv_; }

    template <typename... Ts> constexpr bool query_support() {
      init_phydv();
      return ((device_querion<Ts>::query(phydv_)) && ...);
    }

    // WARNING: if you using this function, then `instance` or `device group`
    // will be initialized.
    vector<queue_duty::type> queue_family_duties() const {
      init_phydv();
      vector<VK_ VkQueueFamilyProperties> props;
      vkget(props, VK_ vkGetPhysicalDeviceQueueFamilyProperties, phydv_);
      vector<queue_duty_family> result(props.size());
      uint16_t family_index = 0u;
      (void)::std::ranges::transform(props, result.begin(), [&](auto &props) {
        return queue_duty_family{family_index++,
                                 queues::to_duty(props.queueFlags)};
      });
      return result;
    }

#if VKTL_HAVE_WINDOW
    // WARNING: if you using this function, then `instance` or `device group`
    //          will be initialized.
    void queue_family_allow_present(uint32_t family, object_of<window> auto& surface) const {
      init_phydv();

      VK_ VkBool32 supported = VK_FALSE;
      VK_ vkGetPhysicalDeviceSurfaceSupportKHR(physical_device(), 
        family, surface.handle(), &supported) 
        | popup{"[EXECUTION] Failed to query presentation support."};
      if (!supported) {
        throw error{int(VK_ VK_ERROR_FEATURE_NOT_PRESENT), 
          "[EXECUTION] Queue family does not support the surface."};
      }
    }
#endif
    // create descriptor set layout.
    VK_ VkDescriptorSetLayout create(default_descriptor_set_layout const& value) {
      auto _ = locker_of(this);
      if (!handle_) { init(); }
      for (auto set_layout : set_layouts_) {
        if (get<1>(set_layout) == value) {
          return get<0>(set_layout);
        }
      }

      auto c = value.layouts.template column<0u>();
      VK_ VkDescriptorSetLayoutCreateInfo create_info{
          .sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          .flags = value.flags,
          .bindingCount = uint32_t(c.size()),
          .pBindings = c.data(),
      };

#if defined(VK_EXT_descriptor_indexing)
      span<VK_ VkDescriptorBindingFlagsEXT> flags_exts 
        = value.layouts.template column<2u>();
      VK_ VkDescriptorSetLayoutBindingFlagsCreateInfoEXT binding_flags_info{
        .sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
        .bindingCount = uint32_t(flags_exts.size()),
        .pBindingFlags = flags_exts.data(),
      };
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
      vector<VK_ VkMutableDescriptorTypeListEXT> mutables;
      mutables.reserve(value.layouts.size());
      VK_ VkMutableDescriptorTypeCreateInfoVALVE mutable_descriptor_info{
          .sType = VK_ VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE,
      };
#endif
      auto binding_index = 0u;
      for (auto&&[stored_binding, samplers
#if defined(VK_EXT_descriptor_indexing)
        , ext_flags
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
        , types
#endif
      ] : value.layouts) {
        (void)stored_binding;
        (void)samplers;
#if defined(VK_EXT_descriptor_indexing)
        (void)ext_flags;
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
        mutables.emplace_back(uint32_t(types.size()), types.data());
#endif
      }

#if defined(VK_VALVE_mutable_descriptor_type)
      mutable_descriptor_info.mutableDescriptorTypeListCount =
          uint32_t(mutables.size());
      mutable_descriptor_info.pMutableDescriptorTypeLists = mutables.data();
      mutable_descriptor_info.pNext =
          ::std::exchange(create_info.pNext, &mutable_descriptor_info);
#endif
#if defined(VK_EXT_descriptor_indexing)
      binding_flags_info.pNext =
          ::std::exchange(create_info.pNext, &binding_flags_info);
#endif
      VK_ VkDescriptorSetLayout layout;
      VK_ vkCreateDescriptorSetLayout(handle_,
        &create_info, N::allocator(), &layout) 
        | popup{"[DESCRIPTOR SET LAYOUT] Create descriptor set layout failure."};

      set_layouts_.emplace_back(layout, value);
      return layout;
    }
    // create pipeline layout.
    VK_ VkPipelineLayout create(default_pipeline_layout const& value) {
      auto _ = locker_of(this);
      if (!handle_) { init(); }
      for (auto pipe_layout : pipe_layouts_) {
        if (get<1>(pipe_layout) == value) {
          return get<0>(pipe_layout);
        }
      }

      VK_ VkPipelineLayoutCreateInfo create_info{
          .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .flags = value.flags,
          .setLayoutCount = uint32_t(value.layouts.size()),
          .pSetLayouts = value.layouts.data(),
          .pushConstantRangeCount = uint32_t(value.push_constants.size()),
          .pPushConstantRanges = value.push_constants.data(),
      };

      VK_ VkPipelineLayout result{};
      VK_ vkCreatePipelineLayout(handle_,
        &create_info, N::allocator(), &result) 
        | popup{"[PIPELINE LAYOUT] Create pipeline layout failure."};

      pipe_layouts_.emplace_back(result, value);
      return result;
    }

    auto &queues() const noexcept { return queue_infos; }

    constexpr void append(queue_ info) {
      VKTL_ASSERT(!handle_);
      auto it = ::std::ranges::lower_bound(queue_infos, [&](auto const &value) {
        return info.family < value.queueFamilyIndex;
      });
      if (it == queue_infos.end() || it->queueFamilyIndex != info.family) {
        it = queue_infos.insert(
            it,
            VK_ VkDeviceQueueCreateInfo{
                .sType = VK_ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = info.family,
                .queueCount = 1u,
            },
            vector{0.5f});
      } else {
        it->queueCount++;
      }
      last_queue_set_ = uint16_t(::std::distance(queue_infos.begin(), it));
    }

  protected:
    void init() {
      if (!handle_) {
        init_phydv();
        VK_ vkCreateDevice(phydv_, &info, N::allocator(), &handle_) |
            popup{"[Device] Create device failure."};
        (void)this->create(default_descriptor_set_layout{});
        (void)this->create(default_pipeline_layout{});
      }
    }

    void reset() {
      if (handle_) {
        N::reset();
        for (auto const &[layout, _] : this->pipe_layouts_) {
          VK_ vkDestroyPipelineLayout(handle_, layout, N::allocator());
        }
        for (auto const &[layout, _] : this->set_layouts_) {
          VK_ vkDestroyDescriptorSetLayout(handle_, layout, N::allocator());
        }
        pipe_layouts_.clear();
        set_layouts_.clear();
        VK_ vkDestroyDevice(::std::exchange(handle_.value, VK_NULL_HANDLE),
                            N::allocator());
      }
    }

  private:
    void init_phydv() {
      if (!phydv_) {
        N::init();
        phydv_ = parent<instance>(this)->physical_device(device_index_);
      }
    }

  protected:
    VK_ VkDeviceCreateInfo info{.sType =
                                    VK_ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    union {
      uint32_t flags = 0u; // after initialization.
      struct {             // before initialization.
        uint16_t last_queue_set_;
        uint16_t reserved;
      };
    };
    vectors<VK_ VkDeviceQueueCreateInfo, vector<float>> queue_infos;

  private:
    uint32_t device_index_ = 0u;
    VK_ VkPhysicalDevice phydv_{VK_NULL_HANDLE};
    copyable_if_null<VK_ VkDevice> handle_{VK_NULL_HANDLE};
    vectors<VK_ VkDescriptorSetLayout, default_descriptor_set_layout>
        set_layouts_;
    vectors<VK_ VkPipelineLayout, default_pipeline_layout> pipe_layouts_;
  };

  template <typename N>
    requires(!have_parent_of<instance, version1_1>)
  struct m<device, N> : basic_device<N> {
    using base = basic_device<N>;

    m(auto &&...others) : base{forward_(others)...} {}

    void relocate() {
      base::relocate();
      this->info.pEnabledFeatures = &features;
    }

  protected:
    VK_ VkPhysicalDeviceFeatures features = {};
  };

#if defined(VK_VERSION_1_1)

  namespace version {
  template <typename N> constexpr auto device_feature_tuple() {
#if defined(VK_VERSION_1_4)
    if constexpr (inside_parent<N, instance, version1_4>) {
      return ::std::tuple{
          VK_ VkPhysicalDeviceVulkan11Features{
              .sType =
                  VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES},
          VK_ VkPhysicalDeviceVulkan12Features{
              .sType =
                  VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES},
          VK_ VkPhysicalDeviceVulkan13Features{
              .sType =
                  VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES},
          VK_ VkPhysicalDeviceVulkan14Features{
              .sType =
                  VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES},
      };
    } else
#endif
#if defined(VK_VERSION_1_3)
        if constexpr (inside_parent<N, instance, version1_3>) {
      return ::std::tuple{
          VK_ VkPhysicalDeviceVulkan11Features{
              .sType =
                  VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES},
          VK_ VkPhysicalDeviceVulkan12Features{
              .sType =
                  VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES},
          VK_ VkPhysicalDeviceVulkan13Features{
              .sType =
                  VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES},
      };
    } else
#endif
#if defined(VK_VERSION_1_2)
        if constexpr (have_parent_of<N, instance, version1_2>) {
      return ::std::tuple{
          VK_ VkPhysicalDeviceVulkan11Features{
              .sType =
                  VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES},
          VK_ VkPhysicalDeviceVulkan12Features{
              .sType =
                  VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES},
      };
    } else
#endif
    {
      return ::std::tuple{
          VK_ VkPhysicalDeviceVulkan11Features{
              .sType =
                  VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES},
      };
    }
  }
  template <typename N>
  using device_feature_tuple_t = decltype(device_feature_tuple<N>());
  } // namespace version

  template <have_parent_of<instance, version1_1> N>
  struct m<device, N> : basic_device<N> {
    using base = basic_device<N>;
    using feature_tuples_t = decltype(version::device_feature_tuple<N>());

    constexpr m(auto &&...others) : base{forward_(others)...} {}

    void relocate() {
      base::relocate();
      features.pNext = &get<0u>(version_features);
      this->info.pEnabledFeatures = features.features;
      vkconnect(version_features).pNext =
          ::std::exchange(this->info.pNext, &features);
    }

    template <typename T, typename V>
    friend void set_features(m &, V T::*value, VK_ VkBool32 enable = false)
      requires(find_if_same_v<feature_tuples_t, T> != invalid)
    {
      (get<T>(version_features).*value) = enable;
    }

  protected:
    VK_ VkPhysicalDeviceFeatures2KHR features = {
        .sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
    };
    feature_tuples_t version_features = version::device_feature_tuple<N>();
  };

#endif

  using namespace device_extensions;
}
