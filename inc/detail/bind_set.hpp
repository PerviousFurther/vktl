#pragma once

// --- Agents specification -------------------------------------------------
// A bind set is a reusable logical collection of resource views. It accepts a
// pass's unified resource declarations through the allow-resource mixin chain
// and owns one `bind_set_handle` per frame; each handle stores descriptor sets
// by Vulkan set number. Incompatible schemas or occupied logical positions are
// programming errors until an explicit compatibility extension is introduced.
// Each `allow_xxx_` layer consumes its resource kind and forwards unmatched
// declarations to `N::bind`; the bind-set base asserts on unsupported kinds.
// Resource creation usage is uploaded through the bound view immediately;
// unbinding does not remove usage bits, and adding usage after the resource
// handle exists is an error. Access-state/barrier tracking is outside this
// component.
// Descriptor materialization recursively accumulates trait-specific descriptor
// infos and write records in one state tuple, records the frame-indexed sets,
// and submits the resulting write list once at the bind-set base.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::vptr {
	template<typename Trait>
	struct view_need_descriptor {
		using host_handle_type = typename detail::trait<typename Trait::host>::handle_type;
		using view_type = typename Trait::view_type;
		using layout_type = typename Trait::layout;
		using subresource_range = typename Trait::subresource_range;

		template<typename C>
		struct apply : C {
			using base = C;

			template<typename T>
			void rebind() noexcept {
				vptr_ = {
					.handle_ = [](void const* ptr, uint32_t frame) -> host_handle_type {
						return static_cast<T const*>(ptr)->handle(frame);
					},
					.view_ = [](void const* ptr, uint32_t frame) -> view_type {
						return static_cast<T const*>(ptr)->view(frame);
					},
					.layout_ = [](void const* ptr) -> layout_type {
						return static_cast<T const*>(ptr)->layout();
					},
					.range_ = [](void const* ptr) -> subresource_range {
						return static_cast<T const*>(ptr)->subresource_range();
					},
					.frame_count_ = [](void const* ptr) -> uint32_t {
						return static_cast<T const*>(ptr)->frame_count();
					},
				};
			}

			host_handle_type handle(uint32_t frame) const { return vptr_.handle_(C::get_this(), frame); }
			view_type descriptor_view(uint32_t frame) const { return vptr_.view_(C::get_this(), frame); }
			layout_type layout() const { return vptr_.layout_(C::get_this()); }
			subresource_range range() const { return vptr_.range_(C::get_this()); }
			uint32_t frame_count() const { return vptr_.frame_count_(C::get_this()); }

		private:
			view_need_descriptor vptr_;
		};

		vfn<host_handle_type(uint32_t) const> handle_;
		vfn<view_type(uint32_t) const> view_;
		vfn<layout_type() const> layout_;
		vfn<subresource_range() const> range_;
		vfn<uint32_t() const> frame_count_;
	};
}

VKTL_EXPORT_ namespace vktl::detail {
	namespace bind {
		struct top {
			vector<VK_ VkWriteDescriptorSet> sets;
		};

		struct write_buffer {
			vector<VK_ VkDescriptorBufferInfo> buffers;
			vector<VK_ VkBufferView> texel_views;
		};

		struct write_image {
			vector<VK_ VkDescriptorImageInfo> images;
		};

		inline auto get_state(trait<buffer_view_>) { return write_buffer{}; }
		inline auto get_state(trait<image_view_>) { return write_image{}; }
	}

	struct bind_set_handle {
		vector<VK_ VkDescriptorSet> sets;

		bind_set_handle() = default;
		bind_set_handle(::std::nullptr_t) noexcept {}
		bind_set_handle(bind_set_handle const&) noexcept {}
		bind_set_handle& operator=(bind_set_handle const&) noexcept {
			sets.clear();
			return *this;
		}
		bind_set_handle(bind_set_handle&&) noexcept = default;
		bind_set_handle& operator=(bind_set_handle&&) noexcept = default;

		bind_set_handle& operator=(::std::nullptr_t) noexcept {
			sets.clear();
			return *this;
		}
		friend bool operator==(bind_set_handle const& value, ::std::nullptr_t) noexcept {
			return value.sets.empty();
		}
		friend bool operator!=(bind_set_handle const& value, ::std::nullptr_t) noexcept {
			return !value.sets.empty();
		}
	};

	template<>
	struct trait<bind_set_> {
		using handle_type = bind_set_handle;
	};

	template<typename Trait>
	struct default_set_bind_point : default_resource_usage {
		box<vptr::view_need_descriptor<Trait>> view;

		constexpr bool declared() const noexcept { 
			return this->index != invalid;
		}
	};

	template<typename N>
	struct m<bind_set_, N> : basic_frame_indexed_handle<N, trait<bind_set_>> {
		using base = basic_frame_indexed_handle<N, trait<bind_set_>>;

		constexpr m(bind_set_, auto&&...others)
			: base{ forward_(others)... }
		{}

		~m() { reset(); }

		default_bind_set_schema const& info() const noexcept {
			return schema_;
		}

		void accept(default_bind_set_schema const& schema) {
			auto _ = locker_of(this);
			if (!schema_accepted_) {
				schema_ = schema;
				schema_accepted_ = true;
			}
			else {
				assert(schema_ == schema);
			}
		}

		void reset() {
			if (bind_.handle) bind_.free();
			bind_ = {};
			for (auto frame = 0u; frame < this->frame_count(); ++frame) {
				auto handle = base::handle(frame);
				handle.value.sets.clear();
			}
		}

		VK_ VkDescriptorSet descriptor_set(uint32_t frame, uint32_t set) const noexcept {
			if (frame >= this->frame_count()) return VK_NULL_HANDLE;
			auto handle = base::handle(frame);
			if (set >= handle.value.sets.size()) return VK_NULL_HANDLE;
			return handle.value.sets[set];
		}

	public:
		void bind(default_resource_usage const&) {
			assert(false && "bind_set does not allow this resource kind");
		}

	protected:
		static auto get_state() noexcept { return::std::tuple(bind::top{}); }

		void bind(uint32_t, auto&) {
			assert(false && "bind_set does not allow this resource view kind");
		}

		void bind(auto& state, bind_descriptors const& bind) {
			if (bind_.handle) bind_.free();
			bind_ = {};
			for (auto frame = 0u; frame < this->frame_count(); ++frame) {
				auto handle = base::handle(frame);
				handle.value.sets.clear();
			}
			if (!bind.handle) return;

			assert(bind.frame_count == this->frame_count());
			assert(bind.set_offsets.size() == schema_.set_layout_indices.size());
			auto& pool = *static_cast<descriptor_pool*>(bind.handle);
			for (auto frame = 0u; frame < bind.frame_count; ++frame) {
				auto handle = base::handle(frame);
				auto& sets = handle.value.sets;
				sets.resize(schema_.set_layout_indices.size(), VK_NULL_HANDLE);
				for (auto set = 0u; set < schema_.set_layout_indices.size(); ++set) {
					auto compact = bind.set_offsets[set];
					if (compact != invalid) {
						sets[set] = pool.sets[bind.first + frame * bind.set_count + compact];
					}
				}
			}

			auto& writes = ::std::get<bind::top>(state).sets;
			if (!writes.empty()) {
				VK_ vkUpdateDescriptorSets(
					handle_of<device>(this), uint32_t(writes.size()), writes.data(), 0u, nullptr);
			}
			bind_ = bind;
		}

	protected:
		default_bind_set_schema schema_;

	private:
		bool schema_accepted_ = false;
		bind_descriptors bind_;
	};

	template<typename N, typename ViewTrait>
	struct basic_allow_bind_resource : N {
		using resource_trait = trait<typename ViewTrait::host>;
		using point_type = default_set_bind_point<ViewTrait>;
		using N::bind;

		constexpr basic_allow_bind_resource(auto&&...others)
			: N{ forward_(others)... }
		{}

		void bind(default_resource_usage const& usage) {
			if (usage.resource_type != resource_trait::object_type) {
				N::bind(usage);
				return;
			}

			assert(usage.index != invalid);
			if (points_.size() <= usage.index) {
				points_.resize(usage.index + 1u);
			}

			auto& point = points_[usage.index];
			if (!point.declared()) {
				static_cast<default_resource_usage&>(point) = usage;
			}
			else {
				assert(point.type == usage.type); // binded resource conflicted.
				assert(point.set == usage.set); // binded resource conflicted.
				assert(point.binding == usage.binding);  // binded resource conflicted.

				point.usages |= usage.usages;
				point.shader_stages |= usage.shader_stages;
				point.stages |= usage.stages;
				point.access |= usage.access;
				point.dependency |= usage.dependency;
			}
		}

		void bind(uint32_t index, object_of<typename ViewTrait::type> auto& view) {
			assert(index < points_.size());
			auto& point = points_[index];
			assert(point.declared()); // the bind point is not initialized.
			view.upload_usage(typename resource_trait::usage_flags_type(point.usages));
			point.view = view;
		}

		void bind(bind_descriptors bind) {
			auto state = get_state();
			this->bind(state, ::std::move(bind));
		}

	protected:
		auto get_state() {
			return ::std::tuple_cat(
				N::get_state(), ::std::tuple{ bind::get_state(ViewTrait{}) });
		}

		void bind(auto& state, bind_descriptors const& bind) {
			if (bind.handle) fill_descriptor_writes(state, bind);
			N::bind(state, bind);
		}

	private:
		static bool uses_texel_view(VK_ VkDescriptorType type) noexcept {
			return type == VK_ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
				|| type == VK_ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		}

		void fill_descriptor_writes(auto& state, bind_descriptors const& descriptors) {
			auto write_count = size_t{};
			for (auto const& point : points_) {
				if (point.declared() && point.uses_descriptor() && !point.view.empty()) {
					write_count += descriptors.frame_count;
				}
			}

			auto& writes = ::std::get<bind::top>(state).sets;
			writes.reserve(writes.size() + write_count);
			auto& resource_state = ::std::get<decltype(bind::get_state(ViewTrait{}))>(state);
			if constexpr (::std::same_as<ViewTrait, trait<buffer_view_>>) {
				resource_state.buffers.reserve(resource_state.buffers.size() + write_count);
				resource_state.texel_views.reserve(resource_state.texel_views.size() + write_count);
			}
			else {
				resource_state.images.reserve(resource_state.images.size() + write_count);
			}

			auto& pool = *static_cast<descriptor_pool*>(descriptors.handle);
			for (auto const& point : points_) {
				if (!point.declared() || !point.uses_descriptor() || point.view.empty()) continue;
				assert(point.set < descriptors.set_offsets.size());
				auto compact_set = descriptors.set_offsets[point.set];
				assert(compact_set != invalid);
				assert(point.view.frame_count() == 1u
					|| point.view.frame_count() == descriptors.frame_count);

				for (auto frame = 0u; frame < descriptors.frame_count; ++frame) {
					auto view_frame = point.view.frame_count() == 1u ? 0u : frame;
					VK_ VkWriteDescriptorSet write{
						.sType = VK_ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						.dstSet = pool.sets[descriptors.first
							+ frame * descriptors.set_count + compact_set],
						.dstBinding = point.binding,
						.descriptorCount = 1u,
						.descriptorType = point.type,
					};

					if constexpr (::std::same_as<ViewTrait, trait<buffer_view_>>) {
						if (uses_texel_view(point.type)) {
							resource_state.texel_views.emplace_back(
								point.view.descriptor_view(view_frame));
							write.pTexelBufferView = &resource_state.texel_views.back();
						}
						else {
							auto range = point.view.range();
							resource_state.buffers.emplace_back(VK_ VkDescriptorBufferInfo{
								.buffer = point.view.handle(view_frame),
								.offset = range.offset,
								.range = range.size,
							});
							write.pBufferInfo = &resource_state.buffers.back();
						}
					}
					else {
						resource_state.images.emplace_back(VK_ VkDescriptorImageInfo{
							.imageView = point.view.descriptor_view(view_frame),
							.imageLayout = point.view.layout(),
						});
						write.pImageInfo = &resource_state.images.back();
					}
					writes.emplace_back(write);
				}
			}
		}

	protected:
		vector<point_type> points_;
	};

	template<typename N>
		requires(object_of<N, bind_set_>)
	struct m<allow_buffer_, N>
		: basic_allow_bind_resource<N, trait<buffer_view_>> {
		using base = basic_allow_bind_resource<N, trait<buffer_view_>>;
		constexpr m(allow_buffer_, auto&&...others) : base{ forward_(others)... } {}
	};

	template<typename N>
		requires(object_of<N, bind_set_>)
	struct m<allow_image_, N>
		: basic_allow_bind_resource<N, trait<image_view_>> {
		using base = basic_allow_bind_resource<N, trait<image_view_>>;
		constexpr m(allow_image_, auto&&...others) : base{ forward_(others)... } {}
	};
}
