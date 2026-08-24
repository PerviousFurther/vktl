#pragma once

// --- Agents specification -------------------------------------------------
// Descriptor allocation expands each bind-set schema in frame-major order.
// Within a frame only active Vulkan set numbers are allocated; `set_offsets`
// maps the sparse public set number to that compact allocation. Allocation
// ranges remain stable for the lifetime of the shared descriptor pool.
// --------------------------------------------------------------------------

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
		uint32_t first = 0u;
		uint32_t frame_count = 0u;
		uint32_t set_count = 0u;
		vector<uint16_t> set_offsets;
		
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
		using layout_t = detail::default_bind_set_schema;
		// using flags_t = VK_ VkDescriptorBindingFlags;
		using bind_t = detail::bind_descriptors;

		template<typename C>
		using base = apply_compose<C, bindable<bind_t>, frame_index_source>;

		template<typename C>
		struct apply;

		vfn<layout_t const& () const> info_;
	};

	template<typename C>
	struct descriptor_set::apply : base<C> {
		using base = base<C>;

		template<typename T>
		constexpr void rebind() noexcept {
			vptr = {
				.info_ = [](void const* ptr) noexcept -> layout_t const& {
					return static_cast<T const*>(ptr)->info();
				},
			};
		}

		layout_t const& info() const noexcept {
			return vptr.info_(C::get_this());
		}

		descriptor_set vptr;
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
			: default_descriptor{parent} {
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
		uint32_t allocations = 0u;
	};

	// N ususally m<descriptor_allocator_, <other>>
	template<typename N>
	struct m<allow_set_, N> : N {
		m(allow_set_, auto&&...others)
			: N{forward_(others)...} 
		{}
		
		void init(VK_ VkDescriptorPoolCreateFlags pool_flags = 0u) {
			N::init();
			auto dv = parent_of<device>(this);
			auto hdv = dv->handle();
			const bool allow_free = pool_flags & VK_ VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			auto _ = locker_of(this);

			struct child_allocation {
				uint32_t first;
				uint32_t frame_count;
				uint32_t set_count;
				vector<uint16_t> set_offsets;
			};

			vector<VK_ VkDescriptorPoolSize> pools;
			vector<VK_ VkDescriptorSetLayout> set_layouts;
			vector<vector<VK_ VkDescriptorPoolSize>> occupied;
			vector<child_allocation> allocations;
			allocations.reserve(childs_.size());

			for (auto& child : childs_) {
				auto const& schema = child.info();
				child_allocation allocation{
					.first = uint32_t(set_layouts.size()),
					.frame_count = child.frame_count(),
					.set_count = 0u,
					.set_offsets = vector<uint16_t>(schema.set_layout_indices.size(), uint16_t(invalid)),
				};

				vector<VK_ VkDescriptorSetLayout> compact_layouts;
				for (auto set = 0u; set < schema.set_layout_indices.size(); ++set) {
					auto layout_index = schema.set_layout_indices[set];
					if (layout_index == invalid) continue;
					allocation.set_offsets[set] = uint16_t(compact_layouts.size());
					auto const& layout_info = schema.layout_infos[layout_index];
					compact_layouts.emplace_back(dv->create(layout_info));
					for (auto const& row : layout_info.layouts) {
						auto binding = get<0u>(row);
						auto total = binding;
						total.descriptorCount *= allocation.frame_count;
						insert_pool_size(pools, total);
					}
				}
				allocation.set_count = uint32_t(compact_layouts.size());
				for (auto frame = 0u; frame < allocation.frame_count; ++frame) {
					for (auto set_layout : compact_layouts) {
						set_layouts.emplace_back(set_layout);
						occupied.emplace_back();
					}
				}
				allocations.emplace_back(::std::move(allocation));
			}

			if (!set_layouts.size()) { return; }

			VK_ VkDescriptorPoolCreateInfo pool_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.flags = pool_flags,
				.maxSets = uint32_t(set_layouts.size()),
				.poolSizeCount = uint32_t(pools.size()),
				.pPoolSizes = pools.data()
			};

			descriptor_pool& pool = pools_.emplace_back<descriptor_pool>(this);
			pool.flags = pool_flags;
			pool.set_occupied = ::std::move(occupied);
			VK_ vkCreateDescriptorPool(hdv, &pool_info, N::allocator(), &pool.pool)
				| popup{ "[DESCRIPTOR POOL] Create descriptor pool failure." };

			VK_ VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = pool.pool,
				.descriptorSetCount = uint32_t(set_layouts.size()),
				.pSetLayouts = set_layouts.data()
			};
			try {
				pool.sets.resize(set_layouts.size());
				VK_ vkAllocateDescriptorSets(hdv, &alloc_info, pool.sets.data())
					| popup{ "[DESCRIPTOR POOL] Allocate descriptor sets failure." };

				auto allocation = allocations.begin();
				for (auto& child : childs_) {
					child.bind(bind_descriptors{
						.handle = &pool,
						.first = allocation->first,
						.frame_count = allocation->frame_count,
						.set_count = allocation->set_count,
						.set_offsets = ::std::move(allocation->set_offsets),
						});
					++allocation;
					++pool.allocations;
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
					for (auto i = 0u; i < bind.frame_count * bind.set_count; ++i) {
						auto index = bind.first + i;
						if (!pool.sets[index]) continue;
						VK_ vkFreeDescriptorSets(handle_of<device>(this), pool.pool, 1u, &pool.sets[index])
							| popup{ "[DESCRIPTOR POOL] Free descriptor set failure." };

						for (auto occ : pool.set_occupied[index]) {
							insert_pool_size(pool.pool_sizes, occ);
						}
						pool.sets[index] = VK_NULL_HANDLE;
					}
				}

				assert(pool.allocations);
				if (--pool.allocations == 0u) {
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
			pool.sets.clear();
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
