#pragma once

// --- Agents specification -------------------------------------------------
// Sampler bindings use valid set/binding members for descriptor sets; an
// additionally valid offset denotes a future descriptor-buffer binding, while
// offset alone denotes a future descriptor-heap binding. Binding a sampler to
// an explicitly named set/binding converts an existing dynamic bind there into
// an immutable sampler bind. Descriptor-buffer and heap writes are not yet
// implemented. A sampler point's dynamic and immutable binding lists stay
// unique and ordered by set first, then binding. Image and sampler components
// fill a shared VkDescriptorImageInfo through bind::image_info(); they append
// writes before forwarding so bind_set_ submits vkUpdateDescriptorSets once.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::vptr {
  template <typename Trait>
  struct view {
    using view_type = typename Trait::view_type;
    using host_trait = detail::trait<typename Trait::host>;
    using host_handle_type = typename host_trait::handle_type;
    using usage_flags_type = typename host_trait::usage_flags_type;

    template <typename C>
    using base = apply_compose<C, frame_counted, initable, lockable>;

    template <typename T>
    constexpr void rebind() noexcept {
      handle_ = [](void const* ptr, uint32_t frame) -> host_handle_type {
        return static_cast<T const*>(ptr)->host(frame);
      };
      view_ = [](void const* ptr, uint32_t frame) -> view_type {
        return static_cast<T const*>(ptr)->handle(frame);
      };
      usage_ = [](void* ptr, usage_flags_type flags) {
        static_cast<T*>(ptr)->usage(flags);
      };
    }

    template <typename C>
    struct apply : base<C> {
      using base = base<C>;

      template <typename T>
      constexpr void rebind() noexcept {
        vptr.template rebind<T>();
      }

      view_type view(uint32_t frame) const {
        return vptr.view_(C::get_this(), frame);
      }
      host_handle_type handle(uint32_t frame) const {
        return vptr.handle_(C::get_this(), frame);
      }
      void usage(usage_flags_type flags) noexcept {
        vptr.usage_(C::get_this(), flags);
      }

      struct view vptr;
    };

    vfn<void(usage_flags_type)> usage_;
    vfn<host_handle_type(uint32_t) const> handle_;
    vfn<view_type(uint32_t) const> view_;
  };

  template <typename Trait>
  struct view_desc {
    using layout_type = typename Trait::layout;
    using subresource_range = typename Trait::subresource_range;

    template <typename C>
    using base = apply_compose<C, view<Trait>>;

    template <typename T>
    constexpr void rebind() noexcept {
      range_ = [](void const* ptr) noexcept -> subresource_range {
        return static_cast<T const*>(ptr)->subresource_range();
      };
      if constexpr (!::std::is_void_v<layout_type>) {
        layout_ = [](void const* ptr) noexcept -> layout_type {
          return static_cast<T const*>(ptr)->layout();
        };
      }
    }

    template <typename C>
    struct apply : base<C> {
      using base = base<C>;

      template <typename T>
      constexpr void rebind() noexcept {
        vptr.template rebind<T>();
      }

      layout_type layout() const noexcept {
        return vptr.layout_(C::get_this());
      }
      subresource_range range() const noexcept {
        return vptr.range_(C::get_this());
      }

      view_desc vptr;
    };

    vfn<layout_type() const noexcept> layout_;
    vfn<subresource_range() const noexcept> range_;
  };

  struct sampler {
    template <typename C>
    using base = apply_compose<C, handle_owner<VK_ VkSampler>, initable>;

    template <typename T>
    constexpr void rebind() noexcept {}
    template <typename C>
    struct apply;
  };
  template <typename C>
  struct sampler::apply : base<C> {
    using base = base<C>;

    VKTL_NO_UNIQUE_ADDRESS sampler vptr;
  };
}

VKTL_EXPORT_ namespace vktl::detail {
  using default_bind_set_schema = vector<default_descriptor_set_layout>;

  // struct resource_pass_binding : resource_binding {
  // 	uint64_t pass_id;
  // };

  template <typename Trait>
  struct default_bind_point {
    box<vptr::view_desc<Trait>> view;
    uint64_t usages;
    VK_ VkFormat format = VK_ VK_FORMAT_UNDEFINED;
    vector<resource_binding> bindings;
  };

  namespace bind {
  //using namespace bind_set_extensions;

    struct top {
      vector<::std::mutex*> locks;
      // split by descriptor type.
      vector<vectors<VK_ VkWriteDescriptorSet, uint16_t>> write; // [[value, index of childs' write (eg. `buffers` or `images`)]]
      vector<vector<VK_ VkCopyDescriptorSet>> copy; // reserved.
    };
    
    struct write_buffer {
      vector<vector<VK_ VkDescriptorBufferInfo>> buffers;
      vector<vector<VK_ VkBufferView>> texel_views;
    };
    
    struct write_image {
      vector<vector<VK_ VkDescriptorImageInfo>> images;
    };

    inline constexpr size_t to_index(VK_ VkDescriptorType type);

    inline VK_ VkDescriptorImageInfo& image_info(auto& state, 
      VK_ VkDescriptorSet set, resource_binding binding) {
      VKTL_ASSERT(set && binding.binding != invalid); // internal error.
      top& host = get<top>(state);
      write_image& infos = get<write_image>(state);
      auto const index = to_index(binding.type);
      if (host.write.size() <= index) { host.write.resize(index + 1u); }
      auto& typed = host.write[index];
      auto writes = typed.template column<0u>();
      auto found = ::std::ranges::find_if(
        writes, [&](VK_ VkWriteDescriptorSet const& write) {
          return write.dstSet == set &&
                 write.dstBinding <= binding.binding &&
                 binding.binding < write.dstBinding + write.descriptorCount;
        });
      if (found != writes.end()) {
        auto it = typed.begin() + ::std::distance(writes.begin(), found);
        auto& images = infos.images[it.template get<1u>()];
        auto const element = binding.binding - found->dstBinding;
        VKTL_ASSERT(element < images.size());
        return images[element];
      }

      VKTL_ASSERT(infos.images.size() < invalid);
      auto& images = infos.images.emplace_back(VK_ VkDescriptorImageInfo{});
      typed.emplace_back(VK_ VkWriteDescriptorSet{
        .sType = VK_ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = binding.binding,
        .dstArrayElement = 0u,
        .descriptorCount = 1u,
        .descriptorType = binding.type,
        .pImageInfo = images.data(),
      }, uint16_t(infos.images.size() - 1u));
      return images.back();
    }
    
    inline auto get_state(trait<buffer_view>) { return write_buffer{}; }
    inline auto get_state(trait<image_view>) { return write_image{}; }
    
    // decrease the dimension of bind descriptor.
    inline constexpr size_t to_index(VK_ VkDescriptorType type) {
      switch (type) {
        case VK_ VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        case VK_ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        case VK_ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
          return size_t(type);
        // TODO: #if defined;
        case VK_ VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
          return size_t(11);
        case VK_ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
          return size_t(12);
        case VK_ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
          return size_t(13);
        case VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE:
          return size_t(14);
        case VK_ VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM:
          return size_t(15);
        case VK_ VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM:
          return size_t(16);
        case VK_ VK_DESCRIPTOR_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV:
          return size_t(17);
        case VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM:
        default:
          VKTL_ASSERT(false);
      }
    }
    
    inline constexpr auto insert(trait<buffer_view>, 
      auto& write, auto frame_index, auto& state,
      resource_binding binding, auto& view) {
      write_buffer& infos = get<write_buffer>(state);
      if (binding.type == VK_ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER 
       || binding.type == VK_ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
        // size can from different type.
        get<1u>(write) = uint16_t(infos.texel_views.size());
        get<0u>(write).pTexelBufferView =
            infos.texel_views.emplace_back(view.view()).data();
      } else {
        auto range = view.range();
        get<1u>(write) = uint16_t(infos.buffers.size());
        get<0u>(write).pBufferInfo =
          infos.buffers.emplace_back(VK_ VkDescriptorBufferInfo{
            .buffer = view.host(),
            .offset = range.offset,
            .range = range.size,
          }).data();
      }
    }
    inline constexpr auto insert(trait<image_view>, 
      auto& write, auto frame_index, auto& state,
      resource_binding binding, auto& point) {
      write_image& infos = get<write_image>(state);
      get<1u>(write) = uint16_t(infos.images.size());
      get<0u>(write).pImageInfo =
        infos.images.emplace_back(VK_ VkDescriptorImageInfo{
          .imageView = point.view.view(),
          .layout = point.view.layout(),
        }).data();
    }
    
    inline auto lower_bound(auto &bindings, resource_binding value) {
      return ::std::ranges::lower_bound(bindings, value, 
        [](resource_binding left, resource_binding right) noexcept {
          return left.set < right.set ||
              (left.set == right.set && left.binding < right.binding);
        });
    }

    inline void insert_or_assign(auto& bindings, resource_binding value) {
      auto it = lower_bound(bindings, value); 
      // TODO: buffer or heap.
      if (it != bindings.end() 
       && it->set == value.set && it->binding == value.binding) {
        *it = value;
      } else {
        bindings.insert(it, value);
      }
    }

    inline constexpr bool uses_sampler(resource_declaration const& decl) noexcept {
      return decl.binding.type == VK_ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER 
        || decl.binding.type == VK_ VK_DESCRIPTOR_TYPE_SAMPLER;
    }


    inline constexpr void merge(
      trait<buffer_view>, auto& write, auto frame_index,
      auto& state, resource_binding binding, auto& view) {
      write_buffer &buffer = get<write_buffer>(state);
      if (binding.type == VK_ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER 
       || binding.type == VK_ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {

      } else {

      }
    }
    inline constexpr void merge(
      trait<image_view>, auto& write, auto frame_index,
      auto& state, resource_binding binding, auto& view) {
      write_image &image = get<write_image>(state);

    }

  }  // namespace bind

  template <typename N>
  struct m<bind_set_, N> : basic_frame_indexed<N> {
    using base = basic_frame_indexed<N>;

    constexpr m(bind_set_, auto&&... others) : base{forward_(others)...} {}

    m(m const&) = delete;
    m& operator=(m const&) = delete;

    m(m&&) noexcept = default; // enabled.

    m& operator=(m&&) = delete; // not allow move assign, since lazy.

    auto layout_infos() const noexcept { return schema_.template column<0u>(); }
    auto layouts() { 
      // should try?
      for (auto &[info, layout] : this->schema_) {
        if (!layout) { layout = parent_of<device>(this)->create(info); }
      }
      return schema_.template column<1u>(); 
    }

    vector<VK_ VkDescriptorSetLayout> layouts(passes::id pass_id) const {
      auto it = pass_to_layouts_.find(pass_id);
      VKTL_ASSERT(it != pass_to_layouts_.end());
      vector<VK_ VkDescriptorSetLayout> result;
      result.reserve(it->second.size());
      for (auto index : it->second) {
        VKTL_ASSERT(index < schema_.size());
        result.emplace_back(get<1u>(schema_[index]));
      }
      return result;
    }

   protected:
    void init() {
      N::init();
      auto dv = parent_of<device>(this);
      for (auto&& [info, layout] : schema_) {
        if (layout == VK_NULL_HANDLE) {
          layout = dv->create(info);
        }
      }
    }

    void reset() {
      N::reset();
      release_descriptors();
      for (auto frame = 0u; frame < this->frame_count(); ++frame) {
        auto handle = base::handle(frame);
      }
    }

    void adopt(::std::in_place_t, auto &pass, span<uint16_t> affine) {
      auto id = pass.id();
      auto it = pass_to_layouts_.find(id);
      if (it != pass_to_layouts_.end()) {
        // TODO: might assign.
        VKTL_ASSERT(
            !"Not allow append one pass in one bind_set for multiple time.");
      } else {
        pass_to_layouts_.emplace(id, vector{affine.begin(), affine.end()});
      }
    }

    // if usage is valid and use descriptor. append it to set layout.
    void bind(::std::in_place_t, resource_declaration const& usage, span<uint16_t> affine) {
      if (!usage.uses_descriptor()) { return; }
      VKTL_ASSERT(usage.binding.set < affine.size()); // affine mismatch.
      auto const set = affine[usage.binding.set];
      VKTL_ASSERT(set != invalid); // not allow invalid here.
      if (schema_.size() <= set) schema_.resize(size_t(set) + 1u);
      default_descriptor_set_layout& layout = schema_.get<0u>(set);
#if defined(VK_EXT_descriptor_indexing)
      auto const flags = usage.binding_flags();
#endif
      auto const scope = layout.add(
          VK_ VkDescriptorSetLayoutBinding{
              .binding = usage.binding.binding,
              .descriptorType = usage.binding.type,
              .descriptorCount = usage.count,
              .stageFlags = to_shader_stages(usage.access.stages),
          },
          {}
#if defined(VK_EXT_descriptor_indexing)
          , flags
#endif
      );
      VKTL_ASSERT(
        (inside_object<N, bind_set_extensions::allow_mutable_> 
          || !scope.is_mutable));
    }
    // for check.
    void bind(uint32_t, auto&) {
      VKTL_ASSERT(!"bind_set does not allow this kind of resource view.");
    }
    // for check.
    void bind(resource_declaration const&, span<uint16_t> = {}) {
      VKTL_ASSERT(!"bind set does not allow this kind of resource.");
    }

    // 
    // bind descriptor handle.
    //

    static auto get_state() noexcept {
      return::std::tuple(bind::top{});
    }

    // when allocated.
    void bind(auto& state, bind_descriptors& bind) {
      release_descriptors();
      for (auto frame = 0u; frame < this->frame_count(); ++frame) {
        auto handle = base::handle(frame);
        handle.value.sets.clear();
      }
      if (!bind.handle) return;

      auto allocated = bind.sets();
      auto const set_count = schema_.size();
      VKTL_ASSERT(allocated.size() == size_t(this->frame_count()) * set_count);
      auto& top = ::std::get<bind::top>(state);
      auto& writes = top.write;
      auto& copy = top.copy;
      if (!writes.empty() || !copy.empty()) {
        VK_ vkUpdateDescriptorSets(
          handle_of<device>(this),
          uint32_t(writes.size()), writes.data(),
          uint32_t(copy.size()), copy.data());
      }
      bind_.value = ::std::move(bind);
    }

    uint32_t set_count() const noexcept { return schema_.size(); }

    vector<VK_ VkDescriptorSetLayout> layouts(uint64_t pass_id) {
      auto it = pass_to_layouts_.find(pass_id);
      if (it == pass_to_layouts_.end()) { return {}; }
      vector<VK_ VkDescriptorSetLayout> result; result.resize(it->second.size());
      ::std::ranges::transform(it->second, result.begin(), [&](auto value) {
        return this->schema_.template get<1u>(value);
      });
      return result;
    }

   private:
    void release_descriptors() {
       if (bind_.handle) {
         ::std::exchange(bind_, {}).value.free();
       }
    }

   protected:
    mumap<uint64_t, uint16_t> pass_to_layouts_;
    vectors<default_descriptor_set_layout, VK_ VkDescriptorSetLayout> schema_;

   private:
    move_only<bind_descriptors> bind_{};
  };

  template <typename N, typename ViewTrait>
  struct basic_allow_bind_resource : N {
    using resource_trait = trait<typename ViewTrait::host>;
    using point_type = default_bind_point<ViewTrait>;

    constexpr basic_allow_bind_resource(auto&&... others)
        : N{forward_(others)...} {}

    void adopt(object_of<pass_> auto& pass) requires(!object_of<pass_need_descriptor>) {
      auto _ = locker_of(this);
      bind(pass.resource_usages());
    }
    void adopt(object_of<pass_need_descriptor> auto& pass, span<uint16_t> affine) {
      auto _ = locker_of(this);
      VKTL_ASSERT(::std::ranges::all_of(pass.usages(), [&](auto const& value) {
        return !value.uses_descriptor() 
          || (value.binding.set < affine.size() && affine[value.binding.set] != invalid);
      })); // not valid affine.
      bind(pass.resource_usages(), affine);
      pass.bind_set(N::as_self());
      N::adopt(::std::in_place, pass, affine);
    }

    void bind(uint32_t index, object_of<typename ViewTrait::type> auto& view) {
      VKTL_ASSERT(index < points_.size());
      auto& point = points_[index];
      view.usage(view.usage() | point.usages);
      point.view = view;
    }

    void bind(bind_descriptors bind) {
      auto state = get_state();
      this->bind(state, bind);
    }

   protected:
    auto get_state() {
      return::std::tuple_cat(N::get_state(),
        ::std::tuple(bind::get_state(ViewTrait{})));
    }

   private:
    void pass_bind(::std::span<resource_declaration const> usages, span<uint16_t> affine = {}) {
      for (auto const& usage : usages) {
        pass_bind(usage, affine);
      }
    }
    void pass_bind(resource_declaration const &usage, span<uint16_t> affine) {
      if (usage.resource_type != resource_trait::object_type) {
        N::bind(usage, affine);
      } else {
        bind(::std::in_place, usage, affine);
      }
    }

   protected:
    // if bottom component need express something, then will enter this batch.
    void bind(::std::in_place_t, resource_declaration const& usage, span<uint16_t> affine) {
      if (usage.resource_type == resource_trait::object_type) {
        bind_resource(usage, affine);
      }
      N::bind(::std::in_place, usage, affine);
    }

   private:
    // no transfer version.
    void bind_resource(resource_declaration const& usage, span<uint16_t> affine) {
      VKTL_ASSERT(usage.index != invalid); // internal error.
      auto set = usage.binding.set;
      if (usage.uses_descriptor() && affine.size()) {
        set = affine[set];
      }
      const auto count = usage.count;
      VKTL_ASSERT(count); // internal error.
      if (points_.size() < usage.index + count) {
        points_.resize(usage.index + count);
      }
      uint32_t offset = 0u;
      for (auto& point : span{points_}.subspan(usage.index, usage.count)) {
        point.usages |= usage.usages;
        if (point.view) {
          point.view.usage(point.usages);
        }
        if (usage.uses_descriptor()) {
          auto copy = usage.binding;
          copy.set = set;
          copy.binding = subres.add(copy.binding, offset);
          copy.offset = subres.add(copy.offset, offset);
          point.bindings.emplace_back(copy);
        }
        ++offset;
      }
    }

   protected:
    void bind(auto& state, bind_descriptors& descriptors) {
      auto const set_count = this->schema_.size();
      auto const frame_count = this->frame_count();
      auto allocated = descriptors.sets();
      VKTL_ASSERT(allocated.size() == size_t(frame_count) * set_count); // internal error.

      bind::top& host = get<bind::top>(state);
      for (point_type& point : points_) try {
        if (point.view.empty()) { continue; }
        point.view.init();
        auto lock = point.view.get_lock();
        auto it = ::std::ranges::find(host.locks, lock);
        if (lock && it == host.locks.end()) {
          lock->lock();
          host.locks.emplace_back(lock);
        }

        for (auto const& binding : point.bindings) {
          if (binding.set == invalid) {
            VKTL_ASSERT(!"Not implment descriptor heap yet.");
          } else {
            if constexpr (::std::same_as<ViewTrait, trait<image_view>>) {
              for (auto frame = 0u; frame < frame_count; ++frame) {
                auto& info = bind::image_info(
                  state, allocated[frame * set_count + binding.set], binding);
                info.imageView = point.view.view(frame);
                info.imageLayout = point.view.layout();
              }
              continue;
            }

            // TODO: merge write descriptor? 
            span sets = descriptors.sets();
            auto set = sets[binding.set];
            auto index = bind::to_index(binding.type);
            auto& write = host.write;
            if (write.size() <= index) {
              write.resize(index + 1u);
            }
            auto& write_type = write[index];

            auto write_set = write_type.template column<0u>();
            auto its = ::std::ranges::find_if(write_set, 
              [&](VK_ VkWriteDescriptorSet const& value) {
                  return value.dstSet == set;
              });
            auto is_upper_bound = [&](VK_ VkWriteDescriptorSet const& value) {
              return value.dstSet != set &&
                     binding.binding + 1u < value.dstBinding;
            };
            if (its != write_set.end()) {
              auto ite = its + 1;
              if (ite != write_set.end() && !is_upper_bound(*ite)) {
                ite = ::std::ranges::find_if(ite, write_set.end(), is_upper_bound);
              }
              its = ::std::ranges::find_if(
                  its, ite, [&](VK_ VkWriteDescriptorSet const& value) {
                    VKTL_ASSERT(!subres.intersected(
                      value.dstBinding, value.descriptorCount,
                      binding.binding, 1u));  // Not allow intersected.
                    return subres.adjacent(
                      value.dstBinding,
                      value.descriptorCount,
                      binding.binding, 1u);
                  });
            }

            auto it =
                write_type.begin() + ::std::distance(write_set.begin(), its);
            if (it == write_type.end() ||
                is_upper_bound(it.template get<0u>())) {
              for (auto frame_index = 0u; frame_index < frame_count; frame_index++) {
                // `!frame_index` to allow [frame[0], frame[1], ..., upper bound].
                it = write_type.insert(it + !frame_index, 
                  VK_ VkWriteDescriptorSet{
                  .sType = VK_ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                  .dstSet = sets[frame_index * set_count + binding.set],
                  .dstBinding = binding.binding,
                  .dstArrayElement = 0u,
                  .descriptorCount = 1u,
                  .descriptorType = binding.type,
                 }, 0u);
                bind::insert(resource_trait(), 
                  *it, frame_index, state, binding, point.view);
              }
            } else {  // adjacent, merge.
              for (auto frame_index = 0u; frame_index < frame_count; frame_index++) {
                auto &value = *(it + frame_index);
                bind::merge(resource_trait(), 
                  value, frame_index, state, binding, point.view);
              }
            }
          }
        }
      } catch (...) {
        for (auto lock : host.locks) if (lock) {
          lock->unlock();
        }
        throw;
      }

      N::bind(state, descriptors);
    }

   protected:
    vector<point_type> points_;
  };

  using namespace bind_set_extensions;

  template <typename N>
  struct m<bind_set_extensions::allow_buffer_, N>
      : basic_allow_bind_resource<N, trait<buffer_view>> {
    using base = basic_allow_bind_resource<N, trait<buffer_view>>;
    using point_type = typename base::point_type;

    constexpr m(bind_set_extensions::allow_buffer_, auto&&... others)
        : base{forward_(others)...} {}
  };

  template <typename N>
  struct m<bind_set_extensions::allow_image_, N>
      : basic_allow_bind_resource<N, trait<image_view>> {
    using base = basic_allow_bind_resource<N, trait<image_view>>;
    constexpr m(bind_set_extensions::allow_image_, auto&&... others)
        : base{forward_(others)...} {}

    void bind(bind_descriptors binds) {
      base::bind(::std::move(binds));
    }
  protected:
    void bind(::std::in_place_t, resource_declaration const& decl, span<uint16_t> affine) {
      base::bind(::std::in_place, decl, affine);
    }
  };

  struct default_sampler_bind_point {
    box<vptr::sampler> view;
    vector<resource_binding> dynamic_binds;
    vector<resource_binding> static_binds;
  };

  template <typename N>
  struct m<allow_sampler_, N> : N {
    using base = N;
    constexpr m(allow_sampler_, auto&&... others) : base{forward_(others)...} {}

    // bind dynamic.
    void bind(uint32_t index, object_of<sampler> auto& sampler) {
      if (samplers_.size() <= index) {
        samplers_.resize(index + 1u);
      }
      samplers_[index].view = {sampler};
    }

    // bind static.
    void bind(uint32_t index, object_of<sampler> auto& sampler, uint32_t set, uint32_t binding) {
      VKTL_ASSERT(set != invalid && binding != invalid);
      bind(index, sampler);

      auto& point = samplers_[index];
      resource_binding target{
        .set = uint16_t(set),
        .binding = uint16_t(binding),
      };
      auto dynamic = bind::lower_bound(point.dynamic_binds, target);
      if (dynamic == point.dynamic_binds.end() 
       || dynamic->set == set && dynamic->binding == binding) {
        target = *dynamic;
        point.dynamic_binds.erase(dynamic);
      }
      bind::insert_or_assign(point.static_binds, target);
    }

    void adopt(object_of<pass_need_descriptor> auto& pass, span<uint16_t> affine) {
      auto _ = locker_of(this);
      for (auto usage : pass.usages()) {
        bind(usage, affine);
      }
      N::adopt(::std::in_place, pass, affine);
      
    }

    void bind(bind_descriptors binds) { 
      auto _ = locker_of(this);
      auto state = get_state();
      bind(state, binds);
    }

    auto layout_infos() {
      for (default_sampler_bind_point &sampler : samplers_) {
        if (sampler.static_binds.size()) {
          sampler.view.init();
          for (auto const &bind : sampler.static_binds) {
            if (bind.set != invalid) {
              VKTL_ASSERT(bind.binding != invalid); // internal error.
              this->schema_.template get<0u>(bind.set)
                .add(sampler.view.handle(), bind.binding);
            }
          }
        }
      }
      return N::layout_infos();
    }

   protected:
    auto get_state() {
      return N::get_state();
    }

    void bind(resource_declaration const &decl, span<uint16_t> affine) { 
      if (bind::uses_sampler(decl)) {
        bind_sampler(decl, affine);
        N::bind(::std::in_place, decl, affine);
      } else {
        N::bind(decl, affine);
      }
    }

    void bind(::std::in_place_t, resource_declaration const& decl, span<uint16_t> affine) {
      if (bind::uses_sampler(decl)) {
        bind_sampler(decl, affine);
      }
      N::bind(::std::in_place, decl, affine);
    }

    void bind(auto& state, bind_descriptors descriptor) {
      VKTL_ASSERT(descriptor.handle); // internal error. 
      span<VK_ VkDescriptorSet> sets = descriptor.sets();
      auto const set_count = this->schema_.size();
      auto const frame_count = this->frame_count();
      VKTL_ASSERT(sets.size() == set_count * frame_count); // internal error.
      for (auto& point : samplers_) {
        if (point.view.empty() || point.dynamic_binds.empty()) { continue; }
        point.view.init();
        auto const sampler = point.view.handle();

        for (auto const& binding : point.dynamic_binds) {
          VKTL_ASSERT(binding.set != invalid 
            && binding.binding != invalid && binding.offset == invalid); // set path only for now.
          for (auto frame = 0u; frame < frame_count; ++frame) {
            bind::image_info(
              state, sets[frame * set_count + binding.set], binding).sampler = sampler;
          }
        }
      }

      N::bind(state, descriptor);
    }

   private:
    void bind_sampler(resource_declaration const& decl, span<uint16_t> affine) {
      VKTL_ASSERT(decl.related_index != invalid && decl.count); // internal error.
      VKTL_ASSERT(decl.binding.set != invalid &&
                  decl.binding.binding != invalid &&
                  decl.binding.offset == invalid); // set path only for now.
      VKTL_ASSERT(decl.binding.set < affine.size());

      auto binding = decl.binding;
      binding.set = affine[binding.set];
      VKTL_ASSERT(binding.set != invalid);
      if (samplers_.size() < size_t(decl.related_index) + decl.count) {
        samplers_.resize(size_t(decl.related_index) + decl.count);
      }

      uint32_t offset = 0u;
      for (auto& point : span{samplers_}.subspan(decl.related_index, decl.count)) {
        auto copy = binding;
        copy.binding = subres.add(copy.binding, offset++);
        auto immutable = bind::lower_bound(point.static_binds, copy);
        if (same_binding(immutable, point.static_binds.end(), copy)) {
          *immutable = copy;
        } else {
          bind::insert_or_assign(point.dynamic_binds, copy);
        }
      }
    }

  private:
    vector<default_sampler_bind_point> samplers_;
  };

  template <>
  struct trait<framebuffer_> {
    using handle_type = VK_ VkFramebuffer;
    static constexpr auto create = &VK_ vkCreateFramebuffer;
    static constexpr auto destroy = &VK_ vkDestroyFramebuffer;
  };

  template <typename N>
  struct basic_framebuffer : N {
    using base = N;
    using attachment_view = box<vptr::view<trait<image_view>>>;

    constexpr basic_framebuffer(auto&&... others) : base{forward_(others)...} {}

    locked_range<VK_ VkImageView> init() {
      base::init();
      locked_range<VK_ VkImageView> atts;
      atts.reserve(attachments.size());
      for (auto& child : attachments) {
        atts.append(child.view());
      }

      return atts;
    }

   protected:
    static void release_attachment(
        vectors<VK_ VkImageView, ::std::mutex*>& attachmets) {
      for (auto& [_, lock] : attachmets) {
        if (lock) {
          lock->unlock();
        }
      }
    }

   protected:
    vector<attachment_view> attachments;
  };

  template <inside_parent<pass_, render_pass_> N>
  struct m<framebuffer_, N>
      : basic_frame_indexed_handle<basic_framebuffer<N>, trait<framebuffer_>> {
    using base =
        basic_frame_indexed_handle<basic_framebuffer<N>, trait<framebuffer_>>;

    constexpr m(framebuffer_, auto&&... others) : base{forward_(others)...} {}

    void attachment(uint32_t index, object_of<image_view> auto& view) {
      auto _ = locker_of(this);
      VKTL_ASSERT(view.frame_count() == 1u ||
                  view.frame_count() == this->frame_count());
      if (this->attachments.size() <= index) {
        this->attachments.resize(size_t(index) + 1u);
      }
      this->attachments[index] = view;
    }

   protected:
    void init() {
      if (this->is_null()) {
        locked_range<VK_ VkImageView> attachments = base::init();
        auto c = attachments.span();
        info.attachmentCount = uint32_t(c.size());
        info.pAttachments = c.data();
        info.renderPass = handle_of<graphics_pass>(this);
        this->generate(info, "[FRAMEBUFFER] Create frame buffer failure.");
      }
    }

    void reset() {
      if (!this->is_null()) {
        base::reset();
        this->destroy();
      }
    }

   protected:
    VK_ VkFramebufferCreateInfo info{ 
      .sType = VK_ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO 
    };
  };

#if defined(VK_KHR_dynamic_rendering)
  template <inside_parent<pass_, rendering_> N>
  struct m<framebuffer_, N>
      : basic_frame_indexed_handle<basic_framebuffer<N>, trait<framebuffer_>> {
    using base =
        basic_frame_indexed_handle<basic_framebuffer<N>, trait<framebuffer_>>;

    constexpr m(framebuffer_, auto&&... others) : base{forward_(others)...} {}

    void bind(uint32_t index, object_of<image_view> auto& view) {
      auto _ = locker_of(this);
      VKTL_ASSERT(view.frame_count() == 1u ||
                  view.frame_count() == this->frame_count());
      if (this->attachments_.size() <= index) {
        this->attachments_.resize(size_t(index) + 1u);
      }
      this->attachments_[index] = view;
    }

   protected:
    VK_ VkRenderingInfoKHR info{
        .sType = VK_ VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
    };
  };
#endif
}
