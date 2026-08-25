#pragma once

// --- Agents specification -------------------------------------------------
// Descriptor allocation expands each bind-set schema in frame-major order.
// Within a frame only active Vulkan set numbers are allocated; `set_offsets`
// maps the sparse public set number to that compact allocation. Allocation
// ranges remain stable for the lifetime of the shared descriptor pool.
// `descriptor::sets` hides its columnar storage and exposes descriptor-handle
// ranges through `handles()` and `append()`.
// --------------------------------------------------------------------------

// descriptor allocator.

VKTL_EXPORT_ namespace vktl::detail {
	struct descriptor_handle_tag : poly_list::node {
		uint32_t type;
		uint32_t padding;
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
		void* info; // since it might have descriptor buffer or descriptor heap, leave it void*.
		// count = frame count * set count.
		// to match actual set and frame, use below.
		// assume i = index
		//        set index = i / frame count
		//        frame index = i % frame count.
		span<VK_ VkDescriptorSet> sets();
		void free();
	};
}

VKTL_EXPORT_ namespace vktl::vptr {
	struct descriptor_sets {
		using layout_infos = ::std::span<detail::default_descriptor_set_layout>;
		using layout_handles = ::std::span<VK_ VkDescriptorSetLayout>;
		using bind_t = detail::bind_descriptors;

		template<typename C>
		using base = apply_compose<C, bindable<bind_t>, frame_indexed>;

		template<typename C>
		struct apply;

		template<typename T>
		constexpr void rebind() noexcept
			requires(requires (T& v) {
				{ v.infos() } -> ::std::convertible_to<layout_infos>;
				{ v.layouts() } -> ::std::convertible_to<layout_handles>;
			}) {
			vptr = {
				.info_ = [](void const* ptr) noexcept -> layout_infos {
					return static_cast<T const*>(ptr)->infos();
				},
				.layout_ = [](void const* ptr) noexcept -> layout_handles {
					return static_cast<T const*>(ptr)->layouts();
				}
			};
		}

		vfn<layout_handles() const noexcept> layouts_ = nullptr;
		vfn<layout_infos() const noexcept> infos_ = nullptr;
	};

	template<typename C>
	struct descriptor_sets::apply : base<C> {
		using base = base<C>;

		template<typename T>
		constexpr void rebind() {
			vptr.template rebind<T>();
		}

		layout_infos infos() const noexcept {
			return vptr.infos_(C::get_this());
		}
		layout_handles layouts() const noexcept {
			return vptr.layouts_(C::get_this());
		}

		descriptor_sets vptr;
	};
}

VKTL_EXPORT_ namespace vktl::detail {

	struct default_descriptor : descriptor_handle_tag {
		default_descriptor(auto pthis) : parent{pthis} {}

		box<vptr::freeable<bind_descriptors>> parent;
	};

	template<typename N>
	struct m<descriptor_allocator_, N> : N {
		m(descriptor_allocator_, auto&&...others) : N{forward_(others)...} {}
	};

	using namespace descriptor_allocator_extensions;

	namespace descriptor {
		inline constexpr auto type_pool = 0x1u;

		using pool_sizes = vectors<VK_ VkDescriptorPoolSize, 
			vector<VK_ VkDescriptorType>>;
		struct alloc_set_info : poly_list::node {
			uint16_t num_inline_uniform_block = 0u;
			uint32_t offset = 0u;
			uint32_t count = 0u;
			pool_sizes pool_sizes;
		};

		// return false means it is not new, else it is new.

		inline constexpr bool insert_pool_size(pool_sizes& vec, VK_ VkDescriptorPoolSize const& binding) {
			bool result = false;
			auto it = ::std::ranges::find_if(vec,
				[&](auto const& p) {
					return get<0u>(p).type == binding.type;
				});
			if (it == vec.end()) {
				it = vec.insert(it, VK_ VkDescriptorPoolSize{ .type = binding.type, }, by_default);
				result = true;
			}
			it.template get<0u>().descriptorCount += binding.descriptorCount;
			return result;
		}
		inline constexpr bool insert_pool_size(pool_sizes& vec, VK_ VkDescriptorSetLayoutBinding const& binding) {
			return insert_pool_size(vec, VK_ VkDescriptorPoolSize{
				.type = binding.descriptorType,
				.descriptorCount = binding.descriptorCount,
			});
		}

#if defined(VK_VALVE_mutable_descriptor_type)
		inline constexpr bool insert_pool_size(pool_sizes& vec, VK_ VkDescriptorPoolSize const& binding, vector<VK_ VkDescriptorType> const& types) {
			bool result = false;
			auto it = ::std::ranges::find_if(vec, [&](auto const& p) {
					return get<1u>(p) == types;
				});
			if (it == vec.end()) {
				it = vec.insert(it, VK_ VkDescriptorPoolSize{ .type = binding.type, }, types);
				result = true;
			}
			it.template get<0u>().descriptorCount += binding.descriptorCount;
			return result;
		}
		inline constexpr bool insert_pool_size(pool_sizes& vec, VK_ VkDescriptorSetLayoutBinding const& binding, vector<VK_ VkDescriptorType> const& types) {
			return insert_pool_size(vec, VK_ VkDescriptorPoolSize{
				.type = binding.descriptorType,
				.descriptorCount = binding.descriptorCount,
			}, types);
		}
#endif
	}

	struct descriptor_pool : default_descriptor {
		descriptor_pool(auto parent)
			: default_descriptor{parent} {
			type = descriptor::type_pool;
		}

		~descriptor_pool() { 
			assert(!pool); // descriptor leakage.
		}

		void destroy(VK_ VkDevice hdv, VK_ VkAllocationCallbacks const* ptr) {
			assert(infos.empty()); // descriptor leakage.
			assert(pool); // double free.
			VK_ vkDestroyDescriptorPool(hdv, ::std::exchange(pool, nullptr), ptr);
		}

		VK_ VkDescriptorPoolCreateFlags flags = VK_ VkDescriptorPoolCreateFlags(0u);
		VKTL_MAYBE_UNUSED uint32_t inline_block_count = 0u;
		VK_ VkDescriptorPool pool = nullptr;
		vector<VK_ VkDescriptorSet> sets;
		descriptor::pool_sizes pool_sizes; // rest pool size, enable when allow free.
		poly_list infos; // alloc infomation. use `poly_list` since `list` cannot erase from element in O(1).
	};

	// N ususally m<descriptor_allocator_, <other>>
	template<typename N>
	struct m<allow_set_, N> : N {
		m(allow_set_, auto&&...others)
			: N{forward_(others)...} 
		{}
		
		// AGENT SPECIFICATION:
		// DO NOT MODIFY THESE FUNCTION.
		void init(VK_ VkDescriptorPoolCreateFlags pool_flags = 0u) {
			N::init();

			const bool allow_free = (pool_flags & VK_ VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
			auto num_childs = uint32_t(childs_.size());

			VKTL_MAYBE_UNUSED bool have_varaible_descriptor_count = false;
			VKTL_MAYBE_UNUSED bool have_mutable_descriptor = false;
			VKTL_MAYBE_UNUSED uint32_t max_inline_uniform_block = 0u;

			descriptor::pool_sizes sizes; // [vk pool size, descriptor types]. per types.
			poly_list alloc_results; // per child, alloc set info.
#if defined(VK_VALVE_mutable_descriptor_type)
			vector<VK_ VkMutableDescriptorTypeListVALVE> type_lists; // per types.
#endif
#if defined(VK_EXT_descriptor_indexing)
			vector<uint32_t> variable_descriptor_counts; // per set.
#endif
			vector<VK_ VkDescriptorSetLayout> set_layouts; // per set.
			for (auto& child : childs_) {
				uint32_t frame_count = child.frame_count();
				// fill alloc infomation and pool infomations.
				auto index = uint32_t(set_layouts.size());
				auto& alloc = alloc_results.template emplace_back<descriptor::alloc_set_info>();
				alloc.offset = index;
				for (default_descriptor_set_layout const& layout_info : child.infos()) {
					alloc.count += frame_count;
#if defined(VK_EXT_descriptor_indexing)
					auto& variable_count = variable_descriptor_counts.emplace_back(0u);
#endif
					for (auto const& info : layout_info.layouts) {
						VK_ VkDescriptorSetLayoutBinding binding = get<0u>(info);
#if defined(VK_EXT_descriptor_indexing)
						VK_ VkDescriptorBindingFlagsEXT flags = get<2u>(info);
						if ((flags & VK_ VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT) != 0u) {
							pool_flags |= VK_ VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
						}
						if ((flags & VK_ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT) != 0u) {
							assert(variable_count == 0u); // test, if trigger this problem, release an issue.
							variable_count = binding.descriptorCount;
							have_varaible_descriptor_count = true;
						}
#endif
#if defined(VK_EXT_inline_uniform_block)
						if (binding.descriptorType == VK_ VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) {
							max_inline_uniform_block += frame_count;
							if (allow_free) {
								alloc.num_inline_uniform_block += 1u;
							}
						}
#endif
						// since fill alloc will only invoke once, multiple with frame count.
						binding.descriptorCount *= frame_count;
#if defined(VK_VALVE_mutable_descriptor_type)
						if (binding.descriptorType == VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE) {
							auto const& types = get<3u>(info);
							if (descriptor::insert_pool_size(sizes, binding, types)) {
								type_lists.emplace_back();
							}
							if (allow_free) {
								(void)descriptor::insert_pool_size(alloc.pool_sizes, binding, types);
							}
							have_mutable_descriptor = true;
						}
						else
#endif
						{
#if defined(VK_VALVE_mutable_descriptor_type)
							if (descriptor::insert_pool_size(sizes, binding)) {
								type_lists.emplace_back();
							}
#else
							(void)descriptor::insert_pool_size(sizes, binding);
#endif
							if (allow_free) {
								(void)descriptor::insert_pool_size(alloc.pool_sizes, binding);
							}
						} 
					} // end binding.	
				} // end one set.

				for (auto layout : child.layouts()) {
					set_layouts.emplace_back(layout);
				}

				// duplicate to frame count.
				auto count = set_layouts.size() - index;
				auto new_count = set_layouts.size() + count * (frame_count - 1u);
				set_layouts.resize(new_count);
				for (auto i = 0u; i < frame_count - 1u; i++) {
					auto itb = set_layouts.begin() + index + i * count;
					auto ite = itb + count;
					::std::ranges::copy(itb, ite, ite);
				}
#if defined(VK_EXT_descriptor_indexing)
				variable_descriptor_counts.resize(new_count);
				for (auto i = 0u; i < frame_count; i++) {
					auto itb = variable_descriptor_counts.begin() + index + i * count;
					auto ite = itb + count;
					::std::ranges::copy(itb, ite, ite);
				}
#endif
			} // end child.
			if (!set_layouts.size()) { 
				childs_.clear(); 
				return; 
			}

			// Create pool.

			VK_ VkDescriptorPoolCreateInfo pool_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.flags = pool_flags,
				.maxSets = uint32_t(set_layouts.size()),
				.poolSizeCount = uint32_t(sizes.size()),
				.pPoolSizes = sizes.template data<0u>(),
			};
#if defined(VK_EXT_inline_uniform_block)
			VK_ VkDescriptorPoolInlineUniformBlockCreateInfoEXT inline_pool_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT,
				.pNext = pool_info.pNext,
				.maxInlineUniformBlockBindings = max_inline_uniform_block,
			};
			if (max_inline_uniform_block > 0u) {
				pool_info.pNext = &inline_pool_info;
			}
#endif
#if defined(VK_EXT_mutable_descriptor_type)
			VK_ VkMutableDescriptorTypeCreateInfoVALVE mutable_info{
				.sType = VK_ VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE,
				.pNext = pool_info.pNext,
				.mutableDescriptorTypeListCount = uint32_t(type_lists.size()),
				.pMutableDescriptorTypeLists = type_lists.data(),
			};
			if (have_mutable_descriptor) {
				for (auto&& [types, size] : spans{ type_lists, sizes }) { // only assign, is safe.
					auto& value = get<1u>(size);
					types.descriptorTypeCount = uint32_t(value.size());
					types.pDescriptorTypes = value.data();
				}
				mutable_info.pNext = ::std::exchange(pool_info.pNext, &mutable_info);
			}
#endif // defined(VK_EXT_mutable_descriptor_type)
			descriptor_pool& pool = pools_.template emplace_back<descriptor_pool>(this);
			auto hdv = handle_of<device>(this);
			VK_ vkCreateDescriptorPool(hdv, &pool_info, N::allocator(), &pool.pool)
				| popup{ "[DESCRIPTOR POOL] Create descriptor pool failure." };
			pool.flags = pool_flags;
			pool.inline_block_count = 0u;
			pool.pool_sizes = ::std::move(sizes);
			pool.infos = ::std::move(alloc_results);

			// Allocate.

			VK_ VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = pool.pool,
				.descriptorSetCount = uint32_t(set_layouts.size()),
				.pSetLayouts = set_layouts.data()
			};
#if defined(VK_EXT_descriptor_indexing)
			VK_ VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variable_alloc_info{
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

				for (auto&& [child, info] : spans{ childs_, pool.infos }) {
					child.bind(bind_descriptors{
						.handle = &pool,
						.info = &info,
					});
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
			auto hdv = handle_of<device>(this);
			for (descriptor_pool& pool : pools_) {
				free(pool);
			}
		}

		void append(object_of<bind_set_> auto& child) {
			auto _ = locker_of(this);
			childs_.emplace_back(child);
		}
	
		void free(bind_descriptors const& bind) {
			auto _ = locker_of(this);
			if (bind.handle) {
				assert(bind.info); // test.
				auto& pool = *static_cast<descriptor_pool*>(bind.handle);
				auto& info = *static_cast<descriptor::alloc_set_info const*>(bind.info);
				if ((pool.flags & VK_ VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) != 0u) {
					for (auto const& [occ, types]: info.pool_sizes) {
						(void)descriptor::insert_pool_size(pool.pool_sizes, occ);
					}
					auto sets = span{ pool.sets }.subspan(info.offset, info.count);
					VK_ vkFreeDescriptorSets(handle_of<device>(this), pool.pool, info.count, sets.data())
						| popup{ "[DESCRIPTOR POOL] Free descriptor set failure." };
					::std::ranges::fill(sets, VK_NULL_HANDLE);
				}
				if (pool.infos.size() == 0u) {
					free(pool);
				}
			}
		}

	private:
		void free(descriptor_pool& pool) {
			VK_ vkDestroyDescriptorPool(handle_of<device>(this), pool.pool, N::allocator());
			pool.sets.clear();
			pools_.erase(pool);
		}

	private:
		vector<box<vptr::descriptor_sets>> childs_;
		poly_list pools_;
	};
}


VKTL_EXPORT_ namespace vktl::detail {
	inline span<VK_ VkDescriptorSet> bind_descriptors::sets() {
		auto& pool = *static_cast<descriptor_pool*>(handle);
		auto& info = *static_cast<descriptor::alloc_set_info const*>(this->info);
		return span{ pool.sets }.subspan(info.offset, info.count);
	}
	inline void bind_descriptors::free() {
		static_cast<default_descriptor*>(handle)->parent.free(*this);
	}
}
