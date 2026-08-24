#pragma once

// Interface style: resource objects gain allocation policies by composing
// focused mixins for budget, dedicated, buddy, and mapping behavior.
// Implementation: allocator/resource coupling uses handwritten vptr tables;
// concrete page strategies share small allocation primitives and state.

VKTL_EXPORT_ namespace vktl::detail {
	// tag constraint for resource.
	struct resource;

	struct default_memory_flags {
		VK_ VkMemoryPropertyFlags property = 0u;
		VK_ VkMemoryHeapFlags heap = 0u;
	};

	struct bind_memory;

}

VKTL_EXPORT_ namespace vktl::vptr {
	template<typename H = memory_handle>
	struct freeable {
		using handle_info = H;

		template<typename C>
		struct apply : C {
			using base = C;

			template<typename T>
			void rebind() {
				vptr = { 
					.free_suballoc_ = [](void* ptr, handle_info handle) {
						static_cast<T*>(ptr)->free(handle);
					},
				};
			}

			void free(handle_info handle) { vptr.free_suballoc_(C::get_this(), handle); }

			freeable vptr;
		};

		vfn<void(handle_info)> free_suballoc_;
	};

	template<typename Trait>
	struct memory_resource {
		using default_memory_flags = detail::default_memory_flags;
		using handle_type = typename Trait::handle_type;
#if defined(VK_KHR_get_memory_requirements2)
		using ext_memreq = VK_ VkMemoryRequirements2KHR;
#endif
		// using usage_flags_t = typename T::usage_flags;
		// using = typename T::usage_flags;
		template<typename C>
		using base = apply_compose<C,
			bindable<detail::bind_memory>,
			handle_owner<detail::locked<handle_type>>,
			frame_indexed>;

		template<typename C>
		struct apply : base<C> {
			using base = base<C>;

			template<typename T>
			void rebind() {
				if constexpr (requires (T & v) { { v.memory_flags() } -> ::std::convertible_to<span<default_memory_flags const>>; }) {
					vptr.memories_flags_ = [](void const* ptr) -> span<default_memory_flags const> {
						return static_cast<T const*>(ptr)->memory_flags();
						};
				}
				else if (requires (T & v) { { v.memory_flags() } -> ::std::convertible_to<default_memory_flags>; }) {
					vptr.memory_flags_ = [](void const* ptr) -> default_memory_flags {
						return static_cast<T const*>(ptr)->memory_flags();
					};
				}
				else {
					vptr.memory_flags_ = [](void const*) {
						return default_memory_flags(0u);
						};
				}
#if defined(VK_KHR_get_memory_requirements2)
				if constexpr (requires (T const& v, ext_memreq& value) { v.memory_requirement(value, 0u); }) {
					vptr.memory_requirement_ = [](void const* ptr, ext_memreq& v, uint32_t i) {
						static_cast<T const*>(ptr)->memory_requirement(v, i);
						};
				}
				else if constexpr (requires (T const& v, ext_memreq& value) { v.memory_requirement(value); }) {
					vptr.memory_requirement_ = [](void const* ptr, ext_memreq& v, uint32_t) {
						static_cast<T const*>(ptr)->memory_requirement(v);
						};
				}
#endif
				if constexpr (requires (T const& v) { { v.is_tiling() } -> ::std::convertible_to<bool>; }) {
					vptr.is_tiling_ = [](void const* ptr) -> bool { return static_cast<T const*>(ptr)->is_tiling(); };
				}
				else {
					vptr.is_tiling_ = [](void const*) { return false; };
				}
			}

#if defined(VK_KHR_get_memory_requirements2)
			bool customized() const noexcept { return vptr.memory_requirement_; }
			void memory_requirement(ext_memreq& req, uint32_t index) {
				vptr.memory_requirement_(C::get_this(), req, index);
			}
#endif
			bool multiple() const noexcept { return vptr.memories_flags_; }

			default_memory_flags memory_flags() const noexcept { return vptr.memory_flags_(C::get_this()); }
			default_memory_flags memories_flags() const noexcept { return vptr.memories_flags_(C::get_this()); }

			memory_resource vptr;
		};

#if defined(VK_KHR_get_memory_requirements2)
		vfn<void(ext_memreq&, uint32_t) const> memory_requirement_;
#endif
		vfn<default_memory_flags() const> memory_flags_;
		vfn<span<default_memory_flags const>() const> memories_flags_;
		vfn<bool() const> is_tiling_;
	};
}

VKTL_EXPORT_ namespace vktl::detail {
	struct memory_handle_tag : detail::poly_list::node {
		VK_ VkDeviceMemory handle = VK_NULL_HANDLE;
	};

	namespace memory {
		inline constexpr auto default_allocation = 0u;
	}

	namespace memory {
		inline auto requirement(VK_ VkDevice device, VK_ VkBuffer handle) {
			VK_ VkMemoryRequirements reqs;
			VK_ vkGetBufferMemoryRequirements(device, handle, &reqs);
			return reqs;
		}

		inline auto requirement(VK_ VkDevice device, VK_ VkImage handle) {
			VK_ VkMemoryRequirements reqs;
			VK_ vkGetImageMemoryRequirements(device, handle, &reqs);
			return reqs;
		}

#if defined(VK_KHR_get_memory_requirements2)
		inline void requirement(VK_ VkDevice device, VK_ VkImage handle, VK_ VkMemoryRequirements2KHR& reqs) {
			VK_ VkImageMemoryRequirementsInfo2KHR info{
				.sType = VK_ VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR,
				.image = handle,
			};
			VK_ vkGetImageMemoryRequirements2KHR(device, &info, &reqs);
		}

		inline void requirement(VK_ VkDevice device, VK_ VkBuffer handle, VK_ VkMemoryRequirements2KHR& reqs) {
			VK_ VkBufferMemoryRequirementsInfo2KHR info{
				.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR,
				.buffer = handle,
			};
			VK_ vkGetBufferMemoryRequirements2KHR(device, &info, &reqs);
		}

		inline VK_ VkMemoryRequirements const& requirement(VK_ VkMemoryRequirements2KHR const& reqs) {
			return reqs.memoryRequirements;
		}
#endif
		inline VK_ VkMemoryRequirements const& requirement(VK_ VkMemoryRequirements const& reqs) {
			return reqs;
		}
	}

	struct default_memory : memory_handle_tag {
		using base = memory_handle_tag;

		default_memory(auto pthis)
			: parent{ pthis }
		{
		}

		box<vptr::freeable<bind_memory>> parent;
		uint32_t allocation_type = memory::default_allocation;
	};
	struct bind_memory {
		memory_handle memory;
		size_t offset;
		size_t size;
		uint32_t index = 0u;

		auto empty() const noexcept { return memory; }
		auto handle() const noexcept { return memory->handle; }
		void free() { static_cast<default_memory*>(memory)->parent.free(*this); }
	};

	namespace memory {
		struct resource_allocation {
			size_t size = 0u;
			size_t alignment = 0u;
			vector<size_t> offset;
			// memory layout is tiling layout.
			bool is_tiling = false;
			// memory should directly allocate?
			bool directly_allocate = false;
			// resource group index (index expressed by childs).
			uint8_t childs_index = 0u;
			// some resource might have multiple allocation.
			uint8_t subres_index = 0u;
			// resource index inside group.
			uint16_t resource_index = 0u;
			// customize mask to identify should open some property.
			uint16_t alloc_mask = 0u;
			union {
				uint32_t type_bits = 0u;
				uint32_t type_index;
			};
			default_memory_flags flags = {};
		};

		using allocate = VK_ VkMemoryAllocateInfo;

		inline constexpr uint16_t dedicated_allocation = 1u;
	}

	namespace memory {
		struct top {
			VK_ VkPhysicalDeviceLimits limits;
			VK_ VkPhysicalDeviceMemoryProperties props;
		};
	}

	template<typename N>
	struct m<memory_allocator_, N> : N {
		using base = N;

		constexpr m(memory_allocator_, auto&&...others)
			: base{ forward_(others)... }
		{
		}

		~m() { reset(); }

	protected:
		void reset() {
			auto hdv = handle_of<device>(this);
			auto _ = locker_of(this);
			// maybe add notiftier, but validation layer will do reference checkage, lazy to implment.
			for (default_memory& memory : this->memories) {
				assert(memory.allocation_type != memory::dedicated_allocation); // test, normally, resource should already release when reset.
				VK_ vkFreeMemory(hdv, memory.handle, N::allocator());
			}
			this->memories.clear();
		}

		auto get_state(void* pprops = nullptr, void* plimits = nullptr) {
			auto handle = handle_of<device>(this);
			memory::top state{};
#if defined(VK_KHR_get_memory_requirements2)
			if (pprops) {
				auto* p2 = static_cast<VK_ VkPhysicalDeviceMemoryProperties2KHR*>(pprops);
				p2->sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR;
				VK_ vkGetPhysicalDeviceMemoryProperties2KHR(handle, p2);
				state.props = p2->memoryProperties;
			} else 
#endif
			{
				VK_ vkGetPhysicalDeviceMemoryProperties(handle, &state.props);
			}
			
#if defined(VK_KHR_get_physical_device_properties2)
			if (plimits) {
				VK_ VkPhysicalDeviceProperties2KHR infos{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
					.pNext = plimits,
				};
				VK_ vkGetPhysicalDeviceProperties2KHR(handle, &infos);
				state.limits = infos.properties.limits;
			} else 
#endif
			{
				VK_ VkPhysicalDeviceProperties devProps{};
				VK_ vkGetPhysicalDeviceProperties(handle, &devProps);
				state.limits = devProps.limits;
			}
			return::std::tuple(::std::move(state));
		}

		void each(auto const& state, auto& child, memory::resource_allocation& alloc, void* pnext = nullptr) {
			auto hdv = handle_of<device>(this);

			VK_ VkMemoryRequirements req;
#if defined(VK_KHR_get_memory_requirements2)
			if (pnext || child.customized()) {
				VK_ VkMemoryRequirements2KHR ext_req{
					.sType = VK_ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR,
					.pNext = pnext
				};
				if (child.customized()) {
					child.memory_requirement(ext_req, alloc.subres_index);
				}

				if (ext_req.memoryRequirements.size == 0u) {
					auto handle = child.handle();
					memory::requirement(hdv, handle.value, ext_req);
				}
				req = ext_req.memoryRequirements;
			} else
#endif
			{
				auto handle = child.handle();
				req = memory::requirement(hdv, handle.value);
			}

			alloc.alignment = req.alignment;
			alloc.size = req.size;
			alloc.type_bits = req.memoryTypeBits;

			find(state, alloc);
		}

		void find(auto const& state, memory::resource_allocation& prealloc) {
			VK_ VkPhysicalDeviceMemoryProperties const& properties 
				= get<memory::top>(state).props;
			auto type_bits = prealloc.type_bits;
			auto prop_flags = prealloc.flags.property;
			auto heap_flags = prealloc.flags.heap;
			if (!type_bits) {
				throw error{ VK_ VK_ERROR_FEATURE_NOT_PRESENT,
					"[MEMORY ALLOCATOR] Cannot allocate memory satisfied with specified resource." };
			}

			uint32_t i = 0;
			for (; i < properties.memoryTypeCount; ++i) {
				if ((type_bits & i) != 0u
					&& (properties.memoryTypes[i].propertyFlags & prop_flags) == prop_flags
					&& (properties.memoryHeaps[properties.memoryTypes[i].heapIndex].flags & heap_flags) == heap_flags) {
					prealloc.type_index = i;
					break;
				}
			}
			if (i >= properties.memoryTypeCount) {
				throw error{ VK_ VK_ERROR_FEATURE_NOT_PRESENT,
					"[MEMORY ALLOCATOR] Cannot allocate memory suitable for specified resource." };
			}
		}

		void allocate(auto&&...) {}

	protected:
		poly_list memories;
	};

	using namespace memory_allocator_extensions;

	// each memory allocator extensions with api selection should have functions:
	// `each` and `allocate`, where allocate have two override version.
	// 
	// one version is for directly allocate -> for dedicated or other memory decision.
	// other version is for resource bundle -> express instance of `memory::allocate`.

#if defined(VK_EXT_memory_budget)
	namespace memory {
		using budget = VK_ VkPhysicalDeviceMemoryBudgetPropertiesEXT;
	}
	// TODO: might override find is better, select most budget type index.
	template<typename N>
	struct m<budget_, N> : N {
		constexpr m(budget_, auto&&...others)
			: N{ forward_(others)... } {
			parent_of<instance>(this)
				->append_extensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, 1u);
			parent_of<device>(this)
				->append_extensions(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
		}

	protected:
		auto get_state(void* pprops = nullptr, void* plimits = nullptr) {
			memory::budget budget { 
				.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
				.pNext = pprops,
			};
			auto state = N::get_state(&budget, plimits);
			return::std::tuple_cat(::std::tuple(::std::move(budget)), ::std::move(state));
		}

		void allocate(auto& state, auto& child, memory::allocate& info, void* pnext = nullptr) {
			N::allocate(state, child, info, pnext);
			check_budget(state, info.memoryTypeIndex);
		}
		void allocate(auto& state, auto& child, memory::resource_allocation& info, void* pnext = nullptr) {
			N::allocate(state, child, info, pnext);
			check_budget(get<memory::budget>(state), info.type_index);
		}

	private:
		void check_budget(auto& state, uint32_t type_index, size_t size) {
			auto const& props = get<memory::top>(state).props;			
			memory::budget& budget = get<memory::budget>(state);
			const uint32_t heapIndex = props.memoryTypes[type_index].heapIndex;
			size_t& size_budget = budget.heapBudget[heapIndex];
			const size_t usage = budget.heapUsage[heapIndex] + size;
			if (usage <= size_budget) {
				size_budget -= size;
			}
			else {
				throw error{ VK_ VK_ERROR_OUT_OF_DEVICE_MEMORY,
					"[MEMORY ALLOCATOR] Cannot allocate memory since enough budgets." };
			}
		}
	};
#else
	template<typename N>
	struct m<budget_, N> : N {
		static_assert(always_false<N>,
			"Vulkan header not supporting VK_KHR_get_memory_requirements2 cannot use `budget`.");
	};
#endif

#if defined(VK_KHR_dedicated_allocation) || defined(VK_VERSION_1_1)
	namespace memory {
		inline VK_ VkMemoryDedicatedAllocateInfoKHR dedicated(VK_ VkBuffer handle) {
			return {
				.sType = VK_ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
				.buffer = handle,
			};
		}
		inline VK_ VkMemoryDedicatedAllocateInfoKHR dedicated(VK_ VkImage handle) {
			return {
				.sType = VK_ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
				.image = handle,
			};
		}

		inline constexpr uint16_t dedicated_bit = 0x1u;
	}

	struct dedicated_memory : default_memory { 
		dedicated_memory(auto pthis)
			: default_memory{ pthis } {
			allocation_type = memory::dedicated_allocation;
		}
	};

	template<typename N>
	struct m<dedicated_, N> : N {
	protected:
		constexpr m(dedicated_, auto&&...others)
			: N{ forward_(others)... } {
			auto dv = parent_of<device>(this);
			dv->append_extensions(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, 1);
		}

		void each(auto& state, auto& child, memory::resource_allocation& info, void* req_ptr = nullptr) {
			VK_ VkMemoryDedicatedRequirementsKHR req{
				.sType = VK_ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR,
				.pNext = req_ptr,
			};
			N::each(child, info, &req);
			// TODO: what strategy handle with prefer?
			if (/*req.prefersDedicatedAllocation || */ req.requiresDedicatedAllocation) {
				info.directly_allocate = true;
				info.alloc_mask |= memory::dedicated_bit;
			}
		}

		void allocate(auto& state, auto& child, memory::resource_allocation const& info, void* pnext = nullptr) {
			if ((info.alloc_mask & memory::dedicated_bit) != 0u) {
				auto handle = child.handle();
				auto dedicated = memory::dedicated(handle.value);

				dedicated.pNext = pnext;
				
				VK_ VkMemoryAllocateInfo alloc_info{
					.sType = VK_ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					.pNext = &dedicated,
					.allocationSize = info.size,
					.memoryTypeIndex = info.type_index,
				};

				dedicated_memory& memory
					= N::memories.template emplace_back<dedicated_memory>(this);
				bind_memory bind {
					.offset = 0u,
					.size = info.size,
				};
				try {
					VK_ vkAllocateMemory(handle_of<device>(this), &alloc_info, N::allocator(), &memory.handle)
						| popup{ "[MEMORY ALLOCATOR] Create dedicated memory failure." };
					bind.memory = &memory;
					child.bind(bind);
				}
				catch (...) {
					free(bind);
					N::memories.erase(memory);
					throw;
				}
			}
			else {
				return N::allocate(state, child, info);
			}
		}

	public:
		// the function is for internal usage, do not use.
		void free(bind_memory bind) {
			if (bind.memory) {
				VK_ vkFreeMemory(handle_of<device>(this), bind.memory->handle, N::allocator());
				N::memories.erase(*bind.memory);
			}
		}
	};
#else
	template<typename N>
	struct m<dedicated_, N> : N {
		static_assert(always_false<N>,
			"Vulkan header not supporting VK_KHR_dedicated_allocation cannot use `dedicated`.");
	};
#endif

	namespace memory {
		struct alloc_group {
			uint32_t type_index;
			vector<resource_allocation> allocation;
		};
	}

	struct page_base {
		bool is_tiling = false;
		size_t offset = 0;
		size_t total_size = 0;
	};

	template <typename P>
	struct paged_memory : default_memory {
		using page_type = P;

		paged_memory(auto pthis) : default_memory{ pthis } {
			allocation_type = page_type::allocation_type;
		}

		size_t allocate(size_t size, size_t alignment, size_t granularity, bool is_tiling) {
			return allocate_impl<true>(size, alignment, granularity, is_tiling);
		}

		size_t allocate_no_grow(size_t size, size_t alignment, bool is_tiling) {
			return allocate_impl<false>(size, alignment, 1, is_tiling);
		}

		void free(size_t offset, size_t size) {
			for (auto& p : pages) {
				if (offset >= p.offset && offset < p.offset + p.total_size) {
					p.free(offset - p.offset, size, min_block_size);
					return;
				}
			}
		}

		bool has_space(size_t size, size_t alignment, bool is_tiling) const {
			for (const auto& p : pages) {
				if (p.is_tiling == is_tiling && p.has_space(size, alignment, min_block_size)) {
					return true;
				}
			}
			return false;
		}

		void free_from_front(size_t offset, size_t size) { free(offset, size); }
		void free_from_back(size_t offset, size_t size) { free(offset, size); }

	private:
		template<bool allow_grow>
		size_t allocate_impl(size_t size, size_t alignment, size_t granularity, bool is_tiling) {
			if constexpr (allow_grow) {
				if (pages.empty() || pages.back().is_tiling != is_tiling) {
					size_t new_offset = 0;
					if (!pages.empty()) {
						const size_t prev_end = pages.back().offset + pages.back().total_size;
						new_offset = align_up(prev_end, granularity);
					}
					page_type new_page {};
					new_page.offset = new_offset;
					new_page.is_tiling = is_tiling;
					pages.push_back(::std::move(new_page));
				}
				else {
					page_type& target_page = pages.back();
					size_t local_offset = target_page.template allocate<true>(size, alignment, min_block_size);
					if (local_offset == invalid) {
						return invalid;
					}
					else {
						return target_page.offset + local_offset;
					}
				}
			}
			else {
				for (auto& p : pages) {
					if (p.is_tiling == is_tiling) {
						size_t local_offset = p.template allocate<false>(size, alignment, min_block_size);
						if (local_offset != invalid) {
							return p.offset + local_offset;
						}
					}
				}
				return invalid;
			}
		}

	private:
		static constexpr size_t default_min_block_size = 256;
		size_t min_block_size = default_min_block_size;

		::std::vector<P> pages;
	};

	// should inherit by allocator algorithm to split api call, 
	// N must not contain allocation algorithm.
	template<typename N, typename M>
	struct basic_memory_allocator : N {
	protected:
		basic_memory_allocator(auto&&...others)
			: N{ forward_(others)... }
		{
		}

	public:
		void free(bind_memory memory) {
			assert(memory.handle()
				&& static_cast<default_memory*>(memory.memory)->allocation_type == M::page_type::allocation_type);
			auto* bmem = static_cast<M*>(memory.memory);
			bmem->free(memory.offset);
		}

	protected:
		void init(auto&&...resources) {
			auto state = N::get_state();
			VK_ VkPhysicalDeviceLimits const& limits = get<memory::top>(state).limits;
			auto groups = each(state, resources...);
			for (memory::alloc_group& group : groups) {
				assert(group.allocation.size()); // for test.

				M& memory =
					N::memories.template emplace_back<M>(this);
				for (memory::resource_allocation& alloc : group.allocation) {
					alloc.offset = memory.allocate(alloc.size, alloc.alignment, limits.bufferImageGranularity, alloc.is_tiling);
				}
				memory::allocate info {
					.allocationSize = memory.total_size,
					.memoryTypeIndex = group.type_index,
				};
				allocate(memory, info);
				bind(memory, group, resources...);
			}
		}

		auto each(auto& state, uint8_t resource_index, auto& childs) {
			vector<memory::alloc_group> infos;
			infos.reserve(childs.size());
			uint16_t child_index = 0u;
			for (auto& child : childs) {
				default_memory_flags flags;
				span<default_memory_flags const> sflags;
				if (child.multiple()) {
					sflags = child.memories_flags();
				}
				else {
					flags = child.memory_flags();
					sflags = { &flags, 1u };
				}
				uint32_t frame_count = child.frame_count();
				uint8_t subres_index = 0u;
				for (auto flag : sflags) {
					memory::resource_allocation result{
						.is_tiling = child.is_tiling(),
						.resource_index = resource_index,
						.subres_index = subres_index * frame_count,
						.child_index = child_index,
						.flags = flag,
					};
					N::each(state, child, result);

					if (result.directly_allocate) {
						for (auto i = 0u; i < frame_count; i++) {
							result.subres_index++;
							allocate(state, child, result);
						}
					}
					else {
						auto type_index = result.type_index;
						auto it = ::std::ranges::find_if(infos,
							[type_index](memory::alloc_group const& group) {
								return group.type_index == type_index;
							});
						if (it == infos.end()) {
							it = infos.insert(it, memory::alloc_group{ type_index });
						}

						it->allocation.reserve(it->allocation.size() + frame_count);
						for (auto i = 0u; i < frame_count; i++) {
							result.subres_index++;
							it->allocation.emplace_back(result);
						}
					}
					subres_index++;
				}
				child_index++;
			}
			::std::ranges::stable_partition(infos, 
				[](auto const& value) { return !value.is_tiling; });
			return infos;
		}

		void allocate(auto& state, auto& child, memory::resource_allocation const& info) {
			N::allocate(state, child, info);
		}
		void allocate(auto& state, auto& memory, memory::allocate& info) {
			info.sType = VK_ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			N::allocate(state, memory, info);
		}

		auto each(auto& state, uint32_t index, auto& values, auto&...resources) {
			vector<memory::alloc_group> others = each(state, index + 1u, resources...);
			vector<memory::alloc_group> groups = each(state, index, values);
			for (memory::alloc_group const& group : groups) {
				auto it = ::std::ranges::lower_bound(others,
					[type_index = group.type_index](auto const& info) {
						return info.type_index < type_index;
					});
				if (it == others.end() || it->type_index != group.type_index) {
					it = others.insert(it, memory::alloc_group{});
				}
				vector<memory::resource_allocation>& allocs = it->allocation;

				auto group_tiling_it = ::std::ranges::find_if(group.allocation, [](auto const& val) {
					return val.is_tiling;
				});
				auto allocs_tiling_it = ::std::ranges::find_if(allocs, [](auto const& val) {
					return val.is_tiling;
				});
				allocs.insert(allocs_tiling_it, group.allocation.begin(), group_tiling_it);
				allocs.insert(allocs.end(), group_tiling_it, group.allocation.end());
			}
			return others;
		}
		auto each(auto&...resources) {
			each(0u, resources...);
		}

		auto bind(auto& memory, memory::alloc_group const& group, auto&...resources) {
			for (auto& allocation : group.allocation) {
				invoke_by_index(allocation.childs_index, [&](auto& childs) {
					childs[allocation.resource_index]
						.bind(bind_memory{
							.memory = &memory,
							.offset = ::std::move(allocation.offset),
							.size = allocation.size,
							.index = allocation.subres_index
						});
				}, resources...);
			}
		}
	};

	namespace memory {
		inline constexpr auto buddy_allocation = 0x2u;
	}

	// 
	// TODO: free from front and free from back is not finised yet.
	// 
	// TODO: these memory implmentation is not checked, they might have lots of bugs.
	//

	struct page_buddy : page_base {
		static constexpr auto allocation_type = memory::buddy_allocation;

		uint32_t size_to_order(size_t size, size_t alignment, size_t min_block_size) const {
			const size_t required = (::std::max)({ size, alignment, min_block_size });
			const size_t rounded = ::std::bit_ceil(required);
			return static_cast<uint32_t>(
				::std::bit_width(rounded) - ::std::bit_width(min_block_size));
		}

		size_t block_size(uint32_t order, size_t min_block_size) const {
			return min_block_size << order;
		}

		bool has_available_block(uint32_t order) const {
			if (order > max_order || free_lists.empty()) {
				return false;
			}
			for (uint32_t k = order; k <= max_order; ++k) {
				if (!free_lists[k].empty()) {
					return true;
				}
			}
			return false;
		}

		bool has_space(size_t size, size_t alignment, size_t min_block_size) const {
			return has_available_block(size_to_order(size, alignment, min_block_size));
		}

		void grow(uint32_t required_order, size_t min_block_size) {
			if (has_available_block(required_order)) {
				return;
			}

			if (total_size == 0) {
				max_order = required_order;
				total_size = block_size(max_order, min_block_size);

				free_lists.resize(max_order + 1);
				free_lists[max_order].push_back(0);
				return;
			}

			while (!has_available_block(required_order)) {
				const uint32_t old_order = max_order;
				const size_t old_size = total_size;

				if (!free_lists[old_order].empty()) {
					auto& root_list = free_lists[old_order];
					auto it = ::std::ranges::find(root_list, size_t{ 0 });
					if (it != root_list.end()) {
						root_list.erase(it);

						++max_order;
						total_size <<= 1;

						free_lists.resize(max_order + 1);
						free_lists[max_order].push_back(0);
						continue;
					}
				}

				++max_order;
				total_size <<= 1;

				free_lists.resize(max_order + 1);
				free_lists[old_order].push_back(old_size);
			}
		}

		template<bool allow_grow>
		size_t allocate(size_t size, size_t alignment, size_t min_block_size) {
			const uint32_t order = size_to_order(size, alignment, min_block_size);

			if constexpr (allow_grow) {
				grow(order, min_block_size);
			}
			else if (!has_available_block(order)) {
				return invalid;
			}

			uint32_t k = order;
			while (k <= max_order && free_lists[k].empty()) {
				++k;
			}

			if (k > max_order) {
				return invalid;
			}

			size_t offset_in_page = free_lists[k].back();
			free_lists[k].pop_back();

			while (k > order) {
				--k;
				const size_t buddy_offset = offset_in_page + block_size(k, min_block_size);
				free_lists[k].push_back(buddy_offset);
			}

			return offset_in_page;
		}

		void free(size_t offset_in_page, size_t size, size_t min_block_size) {
			uint32_t order = size_to_order(size, 1, min_block_size);

			while (order < max_order) {
				const size_t b_size = block_size(order, min_block_size);
				const size_t buddy_offset = offset_in_page ^ b_size;

				auto& list = free_lists[order];
				auto it = ::std::ranges::find(list, buddy_offset);
				if (it == list.end()) {
					break;
				}

				list.erase(it);
				offset_in_page = (::std::min)(offset_in_page, buddy_offset);
				++order;
			}

			free_lists[order].push_back(offset_in_page);
		}

	private:
		uint32_t max_order = 0;
		::std::vector<::std::vector<size_t>> free_lists;
	};
	using buddy_memory = paged_memory<page_buddy>;

	namespace memory {
		static constexpr auto best_fit_allocation = 0x2u;
	}

	struct page_best_fit : page_base {
		static constexpr auto allocation_type = memory::best_fit_allocation;

		struct block_info {
			size_t offset;
			size_t size;

			bool operator<(const block_info& other) const {
				if (size != other.size) return size < other.size;
				else return offset < other.offset;
			}
		};

		set<block_info> free_by_size;
		map<size_t, size_t> free_by_offset;

		template <bool can_grow>
		size_t allocate(size_t size, size_t alignment = 1, size_t min_block_size = 256) {
			return allocate_impl<can_grow>(size, alignment, min_block_size);
		}

		size_t allocate_no_grow(size_t size, size_t alignment = 1, size_t min_block_size = 256) {
			return allocate<false>(size, alignment, min_block_size);
		}

		void free(size_t offset, size_t size, size_t min_block_size = 256) {
			if (size == 0) return;

			size_t new_offset = offset;
			size_t new_size = size;

			auto next_it = free_by_offset.lower_bound(offset);
			if (next_it != free_by_offset.end() && (offset + size == next_it->first)) {
				new_size += next_it->second;
				free_by_size.erase({ next_it->first, next_it->second });
				free_by_offset.erase(next_it);
			}

			auto prev_it = free_by_offset.lower_bound(offset);
			if (prev_it != free_by_offset.begin()) {
				--prev_it;
				if (prev_it->first + prev_it->second == offset) {
					new_offset = prev_it->first;
					new_size += prev_it->second;
					free_by_size.erase({ prev_it->first, prev_it->second });
					free_by_offset.erase(prev_it);
				}
			}

			free_by_offset[new_offset] = new_size;
			free_by_size.insert({ new_offset, new_size });
		}

	private:
		auto find_best_fit(size_t size, size_t alignment) const {
			block_info dummy{ 0, size };
			auto it = free_by_size.lower_bound(dummy);

			auto best_it = free_by_size.end();
			size_t min_waste = ::std::numeric_limits<size_t>::max();

			while (it != free_by_size.end()) {
				size_t aligned_offset = align_up(it->offset, alignment);
				size_t padding = aligned_offset - it->offset;

				if (it->size >= size + padding) {
					size_t total_used = size + padding;
					size_t waste = it->size - total_used;

					if (waste < min_waste) {
						min_waste = waste;
						best_it = it;
						if (waste == 0) break;
					}
				}
				++it;
			}
			return best_it;
		}

	public:
		bool has_space(size_t size, size_t alignment = 1, size_t min_block_size = 256) const {
			return find_best_fit(size, alignment) != free_by_size.end();
		}

		void free_from_front(size_t offset, size_t size, size_t min_block_size = 256) {
			free(offset, size, min_block_size);
		}
		void free_from_behind(size_t offset, size_t size, size_t min_block_size = 256) {
			free(offset, size, min_block_size);
		}

	private:
		template <bool can_grow>
		size_t allocate_impl(size_t size, size_t alignment, size_t min_block_size) {
			assert(size);

			auto it = find_best_fit(size, alignment);
			if (it == free_by_size.end()) {
				if constexpr (can_grow) {
					size_t grow_amount = ::std::max(size + alignment, size_t(4096));
					size_t old_capacity = this->total_size;

					this->total_size += grow_amount;
					this->free(old_capacity, grow_amount, min_block_size);

					return allocate_impl<false>(size, alignment, min_block_size);
				}
				else {
					return invalid;
				}
			}

			block_info chosen = *it;

			free_by_size.erase(it);
			free_by_offset.erase(chosen.offset);

			size_t aligned_offset = align_up(chosen.offset, alignment);
			size_t front_padding = aligned_offset - chosen.offset;
			size_t back_remaining = chosen.size - front_padding - size;
			if (front_padding > 0) {
				free_by_offset[chosen.offset] = front_padding;
				free_by_size.insert({ chosen.offset, front_padding });
			}

			if (back_remaining > 0) {
				size_t back_offset = aligned_offset + size;
				free_by_offset[back_offset] = back_remaining;
				free_by_size.insert({ back_offset, back_remaining });
			}

			return aligned_offset;
		}
	};
	using best_fit_memory = paged_memory<page_best_fit>;

	namespace memory {
		inline constexpr auto linear_allocation = 0x3u;
	}

	struct page_linear : page_base {
		static constexpr auto allocation_type = memory::linear_allocation;

		struct allocation {
			size_t offset;
			size_t size;
		};

		size_t cursor = 0;
		vector<allocation> allocations;

		page_linear() = default;
		page_linear(auto pthis) : default_memory(pthis) {}

		template <bool can_grow>
		size_t allocate(size_t size, size_t alignment = 1, size_t min_block_size = 256) {
			size_t aligned_offset = align_up(cursor, alignment);
			size_t required = aligned_offset + size;

			if constexpr (can_grow) {
				if (total_size < required) {
					total_size = required;
				}
			}
			else {
				if (aligned_offset > total_size || size > total_size - aligned_offset) {
					return invalid;
				}
			}

			cursor = aligned_offset + size;
			allocations.push_back({ aligned_offset, size });

			return aligned_offset;
		}

		size_t allocate_no_grow(size_t size, size_t alignment = 1, size_t min_block_size = 256) {
			return allocate<false>(size, alignment, min_block_size);
		}

		bool has_space(size_t size, size_t alignment = 1, size_t min_block_size = 256) const {
			size_t aligned_cursor = align_up(cursor, alignment);
			return (aligned_cursor <= total_size) && (size <= total_size - aligned_cursor);
		}

		void free(size_t offset, size_t size, size_t min_block_size = 256) {
			auto it = ::std::lower_bound(
				allocations.begin(),
				allocations.end(),
				offset,
				[](const allocation& a, size_t offset) {
					return a.offset < offset;
				}
			);

			if (it != allocations.end() &&
				it->offset == offset &&
				it->size == size) {
				allocations.erase(it);
			}
		}

		void free_from_front(size_t offset, size_t size, size_t min_block_size = 256) {
			if (!allocations.empty() &&
				allocations.front().offset == offset &&
				allocations.front().size == size) {
				allocations.erase(allocations.begin());
			}
			else {
				free(offset, size, min_block_size);
			}
		}

		void free_from_behind(size_t offset, size_t size, size_t min_block_size = 256) {
			if (!allocations.empty() &&
				allocations.back().offset == offset &&
				allocations.back().size == size) {
				allocations.pop_back();
			}
			else {
				free(offset, size, min_block_size);
			}
		}
	};
	using linear_memory = paged_memory<page_linear>;

	template<typename N>
	struct m<buddy_, N> : basic_memory_allocator<N, buddy_memory> {
		using base = basic_memory_allocator<N, buddy_memory>;
		constexpr m(buddy_, auto&&...others)
			: base{ forward_(others)... } {
		}
	};
	template<typename N>
	struct m<best_fit_, N> : basic_memory_allocator<N, best_fit_memory> {
		using base = basic_memory_allocator<N, best_fit_memory>;
		constexpr m(best_fit_, auto&&...others)
			: base{ forward_(others)... } {
		}
	};
	template<typename N>
	struct m<arena_, N> : basic_memory_allocator<N, linear_memory> {
		using base = basic_memory_allocator<N, linear_memory>;
		constexpr m(arena_, auto&&...others)
			: base{ forward_(others)... } {
		}
	};

	template<typename N, typename Trait>
	struct basic_allow_resource : N {
		constexpr basic_allow_resource(auto&&...infos)
			: N{ forward_(infos)... }
		{}

		void init() { this->init(res_); }

		void reset() {
			N::reset();
			res_.clear();
		}

		void init(auto&...resources) { N::init(res_, resources...); }

		void append(object_of<typename Trait::type> auto& resource) {
			res_.push_back(resource);
		}

		template<typename Resource>
			requires requires(N& next, Resource& resource) { next.append(resource); }
		void append(Resource& resource) {
			N::append(resource);
		}

	private:
		box_list<vptr::memory_resource<Trait>> res_;
	};

	template<typename N>
	struct m<memory_allocator_extensions::allow_buffer_, N> : basic_allow_resource<N, trait<buffer>> {
		using base = basic_allow_resource<N, trait<buffer>>;
		m(allow_buffer_, auto&&...others)
			: base{ forward_(others)... }
		{
		}
	};

	template<typename N>
	struct m<memory_allocator_extensions::allow_image_, N> : basic_allow_resource<N, trait<image>> {
		using base = basic_allow_resource<N, trait<image>>;
		m(allow_image_, auto&&...others)
			: base{ forward_(others)... }
		{
		}
	};

	using namespace resource_extensions;

	// struct default_resource_memory : default_memory_flags {
	// 	vector<bind_memory> memories;
	// };

	template<typename N, typename Trait>
	struct basic_resource : basic_frame_indexed_handle<N, Trait> {
		using base = basic_frame_indexed_handle<N, Trait>;
		using handle_type = typename Trait::handle_type;

		basic_resource(auto&&...others)
			: base{ forward_(others)... } {
		}

		span<default_memory_flags const> memory_flags() const noexcept {
			return memories_.column<0u>();
		}

		void bind(bind_memory const& memory) noexcept {
			auto _ = locker_of(this);
			assert(!memories_.empty());
			assert(::std::ranges::all_of(memories_.column<1u>(),
				[](auto const& value) { return value.empty(); })); // internal test, resource cannot bind twice.
			assert(memory.index < this->frame_count()); // internal test, out of range usually.
			Trait::bind_memory(handle_of<device>(this), this->handle(memory.index), memory.handle(), memory.offset)
				| popup{ "[BIND MEMORY] Bind memory failure." };
		}

	protected:
	#if defined(VK_KHR_bind_memory2)
		// ptrs should [subres_index * frame_count], and can be access by [frame_index * subres_count + subres_index].
		void bind(bind_memory const& memory, span<void*> ptrs) noexcept {
			auto _ = locker_of(this);
			const auto frame_count = this->frame_count();
			vector<typename Trait::bind_memory_info> binds;
			binds.reserve(ptrs.size());
			
			// [subres index][frame index]
			memories_.column<1u>()[memory.index / memories_.size()][memory.index % frame_count] = memory;
			if (memory.index == memories_.size() * frame_count) {
				vector<typename Trait::bind_memory_info> binds;
				binds.reserve(memories_.size());
				for (auto frame_index = 0u; frame_index < frame_count; frame_index++) {
					auto index = frame_index * frame_count;
					auto subres_index = 0u;
					for (auto pnext : ptrs.subspan(index, index + this->memories_.size())) {
						bind_memory& memory = memories_.column<1u>()[subres_index][frame_index];
						binds.emplace_back(Trait::memory_info(pnext, this->handle(frame_index), memory.handle(), memory.offset));
						subres_index++;
					}
					Trait::bind_memory_2(handle_of<device>(this), uint32_t(binds.size()), binds.data())
						| popup{ "[BIND MEMORY] Bind memory failure." };
					binds.clear();
				}
			}
		}
	#endif

		void memory_flags(default_memory_flags flags, uint32_t subres_index = 0u) noexcept {
			if (memories_.size() <= subres_index) {
				memories_.resize(subres_index + 1u);
			}
			memories_.column<0u>()[subres_index] = flags;
			memories_.column<1u>()[subres_index].resize(this->frame_count());
		}

	private:
		// visit memory: [subresource, frame_index]
		vectors<default_memory_flags, vector<bind_memory>> memories_;
	};

	template<typename N>
	struct m<mappable_, N> : N {
		constexpr m(mappable_, auto&&...others)
			: N{ forward_(others)... } {
			N::memory_flags(default_memory_flags{
				.property = VK_ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			});
		}

		auto memory_flags() const noexcept { return N::memory_flags(); }

		void upload(byte_view data, size_t offset = maximum) {
			auto _ = locker_of(this);

			if (offset == maximum) {
				offset = bytes_.size();
			}
			else {
				bytes_.resize((::std::max)(bytes_.size(), data.size() + offset));
			}

			auto size = offset + data.size();
			if (size > bytes_.size()) {
				assert(N::info.size >= size);
				bytes_.resize(size);
			}
			::std::ranges::copy(data,
				bytes_.begin() + offset);
		}

		template<typename T>
		void upload(::std::initializer_list<T> list, size_t offset = maximum) {
			upload(byte_view{ list.begin(), list.end() }, offset);
		}

		// void upload() {
		// 	auto _ = locker_of(this);
		//
		// }

	protected:
		void memory_flags(default_memory_flags flags, uint32_t subres_index = 0u) noexcept {
			flags.property |= VK_ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			N::memory_flags(flags, subres_index);
		}

	private:
		vector<::std::byte> bytes_;
	};

	template<>
	struct express<resource_extensions::property> {
		using type = resource_extensions::property;
		static void invoke(resource_extensions::property prop, auto& base) {
			VK_ VkMemoryPropertyFlags flags = 0;

			if (prop.gpu_visible) {
				flags |= VK_ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			}
			if (prop.cpu_visible) {
				flags |= VK_ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			}
			if (prop.cache) {
				flags |= VK_ VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
			}
			if (prop.coherent) {
				flags |= VK_ VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			}

			base.memory_flags(flags);
		}
	};

}
