#pragma once

// --- Agents specification -------------------------------------------------
// A pass uses logical Vulkan set indices. When it is adopted by a bind set,
// `affine[pass_set]` names the corresponding set index in that bind set.
// Resource usages retain their declared count in the pass; bind-set adoption
// expands that count into resource-indexed descriptor points.
// `sampled_image::sampler_index` becomes `related_index`; an invalid sampler
// index declares a sampled-image descriptor instead of a combined descriptor.
// Push-constant ranges stay ordered by offset. Declarations with the same
// offset and size share one range whose shader stage flags are accumulated.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::vptr {
  struct bind_set_from_pass {
    template <typename C> using base = apply_compose<C, initable>;
    template <typename C> struct apply;

    template <typename T> constexpr auto rebind() noexcept {
      layouts_ = [](void const *ptr, uint64_t id) {
        return static_cast<T const *>(ptr)->layouts(id);
      };
    }

    vfn<detail::vector<VK_ VkDescriptorSetLayout>(uint64_t) const> layouts_;
  };

  template <typename C> struct bind_set_from_pass::apply : base<C> {
    using base = base<C>;

    detail::vector<VK_ VkDescriptorSetLayout> layouts(uint64_t id) const {
      return vptr.layouts_(C::get_this(), id);
    }

    bind_set_from_pass vptr;
  };
}

VKTL_EXPORT_ namespace vktl::detail {

  namespace resource_attributes {
  using type = uint64_t;
  }
  struct resource_binding {
    VK_ VkDescriptorType type = VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
    uint16_t set = invalid;      // for descriptor set.
    uint16_t binding = invalid;  // for descriptor set.
    uint32_t per_size = invalid; // for descriptor buffer or descriptor heap,
                                 // mark per slot size.
    uint32_t offset = invalid;   // for descriptor buffer or descriptor heap.

    constexpr bool uses_descriptor() const noexcept {
      return type != VK_ VK_STRUCTURE_TYPE_MAX_ENUM;
    }
  };
  struct resource_access {
    uint64_t stages = 0u;   // Undelying type is VkPipelineStages2.
    uint64_t accesses = 0u; // Undelying type is VkPipelineAccess2.
    VK_ VkDependencyFlags dependency = VK_ VkDependencyFlags(0u);
    uint32_t reserved = 0u;
  };

  struct resource_declaration {
    VK_ VkObjectType resource_type = VK_ VK_OBJECT_TYPE_UNKNOWN;
    uint16_t index = invalid;
    // disable when use `rendering`.
    uint16_t subpass = invalid;
    // for sampled image to store sampler index.
    uint16_t related_index = invalid;
    uint16_t count = 0u;
    uint64_t usages = 0u;
    resource_access access;
    resource_binding binding;
    uint64_t attributes = 0u;

    constexpr bool uses_descriptor() const noexcept {
      return binding.uses_descriptor();
    }

    constexpr VK_ VkPipelineStageFlags legacy_stages() const noexcept {
      auto stages = access.stages;
      VK_ VkPipelineStageFlags result = VK_ VkPipelineStageFlags(
          stages & VK_ VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM);
#if defined(VK_KHR_synchronization2)
      if (stages & (VK_ VK_PIPELINE_STAGE_2_COPY_BIT |
                    VK_ VK_PIPELINE_STAGE_2_RESOLVE_BIT |
                    VK_ VK_PIPELINE_STAGE_2_BLIT_BIT |
                    VK_ VK_PIPELINE_STAGE_2_CLEAR_BIT)) {
        result |= VK_ VK_PIPELINE_STAGE_TRANSFER_BIT;
      }

      if (stages & (VK_ VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT |
                    VK_ VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT)) {
        result |= VK_ VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      }

      if (stages & VK_ VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT) {
        result |= VK_ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                  VK_ VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                  VK_ VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
                  VK_ VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
#if defined(VK_EXT_mesh_shader)
                  | VK_ VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT 
                  | VK_ VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT
#endif
            ;
      }
#endif
      return result;
    }

    constexpr VK_ VkAccessFlags legacy_accesses() const noexcept {
      auto access = this->access.accesses;
      VK_ VkAccessFlags result =
          VK_ VkAccessFlags(access & VK_ VK_ACCESS_FLAG_BITS_MAX_ENUM);
#if defined(VK_KHR_synchronization2)
      if (access & (VK_ VK_ACCESS_2_SHADER_SAMPLED_READ_BIT |
                    VK_ VK_ACCESS_2_SHADER_STORAGE_READ_BIT)) {
        result |= VK_ VK_ACCESS_SHADER_READ_BIT;
      }

      if (access & VK_ VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT) {
        result |= VK_ VK_ACCESS_SHADER_WRITE_BIT;
      }

#if defined(VK_KHR_ray_tracing_maintenance1)
      if (access & VK_ VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR) {
        result |= VK_ VK_ACCESS_SHADER_READ_BIT;
      }
#endif
#endif
      return result;
    }

    constexpr bool have_read() const noexcept {
      constexpr uint64_t reads =
          VK_ VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
          VK_ VK_ACCESS_INDEX_READ_BIT |
          VK_ VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
          VK_ VK_ACCESS_UNIFORM_READ_BIT |
          VK_ VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
          VK_ VK_ACCESS_SHADER_READ_BIT |
          VK_ VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
          VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
          VK_ VK_ACCESS_TRANSFER_READ_BIT | VK_ VK_ACCESS_HOST_READ_BIT |
          VK_ VK_ACCESS_MEMORY_READ_BIT
#if defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2)
          | VK_ VK_ACCESS_2_SHADER_SAMPLED_READ_BIT |
          VK_ VK_ACCESS_2_SHADER_STORAGE_READ_BIT
#endif
#if defined(VK_KHR_acceleration_structure)
          | VK_ VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR
#endif
#if defined(VK_EXT_transform_feedback)
          | VK_ VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT
#endif
#if defined(VK_EXT_conditional_rendering)
          | VK_ VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT
#endif
          ;

      return (this->access.accesses & reads) != 0;
    }

    constexpr bool have_write() const noexcept {
      constexpr uint64_t writes =
          VK_ VK_ACCESS_SHADER_WRITE_BIT |
          VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
          VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
          VK_ VK_ACCESS_TRANSFER_WRITE_BIT | VK_ VK_ACCESS_HOST_WRITE_BIT |
          VK_ VK_ACCESS_MEMORY_WRITE_BIT
#if defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2)
          | VK_ VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
#endif
#if defined(VK_KHR_acceleration_structure)
          | VK_ VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
#endif
#if defined(VK_EXT_transform_feedback)
          | VK_ VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT 
          | VK_ VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT
#endif
          ;

      return (this->access.accesses & writes) != 0;
    }

#if defined(VK_EXT_descriptor_indexing)
    VK_ VkDescriptorBindingFlagsEXT binding_flags() const noexcept {
      return VK_ VkDescriptorBindingFlagsEXT(0u);
    }
#endif
  };

  struct graphics_pass;
  struct compute_pass;
  struct pass_need_descriptor;

  namespace passes {
    using id = ::std::uint64_t;
    inline id id_ = 0u;
    inline bool lock = false;
    // AGENT SPECIFICATION: Do not modify these method.
    // TODO: replace other constexpr compactible
    //       and multithread safe method.
    inline constexpr uint64_t acquire_id() noexcept {
      while (::std::exchange(lock, true)) {
      }
      auto value = id_++;
      lock = false;
      return value;
    }
  } // namespace passes

  using namespace pass_extensions;

  template <>
  struct is_queryable<pass_extensions::render_pass_, graphics_pass>
      : ::std::true_type {};
  template <>
  struct is_queryable<pass_extensions::rendering_, graphics_pass>
      : ::std::true_type {};
  template <>
  struct is_queryable<pass_extensions::compute_, compute_pass>
      : ::std::true_type {};

  template <>
  struct is_queryable<pass_extensions::compute_, pass_need_descriptor>
      : ::std::true_type {};
  template <>
  struct is_queryable<pass_extensions::render_pass_, pass_need_descriptor>
      : ::std::true_type {};
  template <>
  struct is_queryable<pass_extensions::rendering_, pass_need_descriptor>
      : ::std::true_type {};

  template <typename N> struct m<pass_, N> : N {
    // static_assert(is_lockable<N>, "lock pass instance is meaningless.");

    constexpr m(pass_, auto &&...other) : N{forward_(other)...} {}

    passes::id id() const noexcept { return id_; }
    auto &resource_usages() const noexcept { return usages; }

  protected:
    constexpr void append(resource_declaration const &usage) {
      usages.emplace_back(usage);
    }

  protected:
    passes::id id_ = passes::acquire_id();
    vector<resource_declaration> usages;
  };

  inline constexpr auto to_shader_stages(
      VK_ VkPipelineStageFlags stages) noexcept {
    VK_ VkShaderStageFlags result = 0u;
    if (stages & VK_ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT)
      result |= VK_ VK_SHADER_STAGE_VERTEX_BIT;
    if (stages & VK_ VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT)
      result |= VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (stages & VK_ VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT)
      result |= VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if (stages & VK_ VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT)
      result |= VK_ VK_SHADER_STAGE_GEOMETRY_BIT;
    if (stages & VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
      result |= VK_ VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stages & VK_ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
      result |= VK_ VK_SHADER_STAGE_COMPUTE_BIT;
    if (stages & VK_ VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT)
      result |= VK_ VK_SHADER_STAGE_ALL_GRAPHICS;
    if (stages & VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
      result |= VK_ VK_SHADER_STAGE_ALL;
    return result;
  }

  inline constexpr auto pipeline_stage_of(
      VK_ VkShaderStageFlagBits stage) noexcept {
    switch (stage) {
    case VK_ VK_SHADER_STAGE_VERTEX_BIT:
      return VK_ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    case VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
      return VK_ VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
    case VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
      return VK_ VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    case VK_ VK_SHADER_STAGE_GEOMETRY_BIT:
      return VK_ VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    case VK_ VK_SHADER_STAGE_FRAGMENT_BIT:
      return VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_ VK_SHADER_STAGE_COMPUTE_BIT:
      return VK_ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    default:
      VKTL_ASSERT(false);
    }
  }

  template <typename N> struct basic_pipe_pass : N {
    constexpr basic_pipe_pass(auto &&...args) : N{forward_(args)...} {}

    constexpr void bind_set(object_of<bind_set_> auto &binds) noexcept {
      if (bind_set_.empty()) {
        this->bind_set_ = {binds};
      }
    }

  protected:
    void init(void *pnext = nullptr) {
      N::init();
      if (pipeline_layout_) {
        return;
      }

      auto pdv = parent_of<device>(this);
      auto hdv = pdv->handle();
      try {
        VK_ vkCreatePipelineCache(hdv, &cache_info, N::allocator(), &cache_) |
            popup{"[PASS] Create pipeline cache failure."};

        assert(!bind_set_.empty()); // currently not allow not set bind set.

        vector<VK_ VkDescriptorSetLayout> layouts
          = bind_set_.layouts(this->id());
        pipe_layout_info.layouts = ::std::move(layouts);
        pipeline_layout_.value = pdv->create(pipe_layout_info);
      } catch (...) {
        reset();
        throw;
      }
    }

    void reset() {
      if (cache_) {
        VK_ vkDestroyPipelineCache(handle_of<device>(this),
            ::std::exchange(cache_.value, VK_NULL_HANDLE), N::allocator());
      }
      pipeline_layout_.value = VK_NULL_HANDLE;
      N::reset();
    }

    void finalize() {
      N::finalize();

      for (auto const &usage : this->usages) {
        if (!usage.uses_descriptor())
          continue;
        VKTL_ASSERT(usage.set != invalid);
        VKTL_ASSERT(usage.binding != invalid);
        auto shader_stages = to_shader_stages(usage.stages);
        VKTL_ASSERT(shader_stages != 0u);
      }
    }

    VK_ VkPipelineLayout pipeline_layout() const noexcept {
      return pipeline_layout_.value;
    }

    void append(VK_ VkPushConstantRange const &range) {
      vector<VK_ VkPushConstantRange> &ranges = pipe_layout_info.push_constants;
      auto it = ::std::ranges::upper_bound(ranges, range.offset, {},
                                           &VK_ VkPushConstantRange::offset);
      auto first = ::std::ranges::lower_bound(ranges, range.offset, {},
                                              &VK_ VkPushConstantRange::offset);
      auto same = ::std::ranges::find(first, it, range.size,
                                      &VK_ VkPushConstantRange::size);
      if (same != it) {
        same->stageFlags |= range.stageFlags;
      } else {
        ranges.emplace(it, range);
      }
    }

    VK_ VkPipelineCache cache() const noexcept { return cache_.value; }

  protected:
    VK_ VkPipelineCacheCreateInfo cache_info{
        .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
    };

  private:
    box<vptr::bind_set_from_pass> bind_set_;
    default_pipeline_layout pipe_layout_info;
    move_only<VK_ VkPipelineCache> cache_{VK_NULL_HANDLE};
    move_only<VK_ VkPipelineLayout> pipeline_layout_{VK_NULL_HANDLE};
  };

#pragma region NOBODY_LIKE_GRAPHICS_PIPELINE
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineVertexInputStateCreateInfo
      defaultVertexInputState{
          .sType =
              VK_ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      };
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineInputAssemblyStateCreateInfo
      defaultInputAssemblyState{
          .sType =
              VK_ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
          .topology = VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
          .primitiveRestartEnable = VK_FALSE,
      };
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineTessellationStateCreateInfo
      defaultTessellationState{
          .sType =
              VK_ VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
          .patchControlPoints = 3u,
      };
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineViewportStateCreateInfo
      defaultViewportState{
          .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
          .viewportCount = 1u,
          .scissorCount = 1u,
      };
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineRasterizationStateCreateInfo
      defaultRasterizationState{
          .sType =
              VK_ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
          .polygonMode = VK_ VK_POLYGON_MODE_FILL,
          .cullMode = VK_ VK_CULL_MODE_BACK_BIT,
          .frontFace = VK_ VK_FRONT_FACE_COUNTER_CLOCKWISE,
          .lineWidth = 1.0f,
      };
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineMultisampleStateCreateInfo
      defaultMultisampleState{
          .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
          .rasterizationSamples = VK_ VK_SAMPLE_COUNT_1_BIT,
          .minSampleShading = 1.0f,
      };
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineDepthStencilStateCreateInfo
      defaultDepthStencilState{
          .sType =
              VK_ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
          .depthCompareOp = VK_ VK_COMPARE_OP_LESS,
          .front =
              {
                  .failOp = VK_ VK_STENCIL_OP_KEEP,
                  .passOp = VK_ VK_STENCIL_OP_KEEP,
                  .depthFailOp = VK_ VK_STENCIL_OP_KEEP,
                  .compareOp = VK_ VK_COMPARE_OP_NEVER,
              },
          .back =
              {
                  .failOp = VK_ VK_STENCIL_OP_KEEP,
                  .passOp = VK_ VK_STENCIL_OP_KEEP,
                  .depthFailOp = VK_ VK_STENCIL_OP_KEEP,
                  .compareOp = VK_ VK_COMPARE_OP_NEVER,
              },
          .maxDepthBounds = 1.0f,
      };
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineColorBlendAttachmentState
      defaultColorBlendAttachment{
          .blendEnable = VK_FALSE,
          .srcColorBlendFactor = VK_ VK_BLEND_FACTOR_ONE,
          .dstColorBlendFactor = VK_ VK_BLEND_FACTOR_ZERO,
          .colorBlendOp = VK_ VK_BLEND_OP_ADD,
          .srcAlphaBlendFactor = VK_ VK_BLEND_FACTOR_ONE,
          .dstAlphaBlendFactor = VK_ VK_BLEND_FACTOR_ZERO,
          .alphaBlendOp = VK_ VK_BLEND_OP_ADD,
          .colorWriteMask =
              VK_ VK_COLOR_COMPONENT_R_BIT | VK_ VK_COLOR_COMPONENT_G_BIT |
              VK_ VK_COLOR_COMPONENT_B_BIT | VK_ VK_COLOR_COMPONENT_A_BIT,
      };
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineColorBlendStateCreateInfo
      defaultColorBlendState{
          .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
          .logicOp = VK_ VK_LOGIC_OP_COPY,
      };
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkDynamicState defaultDynamicStates[]{
      VK_ VK_DYNAMIC_STATE_VIEWPORT,
      VK_ VK_DYNAMIC_STATE_SCISSOR,
  };
  VKTL_MAYBE_UNUSED inline constexpr VK_ VkPipelineDynamicStateCreateInfo
      defaultDynamicState{
          .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
          .dynamicStateCount = 2u,
          .pDynamicStates = defaultDynamicStates,
      };
#pragma endregion
  namespace attachment_attribute {
    using namespace vktl::attachment_attribute;
    
    inline constexpr auto input = type(0x1u << 12u);
    inline constexpr auto color = type(0x2u << 12u);
    inline constexpr auto resolve = type(0x3u << 12u);
    inline constexpr auto depth = type(0x4u << 12u);
    inline constexpr auto stencil = type(0x5u << 12u);
    inline constexpr auto depth_stencil = type(0x6u << 12u);
    
    inline constexpr auto usage_mask = type(0x7u << 12u);
    inline constexpr auto mask = type(~0u);
  } // namespace attachment_attribute

  namespace attachment {
    using namespace attachment_attribute;
    
    inline constexpr bool has(uint64_t attributes,
                              attachment_attribute::type value) noexcept {
      return (attributes & value) != 0u;
    }
    
    inline constexpr uint64_t depth_operations = attachment_attribute::load |
                                                 attachment_attribute::clear |
                                                 attachment_attribute::store;
    inline constexpr uint64_t stencil_operations =
        attachment_attribute::load_stencil | attachment_attribute::clear_stencil |
        attachment_attribute::store_stencil;
    
    inline constexpr uint16_t index(auto const &usages,
                                    uint16_t requested) noexcept {
      if (requested != invalid)
        return requested;
      uint32_t result = 0u;
      for (auto const &usage : usages) {
        if (usage.index == invalid)
          continue;
        result = ::std::max(result, uint32_t(usage.index) + 1u);
      }
      return uint16_t(result);
    }
    
    inline constexpr VK_ VkImageLayout layout(uint16_t attributes) noexcept {
      switch (attributes & attachment_attribute::usage_mask) {
      case attachment_attribute::color:
      case attachment_attribute::resolve:
        return VK_ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      case attachment_attribute::depth:
      case attachment_attribute::stencil:
      case attachment_attribute::depth_stencil:
        return VK_ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      case attachment_attribute::input:
        return VK_ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      default:
        VKTL_ASSERT(false);
        return VK_ VK_IMAGE_LAYOUT_UNDEFINED;
      }
    }
    
    inline constexpr VK_ VkPipelineStageFlags
    stages(uint16_t attributes) noexcept {
      switch (attributes & attachment_attribute::usage_mask) {
      case attachment_attribute::input:
        return VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      case attachment_attribute::depth:
      case attachment_attribute::stencil:
      case attachment_attribute::depth_stencil:
        return VK_ VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
               VK_ VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      case attachment_attribute::color:
      case attachment_attribute::resolve:
        return VK_ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      default:
        VKTL_ASSERT(false);
        return VK_ VkPipelineStageFlags(0u);
      }
    }
    
    inline constexpr VK_ VkAccessFlags access(uint16_t attributes) noexcept {
      switch (attributes & attachment_attribute::usage_mask) {
      case attachment_attribute::input:
        return VK_ VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
      case attachment_attribute::depth:
      case attachment_attribute::stencil:
      case attachment_attribute::depth_stencil:
        return VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
               VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      case attachment_attribute::color:
      case attachment_attribute::resolve:
        return VK_ VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
               VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      default:
        VKTL_ASSERT(false);
        return VK_ VkAccessFlags(0u);
      }
    }
    
    inline constexpr VK_ VkImageUsageFlags usages(uint16_t attributes) noexcept {
      switch (attributes & attachment_attribute::usage_mask) {
      case attachment_attribute::input:
        return VK_ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
      case attachment_attribute::color:
      case attachment_attribute::resolve:
        return VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      case attachment_attribute::depth:
      case attachment_attribute::stencil:
      case attachment_attribute::depth_stencil:
        return VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      default:
        VKTL_ASSERT(false);
        return VK_ VkImageUsageFlags(0u);
      }
    }

    struct info {
      uint16_t index = invalid;
      uint16_t attributes = 0u;
    };
  } // namespace attachment

  template <typename N> struct basic_graphics_pass : basic_pipe_pass<N> {
    using base = basic_pipe_pass<N>;

    constexpr basic_graphics_pass(auto &&...infos) : base{forward_(infos)...} {}

    void relocate() noexcept {
      base::relocate();
      relocate_pipelines();
    }

    VK_ VkPipeline pipe(uint16_t index) const noexcept {
      VKTL_ASSERT(index < pipes.size());
      return pipes[index].value;
    }

    uint16_t pipe_count() const noexcept {
      VKTL_ASSERT(pipe_infos.size() <= uint16_t(maximum));
      return uint16_t(pipe_infos.size());
    }

  protected:
    void reset() {
      auto hdv = handle_of<device>(this);
      for (auto &pipe : pipes) {
        if (pipe) {
          VK_ vkDestroyPipeline(
              hdv, ::std::exchange(pipe.value, VK_NULL_HANDLE), N::allocator());
        }
      }
      destroy_shaders(hdv);
      base::reset();
    }

    void init_pipelines() {
      // TODO: not allow empty pipe?
      if (pipe_infos.empty() || pipes.front()) { return; }
      auto hdv = handle_of<device>(this);
      try {
        for (auto &&[info, shaders] : pipe_infos.template column<0u, 1u>()) {
          info.layout = base::pipeline_layout();
          info.renderPass = render_pass.value;
          for (auto &&[stage, codes] : shaders.template column<0u, 1u>()) {
            auto cinfo = VK_ VkShaderModuleCreateInfo{
                .sType = VK_ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = codes.size() * sizeof(uint32_t),
                .pCode = codes.data(),
            };
            VK_ vkCreateShaderModule(hdv, &cinfo, N::allocator(),
                                     &stage.module) |
                popup{"[PASS] Failed to create shader module."};
          }
        }

        VK_ vkCreateGraphicsPipelines(
            hdv, base::cache(), uint32_t(pipe_infos.size()),
            pipe_infos.template data<0u>(), N::allocator(), pipes.data()) |
            popup {
              "[PASS] Failed to create graphics pipelines."
            };
        destroy_shaders(hdv);
      } catch (...) {
        reset();
        throw;
      }
    }

    void append(uint32_t pipe_index, attachment::info const &info) {
      if (info.attributes & attachment_attribute::color) {
        auto &attachment = get<vector<VK_ VkPipelineColorBlendAttachmentState>>(
            pipe_infos.back());
        attachment.emplace_back();
      }
    }

    void append(VK_ VkGraphicsPipelineCreateInfo info) {
      info.pVertexInputState = &defaultVertexInputState;
      info.pInputAssemblyState = &defaultInputAssemblyState;
      info.pTessellationState = nullptr;
      info.pViewportState = &defaultViewportState;
      info.pRasterizationState = &defaultRasterizationState;
      info.pMultisampleState = &defaultMultisampleState;
      info.pDepthStencilState = &defaultDepthStencilState;
      info.pColorBlendState = &defaultColorBlendState;
      info.pDynamicState = &defaultDynamicState;
      pipe_infos.emplace_back(::std::move(info));
      pipes.emplace_back(VK_NULL_HANDLE);
    }

  private:
    void destroy_shaders(VK_ VkDevice hdv) noexcept {
      for (auto &shaders : pipe_infos.template column<1u>()) {
        for (auto &stage : shaders.template column<0u>()) {
          VK_ vkDestroyShaderModule(
              hdv, ::std::exchange(stage.module, VK_NULL_HANDLE),
              N::allocator());
        }
      }
    }
    void relocate_pipelines() noexcept {
      for (auto &&[info, shaders] : pipe_infos.template column<0u, 1u>()) {
        info.stageCount = uint32_t(shaders.size());
        info.pStages = shaders.template data<0u>();
      }
    }

  protected:
    vectors<VK_ VkGraphicsPipelineCreateInfo,
      vectors<VK_ VkPipelineShaderStageCreateInfo, 
        vector<uint32_t>>> pipe_infos;

    vector<VK_ VkPipeline> pipes;
  };

  inline constexpr bool access_writes(VK_ VkAccessFlags access) noexcept {
    constexpr auto writes = VK_ VK_ACCESS_SHADER_WRITE_BIT |
                            VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                            VK_ VK_ACCESS_TRANSFER_WRITE_BIT |
                            VK_ VK_ACCESS_HOST_WRITE_BIT |
                            VK_ VK_ACCESS_MEMORY_WRITE_BIT;
    return (access & writes) != 0u;
  }

  template <typename N> struct m<pass_extensions::render_pass_, N> : basic_graphics_pass<N> {
    using base = basic_graphics_pass<N>;

    m(pass_extensions::render_pass_, auto &&...infos)
        : base{forward_(infos)...},
          info{.sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO} {
    }

    VK_ VkRenderPass handle() const noexcept { return handle_.value; }

    // for internal usage.
    void attachment_may_alias(span<uint16_t> which) {
      // TODO: the alias dependency is not finished.
      for (auto c : which) {
        this->attachments_[c].flags |=
            VK_ VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
      }
    }

    auto &attachments() const noexcept { return this->attachments_; }

  protected:
    void init() {
      base::init();
      if (!handle_) {
        VK_ vkCreateRenderPass(handle_of<device>(this),
          &info, N::allocator(), &handle_)
          | popup{"[PASS] Create render pass failure."};
        for (VK_ VkGraphicsPipelineCreateInfo&
          pipe_info : base::pipe_infos.template column<0u>()) {
          pipe_info.renderPass = handle_;
        }

        base::init_pipelines();
      }
    }

    void reset() {
      if (handle_) {
        VK_ vkDestroyRenderPass(handle_of<device>(this),
          ::std::exchange(handle_.value, VK_NULL_HANDLE),
          N::allocator());
      }
      base::reset();
    }

    void relocate() noexcept {
      base::relocate();
      for (auto &&[subpass, inputs, colors, resolves, depth_stencil] : subpasses_) {
        subpass.inputAttachmentCount = uint32_t(inputs.size());
        subpass.pInputAttachments = data_or_null(inputs);
        subpass.colorAttachmentCount = uint32_t(colors.size());
        subpass.pColorAttachments = data_or_null(colors);
        VKTL_ASSERT(resolves.empty() || resolves.size() == colors.size()); // resolve attachment must same size with color attachment.
        subpass.pResolveAttachments = data_or_null(resolves);
        subpass.pDepthStencilAttachment = depth_stencil.index == invalid ? nullptr : depth_stencil;
      }
      info.attachmentCount = uint32_t(attachments_.size());
      info.pAttachments = data_or_null(attachments_);
      info.subpassCount = uint32_t(subpasses_.size());
      info.pSubpasses = subpasses_.empty() ? nullptr : subpasses_.template data<0u>();
      info.dependencyCount = uint32_t(dependencies_.size());
      info.pDependencies = data_or_null(dependencies_);
    }

    constexpr void finalize() {
      base::finalize();
      VKTL_ASSERT(::std::ranges::all_of(attachments_, [](auto const &value) {
        return value.samples != VK_ VkSampleCountFlagBits(0u) &&
               value.format != VK_ VK_FORMAT_UNDEFINED;
      })); // all attachments must defined.
    }

    constexpr void append(uint16_t subpass, resource_declaration const &dst) {
      for (auto const &src : base::usages) {
        if (src.index == dst.index) {
          auto copy = dst;
          copy.subpass = subpass;
          auto dependency = VK_ VkSubpassDependency{
              .srcSubpass =
                  src.subpass == invalid ? VK_SUBPASS_EXTERNAL : src.subpass,
              .dstSubpass =
                  dst.subpass == invalid ? VK_SUBPASS_EXTERNAL : subpass,
              .srcStageMask = src.access.stages,
              .dstStageMask = dst.access.stages,
              .srcAccessMask = src.access.accesses,
              .dstAccessMask = dst.access.accesses,
              .dependencyFlags = src.access.dependency | dst.access.dependency,
          };
          this->append_dependency(dependency);
        }
      }
      base::append(dst);
    }

    constexpr void append(::std::in_place_t, attachment::info const &usage) {
      VKTL_ASSERT(usage.index != invalid);
      auto const subpass 
        = uint16_t(base::pipe_infos.template back<0u>().subpass);
      if (subpasses_.size() <= subpass) {
        subpasses_.resize(subpass + 1u,
          VK_ VkSubpassDescription{ 
            .pipelineBindPoint = VK_ VK_PIPELINE_BIND_POINT_GRAPHICS, 
          });
      }

      last_attachment = usage.index;
      if (attachments_.size() <= usage.index) {
        attachments_.resize(size_t(usage.index) + 1u);
      }
      auto const image_layout = attachment::layout(usage.attributes);
      auto &description = attachments_[usage.index];
      auto const first_declaration =
          description.samples == VK_ VkSampleCountFlagBits(0u);
      if (first_declaration) {
        description = {
            .samples = VK_ VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = image_layout,
        };
      }

      if ((usage.attributes & attachment::depth_operations) != 0u) {
        if (first_declaration) {
          description.loadOp =
              attachment::has(usage.attributes, attachment_attribute::clear)
                  ? VK_ VK_ATTACHMENT_LOAD_OP_CLEAR
              : attachment::has(usage.attributes, attachment_attribute::load)
                  ? VK_ VK_ATTACHMENT_LOAD_OP_LOAD
                  : VK_ VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
        description.storeOp =
            attachment::has(usage.attributes, attachment_attribute::store)
                ? VK_ VK_ATTACHMENT_STORE_OP_STORE
                : VK_ VK_ATTACHMENT_STORE_OP_DONT_CARE;
      }
      if ((usage.attributes & attachment::stencil_operations) != 0u) {
        if (first_declaration) {
          description.stencilLoadOp =
              attachment::has(usage.attributes, attachment_attribute::clear_stencil)
                  ? VK_ VK_ATTACHMENT_LOAD_OP_CLEAR
              : attachment::has(usage.attributes, attachment_attribute::load_stencil)
                  ? VK_ VK_ATTACHMENT_LOAD_OP_LOAD
                  : VK_ VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
        description.stencilStoreOp =
            attachment::has(usage.attributes, attachment_attribute::store_stencil)
                ? VK_ VK_ATTACHMENT_STORE_OP_STORE
                : VK_ VK_ATTACHMENT_STORE_OP_DONT_CARE;
      }
      if (first_declaration &&
          attachment::has(usage.attributes, 
            attachment_attribute::clear | attachment_attribute::clear_stencil)) {
        description.initialLayout = VK_ VK_IMAGE_LAYOUT_UNDEFINED;
      }
      description.finalLayout = image_layout;

      auto reference = VK_ VkAttachmentReference{
          .attachment = usage.index,
          .layout = image_layout,
      };
      auto &subpass_info = subpasses_[subpass];
      // if (generalize_reference(subpass_info, usage.index)) {
      //   reference.layout = VK_ VK_IMAGE_LAYOUT_GENERAL;
      // }
      switch (usage.attributes & attachment_attribute::usage_mask) {
      case attachment_attribute::input:
        insert_reference(get<1u>(subpass_info), reference);
        break;
      case attachment_attribute::color:
        insert_reference(get<2u>(subpass_info), reference);
        break;
      case attachment_attribute::resolve:
        insert_reference(get<3u>(subpass_info), reference);
        break;
      case attachment_attribute::depth:
      case attachment_attribute::stencil:
      case attachment_attribute::depth_stencil: {
        VKTL_ASSERT(first_declaration); // one subpass not allow multiple depth stencil.
        get<4u>(subpass_info) = reference;
      } break;
      default:
        VKTL_ASSERT(false);
      }
    }

    void append(VK_ VkSampleCountFlagBits samples) {
      VKTL_ASSERT(last_attachment != invalid);
      attachments_[last_attachment].samples = samples;
    }

    void append(VK_ VkFormat format) {
      VKTL_ASSERT(last_attachment != invalid);
      VKTL_ASSERT(format != VK_ VK_FORMAT_UNDEFINED);
      auto &current = attachments_[last_attachment].format;
      VKTL_ASSERT(current == VK_ VK_FORMAT_UNDEFINED || current == format);
      current = format;
    }

  private:
    // AGENT SPECIFICATION: Not sure mechisam, reserved at here.
    static constexpr bool generalize_reference(auto &subpass, uint16_t attachment, VK_ VkImageLayout layout) {
      auto found = false;
      auto generalize = [&](auto &references) {
        auto it = ::std::ranges::lower_bound(
            references, attachment, {}, &VK_ VkAttachmentReference::attachment);
        VKTL_ASSERT(it->layout != VK_ VK_IMAGE_LAYOUT_UNDEFINED); // test.
        if (it != references.end() && it->attachment == attachment && it->layout != layout) {
          it->layout = VK_ VK_IMAGE_LAYOUT_GENERAL;
          found = true;
        }
      };
      generalize(get<1u>(subpass));
      generalize(get<2u>(subpass));
      generalize(get<3u>(subpass));
      if (get<4u>(subpass).attachment == attachment && layout != get<4u>(subpass).layout) {
        get<4u>(subpass).layout = VK_ VK_IMAGE_LAYOUT_GENERAL;
      }
      return found;
    }

    static constexpr void insert_reference(vector<VK_ VkAttachmentReference> &references, VK_ VkAttachmentReference reference) {
      auto it = ::std::ranges::lower_bound(references, 
        reference.attachment, {}, &VK_ VkAttachmentReference::attachment);
      if (it == references.end() || it->attachment != reference.attachment) {
        references.insert(it, reference);
      } else if (it->layout != reference.layout) {
        it->layout = VK_ VK_IMAGE_LAYOUT_GENERAL;
      }
    }

    constexpr void append_dependency(VK_ VkSubpassDependency const &dependency) {
      auto src_begin = ::std::ranges::lower_bound(
        dependencies_, dependency.srcSubpass, {}, &VK_ VkSubpassDependency::srcSubpass);
      auto src_end = ::std::ranges::find_if(src_begin + 1u, dependencies_.end(),
        [subpass = dependency.srcSubpass](auto value) { return subpass != value; },
        &VK_ VkSubpassDependency::srcSubpass);
      auto found = ::std::ranges::lower_bound(
        src_begin, src_end, dependency.dstSubpass, {}, &VK_ VkSubpassDependency::dstSubpass);
      if (found == src_end || found->dstSubpass != dependency.dstSubpass) {
        dependencies_.insert(found, dependency);
        return;
      }
      found->srcStageMask |= dependency.srcStageMask;
      found->dstStageMask |= dependency.dstStageMask;
      found->srcAccessMask |= dependency.srcAccessMask;
      found->dstAccessMask |= dependency.dstAccessMask;
      found->dependencyFlags |= dependency.dependencyFlags;
    }

  protected:
    static constexpr uint16_t subpass = 0u;

  protected:
    VK_ VkRenderPassCreateInfo info{
        .sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    uint16_t last_attachment = invalid;

  private:
    vector<VK_ VkAttachmentDescription> attachments_;
    // not support preserve attachment.
    vectors<VK_ VkSubpassDescription,
      vector<VK_ VkAttachmentReference>, // input
      vector<VK_ VkAttachmentReference>, // color
      vector<VK_ VkAttachmentReference>, // resolve
      VK_ VkAttachmentReference> subpasses_;
    vector<VK_ VkSubpassDependency> dependencies_;

    copyable_if_null<VK_ VkRenderPass> handle_{VK_NULL_HANDLE};
  };

#if defined(VK_KHR_dynamic_rendering)
  template <> struct device_querion<pass_extensions::rendering_> {
    static bool query(VK_ VkPhysicalDevice device) noexcept {
      VK_ VkPhysicalDeviceDynamicRenderingFeaturesKHR fea{
          .sType = VK_
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
      };
      VK_ VkPhysicalDeviceFeatures2KHR features{
          .sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
          .pNext = &fea,
      };
      VK_ vkGetPhysicalDeviceFeatures2KHR(device, &features);
      return fea.dynamicRendering != 0u;
    }
  };

  template <typename N>
  struct m<pass_extensions::rendering_, N> : basic_graphics_pass<N> {
    using base = basic_graphics_pass<N>;

    constexpr m(pass_extensions::rendering_, auto &&...infos) 
      : base{forward_(infos)...} {
      // see https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_dynamic_rendering.html
      if constexpr (!have_parent_of<N, instance, version1_3>) {
        if constexpr (!have_parent_of<N, instance, version1_2>) {
          if constexpr (!have_parent_of<N, instance, version1_1>) {
            parent_of<instance>(this)->append_extensions(
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
          }
          parent_of<device>(this)
            ->append_extensions(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
        } 
        parent_of<device>(this)
          ->append_extensions(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
      }
    }


  protected:
    void finalize() {
      base::finalize();
      VKTL_ASSERT(renderings_.size() == base::pipe_count());
    }

    void relocate() noexcept {
      base::relocate();
      VKTL_ASSERT(renderings_.size() == base::pipe_count());
      auto index = 0u;
      for (auto &&[rendering, color_formats] : renderings_) {
        rendering.colorAttachmentCount = uint32_t(color_formats.size());
        rendering.pColorAttachmentFormats = data_or_null(color_formats);
        auto &pipeline = get<0u>(base::pipe_infos[index++]);
        pipeline.pNext = &rendering;
      }
    }

    auto init() {
      base::init(); 
      base::init_pipelines();
    }

    constexpr void append(VK_ VkGraphicsPipelineCreateInfo const& info) {
      base::append(info);
      renderings_.emplace_back(
          VK_ VkPipelineRenderingCreateInfoKHR{
              .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
          },
          by_default);
    }

    constexpr void append(attachment::info const &usage) {
      // base::append(usage.resource_usage());
      VKTL_ASSERT(!renderings_.empty());
      get<2u>(renderings_.back()).emplace_back(usage);
    }

    void append(VK_ VkFormat format) {
      VKTL_ASSERT(!renderings_.empty());
      auto const &usage = get<2u>(renderings_.back()).back();
      auto const index = renderings_.size() - 1u;
      auto &rendering = get<0u>(renderings_[index]);
      auto &color_formats = get<1u>(renderings_[index]);
      switch (usage.attributes & attachment_attribute::usage_mask) {
      case attachment_attribute::color:
        if (color_formats.size() <= usage.index) {
          color_formats.resize(usage.index + 1u, VK_ VK_FORMAT_UNDEFINED);
        }
        color_formats[usage.index] = format;
        break;
      case attachment_attribute::depth:
      case attachment_attribute::stencil:
      case attachment_attribute::depth_stencil:
        if ((usage.attributes & attachment::depth_operations) != 0u) {
          rendering.depthAttachmentFormat = format;
        }
        if ((usage.attributes & attachment::stencil_operations) != 0u) {
          rendering.stencilAttachmentFormat = format;
        }
        break;
      default:
        break;
      }
    }

  private:
    vectors<VK_ VkPipelineRenderingCreateInfoKHR, vector<VK_ VkFormat>,
            vector<attachment::info>>
        renderings_;
  };
#else
  template <typename N> struct m<pass_extensions::rendering_, N> : N {
    constexpr m(pass_extensions::rendering_, auto &&...infos)
        : N{forward_(infos)...} {
      static_assert(
          always_false<N>,
          "Dynamic rendering is not supported by the Vulkan headers.");
    }
  };
#endif

  template <typename N>
  struct m<pass_extensions::compute_, N> : basic_pipe_pass<N> {
    using base = basic_pipe_pass<N>;

    constexpr m(pass_extensions::compute_, auto &&...infos)
        : base{forward_(infos)...} {}

    ~m() { reset(); }

    void finalize() { base::finalize(); }

    void relocate() noexcept {
      base::relocate();
      for (auto &pipeline : pipe_infos.template column<0u>()) {
        pipeline.stage.module = VK_NULL_HANDLE;
      }
    }

    void init() {
      base::init();
      if (pipe_infos.empty() || pipes.front())
        return;
      auto hdv = handle_of<device>(this);
      try {
        for (auto &&[pipeline, codes] : pipe_infos) {
          VKTL_ASSERT(!codes.empty());
          auto module = VK_ VkShaderModuleCreateInfo{
              .sType = VK_ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
              .codeSize = codes.size() * sizeof(uint32_t),
              .pCode = codes.data(),
          };
          VK_ vkCreateShaderModule(hdv, &module, N::allocator(),
                                   &pipeline.stage.module) |
              popup{"[PASS] Failed to create compute shader module."};
          pipeline.layout = base::pipeline_layout();
        }
        auto result =
            VK_ vkCreateComputePipelines(
                hdv, base::pipeline_cache(), uint32_t(pipe_infos.size()),
                pipe_infos.template data<0u>(), N::allocator(), pipes.data()) |
            popup {
              "[PASS] Failed to create compute pipelines."
            };
        destory_shaders(hdv);
      } catch (...) {
        reset();
        throw;
      }
    }

    void reset() {
      auto hdv = handle_of<device>(this);
      for (auto &pipe : pipes) {
        if (pipe) {
          VK_ vkDestroyPipeline(
              hdv, ::std::exchange(pipe.value, VK_NULL_HANDLE), N::allocator());
        }
      }
      destory_shaders(hdv);
      base::reset();
    }

    VK_ VkPipeline pipe(uint16_t index) const noexcept {
      VKTL_ASSERT(index < pipes.size());
      return pipes[index].value;
    }

    uint16_t pipe_count() const noexcept { return uint16_t(pipe_infos.size()); }

  protected:
    void append(VK_ VkComputePipelineCreateInfo info) {
      VKTL_ASSERT(pipe_infos.size() < uint16_t(maximum));
      pipe_infos.emplace_back(info);
      pipes.emplace_back(VK_NULL_HANDLE);
    }

  private:
    void destory_shaders(VK_ VkDevice hdv) {
      for (auto &pipeline : pipe_infos.template column<0u>()) {
        VK_ vkDestroyShaderModule(
            hdv, ::std::exchange(pipeline.stage.module, VK_NULL_HANDLE),
            N::allocator());
      }
    }

  protected:
    vectors<VK_ VkComputePipelineCreateInfo, vector<uint32_t>> pipe_infos;
    vector<VK_ VkPipeline> pipes;
  };

  template <inside_object<render_pass_> N> struct m<split_subpass_, N> : N {
    m(split_subpass_, auto &&...others) : N{forward_(others)...} {}

  protected:
    static constexpr uint16_t subpass = uint16_t(N::subpass + 1u);
  };

  template <typename N> struct basic_pipe : N {
    using base = N;
    constexpr basic_pipe(auto &&...others) : base{forward_(others)...} {
      if constexpr (requires(VK_ VkGraphicsPipelineCreateInfo const &v) { base::append(v); }) {
        VK_ VkGraphicsPipelineCreateInfo info{ .sType = VK_ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, };
        if constexpr (requires { base::subpass; }) {
          info.subpass = base::subpass;
        }
        base::append(info);
      } else if constexpr (requires(VK_ VkComputePipelineCreateInfo const &v) { base::append(v); }) {
        base::append(VK_ VkComputePipelineCreateInfo{
          .sType = VK_ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
          .stage = { .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, },
        });
      } else {
#if defined(VK_KHR_pipeline_binary)
        base::append(VK_ VkPipelineCreateInfoKHR{
          .sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_CREATE_INFO_KHR,
        });
#else
        static_assert(always_false<base>, "A pipeline requires a graphics or compute pass.");
#endif
      }
    }

  protected:
    static constexpr uint32_t pipe_index = []() constexpr {
      if constexpr (requires { N::pipe_index; }) {
        return N::pipe_index + 1u;
      } else {
        return 0u;
      }
    }();
  };

  template <object_of<graphics_pass> N> struct m<pipe_, N> : basic_pipe<N> {
    using base = basic_pipe<N>;
    constexpr m(pipe_, auto &&...others) : base{forward_(others)...} {}


  protected:
    void relocate() noexcept {
      base::relocate();
      blend_.attachmentCount = uint32_t(attachment_states_.size());
      blend_.pAttachments = attachment_states_.data();
    }
    constexpr void append(attachment::info info) {
      base::append(::std::in_place, info);
    }

    auto& blend_state() noexcept { return blend_; }

  private:
    VK_ VkPipelineColorBlendStateCreateInfo blend_ = defaultColorBlendAttachment;
    vector<VK_ VkPipelineColorBlendAttachmentState> attachment_states_;
  };

  template <object_of<compute_pass> N> struct m<pipe_, N> : basic_pipe<N> {
    using base = basic_pipe<N>;
    constexpr m(pipe_, auto &&...others) : base{forward_(others)...} {}
  };

  namespace resource_usage {
  template <bool graphics>
    inline constexpr resource_declaration fill_stages(resource_declaration usage,
                                                      auto &infos) {
      VKTL_ASSERT(!infos.empty());
      auto &pipeline = get<0u>(infos.back());
      if constexpr (graphics) {
        auto &shaders = get<1u>(infos.back());
        VKTL_ASSERT(!shaders.empty());
        VKTL_ASSERT(pipeline.subpass < uint32_t(invalid));
        usage.access.stages = pipeline_stage_of(get<0u>(shaders.back()).stage);
        usage.subpass = uint16_t(pipeline.subpass);
      } else {
        usage.access.stages = pipeline_stage_of(pipeline.stage.stage);
      }
      return usage;
    }
  } // namespace resource_usage

  template <> struct express<customized_entry_point> {
    static constexpr void invoke(customized_entry_point entry, auto &base) {
      static_assert(requires { base.pipe_index; }, "Must append any shader before `customized_entry_point`.");
      VKTL_ASSERT(entry.name);
      if constexpr (requires { base.render_pass; }) {
        auto &shaders = get<1u>(base.pipe_infos.back());
        VKTL_ASSERT(!shaders.empty());
        auto &stage = get<0u>(shaders.back());
        stage.pName = entry.name;
      } else {
        auto &pipeline = get<0u>(base.pipe_infos.back());
        pipeline.stage.pName = entry.name;
      }
    }
  };

  template <> struct express<uniform_buffer> {
    template <typename Base>
    static constexpr void invoke(uniform_buffer usage, Base &base) {
      base.append(resource_usage::fill_stages<object_of<Base, graphics_pass>>(
          resource_declaration{
              .index = usage.index,
              .resource_type = VK_ VK_OBJECT_TYPE_BUFFER,
              .type = VK_ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .access = VK_ VK_ACCESS_UNIFORM_READ_BIT,
              .usages = VK_ VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
          },
          base.pipe_infos));
    }
  };

  template <> struct express<sampled_image> {
    template <typename Base>
    static constexpr void invoke(sampled_image usage, Base &base) {
      base.append(resource_usage::fill_stages<object_of<Base, graphics_pass>>(
          resource_declaration{
              .index = usage.index,
              .related_index = usage.sampler_index,
              .resource_type = VK_ VK_OBJECT_TYPE_IMAGE,
              .type = usage.sampler_index == invalid
                          ? VK_ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                          : VK_ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .access = VK_ VK_ACCESS_SHADER_READ_BIT,
              .usages = VK_ VK_IMAGE_USAGE_SAMPLED_BIT,
          },
          base.pipe_infos));
    }
  };

  template <> struct express<standalone_sampler> {
    static constexpr void invoke(standalone_sampler usage, auto &base) {
      base.append(resource_usage::fill_stages(
          resource_declaration{
              .index = usage.index,
              .related_index = usage.index,
              .resource_type = VK_ VK_OBJECT_TYPE_SAMPLER,
              .type = VK_ VK_DESCRIPTOR_TYPE_SAMPLER,
          },
          base.pipe_infos));
    }
  };

  namespace attachment {
  inline constexpr void assert_usage(auto usage) {
    VKTL_ASSERT(!(has(usage.attribute, attachment::load) &&
                  has(usage.attribute, attachment::clear)));
  }
  } // namespace attachment

  template <> struct express<input_attachment> {
    template <typename B>
    static constexpr void invoke(input_attachment usage, B &base) {
      VKTL_ASSERT(!base.pipe_infos.empty());
      base.append(
          B::subpass, B::pipe_index,
          attachment::info{
              .index = attachment::index(base, usage.index),
              .attributes = uint16_t(usage.attribute | attachment::load |
                                     attachment::input),
          });
    }
  };

  template <> struct express<color_attachment> {
    template <object_of<graphics_pass> B>
    static constexpr void invoke(color_attachment usage, B &base) {
      static_assert(
          requires { B::pipe_index; },
          "must append pipe before any attachment.");
      attachment::assert_usage(usage);
      base.append(
          B::subpass, B::pipe_index,
          attachment::info{
              .index = attachment::index(base, usage.index),
              .attributes = uint16_t(usage.attribute | attachment::color),
          });
    }
  };

  template <> struct express<resolve_attachment> {
    template <object_of<graphics_pass> B>
    static constexpr void invoke(resolve_attachment usage, B &base) {
      static_assert(
          requires { B::pipe_index; },
          "must append pipe before any attachment.");
      attachment::assert_usage(usage);
      base.append(
          B::subpass, B::pipe_index,
          attachment::info{
              .index = attachment::index(base, usage.index),
              .attributes = uint16_t(usage.attribute | attachment::resolve),
          });
    }
  };

  template <> struct express<depth_stencil_attachment> {
    template <object_of<graphics_pass> B>
    static constexpr void invoke(depth_stencil_attachment usage, B &base) {
      static_assert(
          requires { B::pipe_index; },
          "must append pipe before any attachment.");
      attachment::assert_usage(usage);
      base.append(B::subpass, B::pipe_index,
                  attachment::info{
                      .index = attachment::index(base, usage.index),
                      .attributes = uint16_t(usage.attribute),
                  });
    }
  };

  using namespace resource_usage_extensions;

  template <> struct express<with_count> {
    static void invoke(with_count value, auto &object) {
      VKTL_ASSERT(value.count > 0u);
      object.usages.back().count = uint16_t(value.count);
    }
  };

  template <> struct express<bind_on_set> {
    static void invoke(bind_on_set value, auto &object) {
      auto &usage = object.usgaes.back();
      VKTL_ASSERT(usage.uses_descriptor());
      VKTL_ASSERT(usage.set == invalid || usage.set == value.set);
      VKTL_ASSERT(usage.binding == invalid || usage.binding == value.binding);
      usage.set = value.set;
      usage.binding = value.binding;
    }
  };

  template <> struct express<bind_on_heap> {
    static void invoke(bind_on_heap value, auto &object) {
      auto &usage = object.usgaes.back();
      VKTL_ASSERT(usage.offset == invalid || usage.offset == value.offset);
      usage.offset = value.offset;
    }
  };
}
