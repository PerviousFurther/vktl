#pragma once

// --- Agents specification -------------------------------------------------
// This file is an experimental replacement for descriptor.hpp. It is not
// included by vktl.hpp and must not be included together with descriptor.hpp.
// Descriptor allocation consumes bind-set objects only. append() immediately
// lowers each bind-set schema to dense active layouts plus a sparse-set offset
// map; pass/schema representation does not cross the allocator boundary.
// One init() call creates one exactly-sized descriptor pool for all pending
// bind sets. The allocator does not reuse capacity or move live allocations.
// Descriptor sets are expanded child-major and frame-major within each child.
// init() and reset() do not acquire or return locks.
// --------------------------------------------------------------------------

VKTL_EXPORT_ namespace vktl::detail {
	struct descriptor_handle_tag : poly_list::node {
		uint32_t type = 0u;
	};

	union descriptor_binding {
		struct {
			uint32_t set;
			uint32_t binding;
		};
		uint64_t offset;
	};

	struct bind_descriptors {
		descriptor_handle handle = nullptr;
		uint32_t first = 0u;
		uint32_t frame_count = 0u;
		uint32_t set_count = 0u;
		vector<uint16_t> set_offsets;

		void free();
	};
}

VKTL_EXPORT_ namespace vktl::vptr {
	// The allocator only needs to bind its result and query the frame count.
	// Layout declarations are lowered statically by allow_set_::append().
	struct descriptor_target {
		template<typename C>
		struct apply : C {
			template<typename T>
			constexpr void rebind() noexcept {
				vptr = {
					.bind_ = [](void* ptr, detail::bind_descriptors bind) {
						static_cast<T*>(ptr)->bind(::std::move(bind));
					},
					.frame_count_ = [](void const* ptr) noexcept {
						return static_cast<T const*>(ptr)->frame_count();
					},
				};
			}

			void bind(detail::bind_descriptors bind) {
				vptr.bind_(C::get_this(), ::std::move(bind));
			}

			uint32_t frame_count() const noexcept {
				return vptr.frame_count_(C::get_this());
			}

			descriptor_target vptr;
		};

		vfn<void(detail::bind_descriptors)> bind_;
		vfn<uint32_t() const noexcept> frame_count_;
	};
}

VKTL_EXPORT_ namespace vktl::detail {
	struct default_descriptor : descriptor_handle_tag {
		default_descriptor(auto pthis)
			: parent{ pthis }
		{}

		box<vptr::freeable<bind_descriptors>> parent;
	};

	template<typename N>
	struct m<descriptor_allocator_, N> : N {
		constexpr m(descriptor_allocator_, auto&&... others)
			: N{ forward_(others)... }
		{}

		void init() {
			N::init();
		}

		void reset() {
			N::reset();
		}
	};

	using namespace descriptor_allocator_extensions;

	namespace descriptor {
		inline constexpr auto type_set = 0x1u;
	}

	struct descriptor_pool : default_descriptor {
		descriptor_pool(auto parent)
			: default_descriptor{ parent } {
			type = descriptor::type_set;
		}

		~descriptor_pool() {
			assert(!pool);
			assert(!live_bindings);
		}

		void destroy(VK_ VkDevice device, VK_ VkAllocationCallbacks const* allocator) {
			if (!pool) return;
			VK_ vkDestroyDescriptorPool(device,
				::std::exchange(pool, VK_NULL_HANDLE), allocator);
			sets.clear();
		}

		VK_ VkDescriptorPoolCreateFlags flags = 0u;
		VK_ VkDescriptorPool pool = VK_NULL_HANDLE;
		uint32_t live_bindings = 0u;
		vector<VK_ VkDescriptorSet> sets;
	};

	namespace descriptor {
		struct pending_bind_set {
			box<vptr::descriptor_target> target;
			vector<default_descriptor_set_layout> layouts;
			vector<uint16_t> set_offsets;
		};

		struct default_extensions {
#if defined(VK_EXT_descriptor_indexing)
			vector<uint32_t> variable_counts;
			bool have_variable_count = false;
#endif
#if defined(VK_EXT_inline_uniform_block)
			uint32_t inline_binding_count = 0u;
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
			vector<VK_ VkDescriptorType> mutable_types;
#endif

			void append(default_descriptor_set_layout const& layout) {
#if defined(VK_EXT_descriptor_indexing)
				uint32_t variable_count = 0u;
#endif
				for (auto i = 0u; i < layout.layouts.size(); ++i) {
					auto const& binding = layout.layouts.template get<0u>(i);
#if defined(VK_EXT_descriptor_indexing)
					auto flags = layout.layouts.template get<2u>(i);
					if ((flags & VK_ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT) != 0u) {
						assert(!variable_count);
						variable_count = binding.descriptorCount;
						have_variable_count = true;
					}
#endif
#if defined(VK_EXT_inline_uniform_block)
					if (binding.descriptorType == VK_ VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) {
						++inline_binding_count;
					}
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
					if (binding.descriptorType == VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE) {
						auto const& types = layout.layouts
							.template get<vector<VK_ VkDescriptorType>>(i);
						for (auto type : types) insert_unique(mutable_types, type);
					}
#endif
				}
#if defined(VK_EXT_descriptor_indexing)
				variable_counts.emplace_back(variable_count);
#endif
			}

		private:
			static void insert_unique(vector<VK_ VkDescriptorType>& values,
				VK_ VkDescriptorType value) {
				if (::std::ranges::find(values, value) == values.end()) {
					values.emplace_back(value);
				}
			}
		};

		struct allocation_plan {
			VK_ VkDescriptorPoolCreateFlags pool_flags = 0u;
			vector<VK_ VkDescriptorPoolSize> pool_sizes;
			vector<VK_ VkDescriptorSetLayout> layouts;
			vector<bind_descriptors> bindings;
			default_extensions extensions;
		};
	}

	// N is usually m<descriptor_allocator_, <other>>.
	template<typename N>
	struct m<allow_set_, N> : N {
		constexpr m(allow_set_, auto&&... others)
			: N{ forward_(others)... }
		{}

		void init(VK_ VkDescriptorPoolCreateFlags pool_flags = 0u) {
			N::init();
			if (childs_.empty()) return;

			auto device = parent_of<vktl::device>(this);
			auto plan = make_plan(*device, pool_flags);
			if (plan.layouts.empty()) {
				for (auto& child : childs_) child.target.bind({});
				childs_.clear();
				return;
			}

			auto& pool = create_pool(*device, plan);
			allocate_sets(*device, pool, plan);

			auto committed = size_t{};
			try {
				for (; committed < childs_.size(); ++committed) {
					auto bind = ::std::move(plan.bindings[committed]);
					bind.handle = &pool;
					childs_[committed].target.bind(::std::move(bind));
					++pool.live_bindings;
				}
			}
			catch (...) {
				for (auto i = committed; i > 0u; --i) {
					childs_[i - 1u].target.bind({});
				}
				if (!committed) destroy_pool(pool);
				throw;
			}

			childs_.clear();
		}

		void reset() {
			assert(pools_.empty());
			childs_.clear();
			N::reset();
		}

		void append(object_of<bind_set_> auto& child) {
			auto _ = locker_of(this);
			auto const& schema = child.info();

			descriptor::pending_bind_set pending{
				.target = child,
			};
			pending.set_offsets.resize(schema.set_layout_indices.size(), uint16_t(invalid));

			for (auto set = 0u; set < schema.set_layout_indices.size(); ++set) {
				auto layout_index = schema.set_layout_indices[set];
				if (layout_index == invalid) continue;
				assert(layout_index < schema.layout_infos.size());
				assert(pending.layouts.size() < size_t(invalid));
				pending.set_offsets[set] = uint16_t(pending.layouts.size());
				pending.layouts.emplace_back(schema.layout_infos[layout_index]);
			}

			childs_.emplace_back(::std::move(pending));
		}

		void free(bind_descriptors const& bind) {
			if (!bind.handle) return;
			auto& pool = *static_cast<descriptor_pool*>(bind.handle);
			assert(pool.live_bindings);

			auto count = bind.frame_count * bind.set_count;
			assert(bind.first + count <= pool.sets.size());
			if (count && (pool.flags & VK_ VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) != 0u) {
				VK_ vkFreeDescriptorSets(handle_of<vktl::device>(this), pool.pool, count,
					pool.sets.data() + bind.first)
					| popup{ "[DESCRIPTOR POOL] Free descriptor sets failure." };
				for (auto i = 0u; i < count; ++i) {
					pool.sets[bind.first + i] = VK_NULL_HANDLE;
				}
			}

			if (--pool.live_bindings == 0u) destroy_pool(pool);
		}

	private:
		descriptor::allocation_plan make_plan(auto& device,
			VK_ VkDescriptorPoolCreateFlags pool_flags) {
			descriptor::allocation_plan plan;
			plan.pool_flags = pool_flags;
			plan.bindings.reserve(childs_.size());

			for (auto const& child : childs_) {
				vector<VK_ VkDescriptorSetLayout> layouts;
				layouts.reserve(child.layouts.size());
				for (auto const& layout : child.layouts) {
#if defined(VK_EXT_descriptor_indexing)
					if ((layout.flags
						& VK_ VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT) != 0u) {
						plan.pool_flags |= VK_ VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
					}
#endif
#if defined(VK_VALVE_mutable_descriptor_type)
					if ((layout.flags
						& VK_ VK_DESCRIPTOR_SET_LAYOUT_CREATE_HOST_ONLY_POOL_BIT_VALVE) != 0u) {
						plan.pool_flags |= VK_ VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_VALVE;
					}
#endif
					layouts.emplace_back(device.create(layout));
				}

				auto frame_count = child.target.frame_count();
				plan.bindings.emplace_back(bind_descriptors{
					.first = uint32_t(plan.layouts.size()),
					.frame_count = frame_count,
					.set_count = uint32_t(layouts.size()),
					.set_offsets = child.set_offsets,
				});

				for (auto frame = 0u; frame < frame_count; ++frame) {
					for (auto set = 0u; set < layouts.size(); ++set) {
						plan.layouts.emplace_back(layouts[set]);
						append_layout(plan, child.layouts[set]);
					}
				}
			}

			assert(plan.layouts.size() <= size_t(uint32_t(-1)));
#if defined(VK_EXT_descriptor_indexing)
			assert(plan.extensions.variable_counts.size() == plan.layouts.size());
#endif
			return plan;
		}

		static void append_layout(descriptor::allocation_plan& plan,
			default_descriptor_set_layout const& layout) {
			for (auto i = 0u; i < layout.layouts.size(); ++i) {
				insert_pool_size(plan.pool_sizes,
					layout.layouts.template get<0u>(i));
			}
			plan.extensions.append(layout);
		}

		descriptor_pool& create_pool(auto& device,
			descriptor::allocation_plan& plan) {
			VK_ VkDescriptorPoolCreateInfo info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.flags = plan.pool_flags,
				.maxSets = uint32_t(plan.layouts.size()),
				.poolSizeCount = uint32_t(plan.pool_sizes.size()),
				.pPoolSizes = data_or_null(plan.pool_sizes),
			};

#if defined(VK_EXT_inline_uniform_block)
			VK_ VkDescriptorPoolInlineUniformBlockCreateInfoEXT inline_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT,
				.maxInlineUniformBlockBindings = plan.extensions.inline_binding_count,
			};
			if (inline_info.maxInlineUniformBlockBindings) prepend(info, inline_info);
#endif

#if defined(VK_VALVE_mutable_descriptor_type)
			vector<VK_ VkMutableDescriptorTypeListVALVE> mutable_lists(plan.pool_sizes.size());
			if (!plan.extensions.mutable_types.empty()) {
				for (auto i = 0u; i < plan.pool_sizes.size(); ++i) {
					if (plan.pool_sizes[i].type != VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE) continue;
					mutable_lists[i] = {
						.descriptorTypeCount = uint32_t(plan.extensions.mutable_types.size()),
						.pDescriptorTypes = plan.extensions.mutable_types.data(),
					};
				}
			}
			VK_ VkMutableDescriptorTypeCreateInfoVALVE mutable_info{
				.sType = VK_ VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE,
				.mutableDescriptorTypeListCount = uint32_t(mutable_lists.size()),
				.pMutableDescriptorTypeLists = mutable_lists.data(),
			};
			if (!plan.extensions.mutable_types.empty()) prepend(info, mutable_info);
#endif

			auto& pool = pools_.template emplace_back<descriptor_pool>(this);
			pool.flags = plan.pool_flags;
			try {
				VK_ vkCreateDescriptorPool(device.handle(), &info, N::allocator(), &pool.pool)
					| popup{ "[DESCRIPTOR POOL] Create descriptor pool failure." };
			}
			catch (...) {
				pools_.erase(pool);
				throw;
			}
			return pool;
		}

		void allocate_sets(auto& device, descriptor_pool& pool,
			descriptor::allocation_plan& plan) {
			VK_ VkDescriptorSetAllocateInfo info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = pool.pool,
				.descriptorSetCount = uint32_t(plan.layouts.size()),
				.pSetLayouts = plan.layouts.data(),
			};

#if defined(VK_EXT_descriptor_indexing)
			VK_ VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variable_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
				.descriptorSetCount = uint32_t(plan.extensions.variable_counts.size()),
				.pDescriptorCounts = plan.extensions.variable_counts.data(),
			};
			if (plan.extensions.have_variable_count) prepend(info, variable_info);
#endif

			try {
				pool.sets.resize(plan.layouts.size(), VK_NULL_HANDLE);
				VK_ vkAllocateDescriptorSets(device.handle(), &info, pool.sets.data())
					| popup{ "[DESCRIPTOR POOL] Allocate descriptor sets failure." };
			}
			catch (...) {
				destroy_pool(pool);
				throw;
			}
		}

		static void insert_pool_size(vector<VK_ VkDescriptorPoolSize>& values,
			VK_ VkDescriptorSetLayoutBinding const& binding) {
			auto found = ::std::ranges::find_if(values,
				[&](VK_ VkDescriptorPoolSize const& value) {
					return value.type == binding.descriptorType;
				});
			if (found == values.end()) {
				values.emplace_back(VK_ VkDescriptorPoolSize{
					.type = binding.descriptorType,
					.descriptorCount = binding.descriptorCount,
				});
			}
			else {
				found->descriptorCount += binding.descriptorCount;
			}
		}

		template<typename Info, typename Extension>
		static void prepend(Info& info, Extension& extension) noexcept {
			extension.pNext = info.pNext;
			info.pNext = &extension;
		}

		void destroy_pool(descriptor_pool& pool) {
			pool.destroy(handle_of<vktl::device>(this), N::allocator());
			pools_.erase(pool);
		}

	private:
		vector<descriptor::pending_bind_set> childs_;
		poly_list pools_;
	};
}

VKTL_EXPORT_ namespace vktl::detail {
	inline void bind_descriptors::free() {
		if (!handle) return;
		static_cast<default_descriptor*>(handle)->parent.free(*this);
	}
}
