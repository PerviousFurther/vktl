#pragma once

// Interface style: descriptor allocators compose allocation policies and bind
// frame-aware bind-set children through a small allocator-facing API.
// Implementation: pools, layouts, and child lifetimes are coordinated through
// explicit handles and handwritten vptr adapters.


// descriptor allocator.

VKTL_EXPORT_ namespace vktl::detail {
	struct descriptor_handle_tag : poly_list::node {
		uint32_t type;

	};

	union descriptor_binding {
		struct {
			uint32_t set;
			uint32_t binding;
		};
		uint64_t offset;
	};

	struct bind_descriptors {
		descriptor_handle handle;
		union {
			uint32_t set;
			uint32_t offset;
		};
		uint32_t count;
		
		// void empty() const noexcept { return handle; }
		void free();
		// VK_ VkDescriptorSet handle() const;
	};

	// struct bind_descriptor {
	// 	// descriptor_handle handle;
	// 	void 
	// };
}

VKTL_EXPORT_ namespace vktl::vptr {
	struct descriptor_set {
		using layout_t = detail::default_descriptor_set_layout;
		// using flags_t = VK_ VkDescriptorBindingFlags;
		using bind_t = detail::bind_descriptors;

		template<typename C>
		using base = apply_compose<C, bindable<bind_t>, frame_related>;

		template<typename C>
		struct apply;

		vfn<layout_t const& () const> info_;
	};

	template<typename C>
	struct descriptor_set::apply : base<C> {
		using base = base<C>;

		template<typename T>
		constexpr void rebind() noexcept {
			vptr_ = {
				.info_ = [](void const* ptr) noexcept -> layout_t const& {
					return static_cast<T const*>(ptr)->info();
				},
			};
		}

		layout_t const& info() const noexcept {
			return vptr_.info_(C::get_this());
		}

		descriptor_set vptr_;
	};
}

VKTL_EXPORT_ namespace vktl::detail {

	struct default_descriptor : descriptor_handle_tag {
		default_descriptor(auto pthis)
			: parent{pthis} 
		{}

		box<vptr::freeable<bind_descriptors>> parent;
	};

	template<typename N>
	struct m<descriptor_allocator_, N> : N {
		m(descriptor_allocator_, auto&&...others)
			: N{forward_(others)...} {
		}

		void init() {
			N::init();
		}

		void reset() {}
	};

	using namespace descriptor_allocator_extensions;

	namespace descriptor {
		inline constexpr auto type_set = 0x1u;
	}

	struct descriptor_pool : default_descriptor {
		descriptor_pool(auto parent)
			: descriptor_pool{parent} {
			type = descriptor::type_set;
		}

		~descriptor_pool() { 
			assert(!sets.size()); // memory leakage.
		}

		void destroy(VK_ VkDevice hdv, VK_ VkAllocationCallbacks const* ptr) {
			assert(pool); VK_ vkDestroyDescriptorPool(hdv, pool, ptr);
		}

		VK_ VkDescriptorPoolCreateFlags flags;
		
		VK_ VkDescriptorPool pool;
		vector<VK_ VkDescriptorPoolSize> pool_sizes;
		vector<VK_ VkDescriptorSet> sets;
		vector<vector<VK_ VkDescriptorPoolSize>> set_occupied;
	};

	// N ususally m<descriptor_allocator_, <other>>
	template<typename N>
	struct m<allow_set_, N> : N {
		m(allow_set_, auto&&...others)
			: N{forward_(others)...} 
		{}
		
		// TODO: kind ugly.
		void init(VK_ VkDescriptorPoolCreateFlags pool_flags = 0u) {
			N::init();
			auto dv = parent_of<device>(this);
			auto hdv = dv->handle();

			const bool allow_free = pool_flags & VK_ VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

			auto _ = locker_of(this);
			auto num_childs = uint32_t(childs_.size());

		#if defined(VK_VALVE_mutable_descriptor_type)
			vector<VK_ VkDescriptorType> type_list;
		#endif // defined(VK_VALVE_mutable_descriptor_type)
		#if defined(VK_EXT_descriptor_indexing)
			vector<uint32_t> variable_descriptor_counts(num_childs);
			variable_descriptor_counts.reserve(num_childs);
			bool have_varaible_descriptor_count = false;
		#endif
		#if defined(VK_EXT_inline_uniform_block)
			uint32_t max_inline_uniform_block = 0u;
		#endif
			vector<VK_ VkDescriptorPoolSize> pools;
			vector<vector<VK_ VkDescriptorPoolSize>> child_occupied;
			child_occupied.reserve(num_childs);

			vector<VK_ VkDescriptorSetLayout> set_layouts;
			set_layouts.reserve(num_childs);
			for (auto& child : childs_) {
				uint32_t frame_count = child.frame_count();
				default_descriptor_set_layout layout_info = child.info();
				for (auto const& info : layout_info.layouts) {
					VK_ VkDescriptorSetLayoutBinding binding = get<0u>(info);
				#if defined(VK_EXT_inline_uniform_block)
					if (binding.descriptorType == VK_ VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) {
						max_inline_uniform_block += frame_count;
					} 
				#endif

					if (allow_free) {
						for (auto i = 0u; i < frame_count; i++) {
							insert_pool_size(child_occupied.emplace_back(), binding);
						}
					}

				#if defined(VK_VALVE_mutable_descriptor_type)
					if (binding.descriptorType == VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE) {
						vector<VK_ VkDescriptorType> const& types = get<3u>(info);
						::std::ranges::copy_if(types,
							::std::back_inserter(type_list), 
							[&](auto type) {
								return::std::ranges::find(type_list, type) == type_list.end();
							});
					}
				#endif

				#if defined(VK_EXT_descriptor_indexing)
					VK_ VkDescriptorBindingFlagsEXT flags = get<2u>(info);
					for (auto i = 0u; i < frame_count; i++) {
						auto& vdc = variable_descriptor_counts.emplace_back();
						if ((flags & VK_ VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT) != 0u) {
							pool_flags |= VK_ VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
						}
						if ((flags & VK_ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT) != 0u) {
							// usually only last one have this, thus directly increase is ok.
							vdc = binding.descriptorCount;
							have_varaible_descriptor_count = true;
						}
					}
				#endif

					binding.descriptorCount *= frame_count;
					insert_pool_size(pools, binding);
				}

				auto layout = dv->create(::std::move(layout_info));
				for (auto i = 0u; i < frame_count; i++) {
					set_layouts.emplace_back(layout);
				}	
			}

			if (!set_layouts.size()) { return; }

			VK_ VkDescriptorPoolCreateInfo pool_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.flags = pool_flags,
				.maxSets = uint32_t(set_layouts.size()),
				.poolSizeCount = uint32_t(pools.size()),
				.pPoolSizes = pools.data()
			};

		#if defined(VK_EXT_inline_uniform_block)
			VK_ VkDescriptorPoolInlineUniformBlockCreateInfoEXT inline_pool_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT,
				.pNext = pool_info.pNext,
				.maxInlineUniformBlockBindings = max_inline_uniform_block
			};
			if (max_inline_uniform_block > 0u) {
				pool_info.pNext = &inline_pool_info;
			}
		#endif
		#if defined(VK_EXT_mutable_descriptor_type)
			auto it = ::std::ranges::find_if(pools, 
				[](VK_ VkDescriptorPoolSize const& p) { return p.type == VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE; });
			if (it != pools.end()) {
				::std::iter_swap(pools.begin(), it);
				VK_ VkMutableDescriptorTypeListVALVE type_list_info{
					.descriptorTypeCount = uint32_t(type_list.size()),
					.pDescriptorTypes = type_list.data(),
				};
				VK_ VkMutableDescriptorTypeCreateInfoVALVE mutable_info{
					.sType = VK_ VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE,
					.pNext = pool_info.pNext,
					.mutableDescriptorTypeListCount = 1u,
					.pMutableDescriptorTypeLists = &type_list_info,
				};
				if (type_list.size()) {
					mutable_info.pNext = ::std::exchange(pool_info.pNext, &mutable_info);
				}
			}
		#endif // defined(VK_EXT_mutable_descriptor_type)
			descriptor_pool& pool = pools_.emplace_back<descriptor_pool>(this);
			pool.flags = pool_flags;
			VK_ vkCreateDescriptorPool(hdv, &pool_info, N::allocator(), &pool.pool)
				| popup{ "[DESCRIPTOR POOL] Create descriptor pool failure." };

			VK_ VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = pool.pool,
				.descriptorSetCount = uint32_t(set_layouts.size()),
				.pSetLayouts = set_layouts.data()
			};
		#if defined(VK_EXT_descriptor_indexing)
			VK_ VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variable_alloc_info {
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
				.descriptorSetCount = uint32_t(variable_descriptor_counts.size()),
				.pDescriptorCounts = variable_descriptor_counts.data()
			};
			if (have_varaible_descriptor_count) {
				variable_alloc_info.pNext = ::std::exchange(alloc_info.pNext, &variable_alloc_info);
			}
		#endif
			try {
				pool.sets.resize(set_layouts.size());
				VK_ vkAllocateDescriptorSets(hdv, &alloc_info, pool.sets.data())
					| popup{ "[DESCRIPTOR POOL] Allocate descriptor sets failure." };

				// auto its = pool.sets.begin();
				auto set_index = 0u;

				for (auto& child : childs_) {
					uint32_t frame_count = child.frame_count();
					child.bind(bind_descriptors{
						.handle = &pool,
						.set = set_index,
						.count = frame_count,
						});
					set_index += frame_count;
				}
				childs_.clear();
			}
			catch (...) {
				for (auto& child : childs_) {
					child.bind(bind_descriptors{});
				}
				free(pool);
				throw;
			}
		}

		void reset() {
			N::reset();
			auto _ = locker_of(this);
			for (auto& child : childs_) {
				child.bind(bind_descriptors{});
			}
			childs_.clear();
		}

		void append(object_of<bind_set_> auto& child) {
			childs_.emplace_back(child);
		}
	
		void free(bind_descriptors const& bind) {
			if (bind.handle) {
				auto& pool = *static_cast<descriptor_pool*>(bind.handle);
				if ((pool.flags & VK_ VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) != 0u) {
					for (auto i = 0u; i < bind.count; i++) {
						VK_ vkFreeDescriptorSets(handle_of<device>(this), pool.pool, 1u, &pool.sets[bind.set + i])
							| popup{ "[DESCRIPTOR POOL] Free descriptor set failure." };

						for (auto occ : pool.set_occupied[bind.set + i]) {
							insert_pool_size(pool.pool_sizes, occ);
						}
					}
				}

				pool.sets.erase(pool.sets.begin() + bind.set, pool.sets.begin() + bind.set + bind.count);
				if (!pool.sets.size()) {
					free(pool);
				}
			}
		}

	private:
		static constexpr void insert_pool_size(auto& vec, VK_ VkDescriptorSetLayoutBinding const& binding) {
			insert_pool_size(vec, VK_ VkDescriptorPoolSize{
				.type = binding.descriptorType,
				.descriptorCount = binding.descriptorCount,
			});
		}
		static constexpr void insert_pool_size(auto& vec, VK_ VkDescriptorPoolSize const& binding) {
			auto it = ::std::ranges::find_if(vec,
				[&](VK_ VkDescriptorPoolSize const& p) {
					return p.type == binding.type;
				});
			if (it == vec.end()) {
				it = vec.insert(it, VK_ VkDescriptorPoolSize{
					.type = binding.type,
					.descriptorCount = 0u
					});
			}
			it->descriptorCount += binding.descriptorCount;
		}

		void free(descriptor_pool& pool) {
			VK_ vkDestroyDescriptorPool(handle_of<device>(this), pool.pool, N::allocator());
			pools_.erase(pool);
		}

	private:
		vector<box<vptr::descriptor_set>> childs_;
		poly_list pools_;
	};
}


VKTL_EXPORT_ namespace vktl::detail {
	inline void bind_descriptors::free() {
		static_cast<default_descriptor*>(handle)->parent.free(*this);
	}
}
