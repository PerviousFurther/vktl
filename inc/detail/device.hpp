#pragma once

VKTL_EXPORT_ namespace vktl::vptr {
	using sampler = handle_owner<VK_ VkSampler>;
}

namespace VK_NAMESPACE {
	inline constexpr bool operator==(VK_ VkDescriptorSetLayoutBinding const& a, VK_ VkDescriptorSetLayoutBinding const& b) noexcept {
		return a.binding == b.binding
			&& a.descriptorType == b.descriptorType
			&& a.descriptorCount == b.descriptorCount
			&& a.stageFlags == b.stageFlags;
	}
	inline constexpr bool operator==(VK_ VkPushConstantRange const& a, VK_ VkPushConstantRange const& b) noexcept {
		return a.offset == b.offset
			&& a.size == b.size
			&& a.stageFlags == b.stageFlags;
	}
	inline constexpr VK_ VkDescriptorSetLayoutBinding operator|(VK_ VkDescriptorSetLayoutBinding const& left, VK_ VkDescriptorSetLayoutBinding const& right) noexcept {
		auto copy = left;
		copy.stageFlags |= right.stageFlags;
		return copy;
	}
}

VKTL_EXPORT_ namespace vktl::detail {
	
	struct descriptor_scope {
		uint32_t index = 0u; // descriptor range index.
		uint32_t element = 0u; // index inside descriptor range.
		bool is_mutable = false; // descriptor type is mutable.
	};

	struct default_descriptor_set_layout {
		VK_ VkDescriptorSetLayoutCreateFlags flags = 0u;
		vectors<VK_ VkDescriptorSetLayoutBinding
			, vector<box<vptr::sampler>>
		#if defined(VK_EXT_descriptor_indexing)
			, VK_ VkDescriptorBindingFlagsEXT
		#endif
		#if defined(VK_VALVE_mutable_descriptor_type)
			, vector<VK_ VkDescriptorType>
		#endif
			> layouts;
		
		descriptor_scope add(VK_ VkDescriptorSetLayoutBinding binding, vector<box<vptr::sampler>> sampler = {}
		#if defined(VK_EXT_descriptor_indexing)
			, VK_ VkDescriptorBindingFlagsEXT flags = 0u
		#endif
		#if defined(VK_VALVE_mutable_descriptor_type)
			, vector<VK_ VkDescriptorType> types = {}
		#endif
		) {
			assert(binding.descriptorCount == sampler.size());
			uint32_t bind_offset = binding.binding;
			bool is_mutable = false;
			auto it = layouts.begin();
			while (it != layouts.end()) {
				auto& value = it.get<0u>();
				if (value.binding + value.descriptorCount < binding.binding) {
					it++;
				}
				else {
					break;
				}
			}
			while (true) {
				if (it == layouts.end()
				|| subres.intersected(it.get<0u>().binding, it.get<0u>().descriptorCount, binding.binding, binding.descriptorCount)) {
					bool can_break = false;
					auto left_binding = it.get<0u>();
					auto intersected = subres.get_intersect(left_binding.binding, left_binding.descriptorCount, binding.binding, binding.descriptorCount);

					it.get<0u>().binding = intersected.offset;
					it.get<0u>().descriptorCount = intersected.size;
					it.get<0u>().stageFlags |= binding.stageFlags;

					auto left_samplers = ::std::move(it.get<1u>());
					::std::move(sampler.begin(), sampler.begin() + intersected.size, it.get<1u>().end());
					sampler.erase(sampler.begin(), sampler.begin() + intersected.size);

				#if defined(VK_EXT_descriptor_indexing)
					auto left_flags = it.get<2u>();
					auto binding_flags = left_flags | flags;
					it.get<2u>() |= flags;
					if (((binding_flags) & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT) != 0u) {
						this->flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
					}
				#endif
				#if defined(VK_VALVE_mutable_descriptor_type)
					auto left_types = ::std::move(it.get<3u>());
					auto copy = left_types;
					copy.insert(copy.end(), types.begin(), types.end());
					if (left_binding.descriptorType != binding.descriptorType) {
						it.get<0u>().descriptorType = VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE;
						if (left_binding.descriptorType != VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE) {
							insert(copy, left_binding.descriptorType);
						}
						if (binding.descriptorType != VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE) {
							insert(copy, binding.descriptorType);
						}
						is_mutable = true;
					}
					it.get<3u>() = ::std::move(copy);
				#else
					assert(left_binding.descriptorType == binding.descriptorType);
				#endif
					auto not_intersected = subres.get_not_intersected(left_binding.binding, left_binding.descriptorCount, binding.binding, binding.descriptorCount);
					can_break = not_intersected.count == 2u;
					for (auto c : ::std::move(not_intersected)) {
						if (c.is_first) { 
							it = layouts.insert(it, left_binding, ::std::move(left_samplers)
							#if defined(VK_EXT_descriptor_indexing)
								, left_flags
							#endif
							#if defined(VK_VALVE_mutable_descriptor_type)
								, ::std::move(left_types)
							#endif
								);
						}
						else {
							it = layouts.insert(it, binding, ::std::move(sampler)
							#if defined(VK_EXT_descriptor_indexing)
								, flags
							#endif
							#if defined(VK_VALVE_mutable_descriptor_type)
								, ::std::move(types)
							#endif
								);
						}
					}
					if (can_break) {
						break;
					}
				}
				else if (binding.binding + binding.descriptorCount == it.get<0u>().binding) {
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

				return descriptor_scope{
					.index = uint32_t(::std::distance(layouts.begin(), it)),
					.element = it.get<0u>().binding - bind_offset,
					.is_mutable = is_mutable,
				};
			}
		}

		bool operator==(const default_descriptor_set_layout& other) const noexcept {
			return flags == other.flags && layouts == other.layouts;
		}

	private:
		static constexpr void insert(vector<VK_ VkDescriptorType>& vec, VK_ VkDescriptorType type) {
			auto it = ::std::ranges::find(vec, type);
			if (it == vec.end()) {
				vec.emplace_back(type);
			}
		}
	};

	struct default_pipeline_layout {
		VK_ VkPipelineLayoutCreateFlags flags;
		vector<VK_ VkDescriptorSetLayout> layouts;
		vector<VK_ VkPushConstantRange> push_constants;

		bool operator==(default_pipeline_layout const& other) const noexcept {
			return flags == other.flags 
				&& layouts == other.layouts 
				&& push_constants == other.push_constants;
		}
	};

	template<typename N>
	struct m<device, N> : basic_extensions<N> {
		using base = basic_extensions<N>;

		constexpr m(device const& d, auto&&...others)
			: base{ forward_(others)... }
			, info{ .sType = VK_ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO } 
		{ device_index_ = d.index; }

		~m() { reset(); }

		void relocate() {
			N::relocate();

			auto extensions = base::extensions(parent_of<instance>(this)->api_version_minor());
			info.enabledExtensionCount = uint32_t(extensions.size());
			info.ppEnabledExtensionNames = extensions.data();
			info.enabledLayerCount = 0u;
			info.ppEnabledLayerNames = nullptr;
			info.queueCreateInfoCount = uint32_t(queues.size());
			info.pQueueCreateInfos = queues.data();
			info.pEnabledFeatures = &features;
		}

		bool append_extensions(const char* layer, uint16_t disabled_minor = api::max_minor + 1u) {
			assert(!handle_); return base::append_extensions(layer, disabled_minor);
		}

		void init() {
			N::init();
			auto _ = locker_of(this);
			if (!handle_) {
				auto ins = parent_of<instance>(this);
				phydv_ = ins->physical_device(device_index_);
				info.sType = VK_ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
				VK_ vkCreateDevice(phydv_, &info, N::allocator(), &handle_)
					| popup{ "[Device] Create device failure." };
			}
		}

		void reset() {
			auto _ = locker_of(this);
			if (handle_) {
				for (auto const& [layout, _] : this->pipe_layouts_) {
					VK_ vkDestroyPipelineLayout(handle_, layout, N::allocator());
				}
				for (auto const& [layout, _] : this->set_layouts_) {
					VK_ vkDestroyDescriptorSetLayout(handle_, layout, N::allocator());
				}
				VK_ vkDestroyDevice(handle_, N::allocator());
			}
		}

		auto handle() const noexcept { return handle_.value; }
		auto physical_device() const noexcept { return phydv_; }

		VK_ VkDescriptorSetLayout create(default_descriptor_set_layout value) {
			for (auto set_layout : set_layouts_) {
				if (get<1>(set_layout) == value) {
					return get<0>(set_layout);
				}
			}
			
			VK_ VkDescriptorSetLayoutCreateInfo create_info {
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.flags = value.flags,
			};

			vector<vector<VK_ VkSampler>> static_samplers;
		#if defined(VK_EXT_descriptor_indexing)
			span<VK_ VkDescriptorBindingFlagsEXT> flags_exts = value.layouts.column<2u>();
			VK_ VkDescriptorSetLayoutBindingFlagsCreateInfoEXT binding_flags_info{
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
				.bindingCount = uint32_t(flags_exts.size()),
				.pBindingFlags = flags_exts.data(),
			};
		#endif
		#if defined(VK_VALVE_mutable_descriptor_type)
			vector<VK_ VkMutableDescriptorTypeListEXT> mutables;
			VK_ VkMutableDescriptorTypeCreateInfoVALVE mutable_descriptor_info{
				.sType = VK_ VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE,
			};
		#endif
			for (auto&& [binding, samplers
			#if defined(VK_EXT_descriptor_indexing)
				, ext_flags
			#endif
			#if defined(VK_VALVE_mutable_descriptor_type)
				, types
			#endif
			] : value.layouts) {
				auto& ssamplers = static_samplers.emplace_back();
				ssamplers.reserve(samplers.size());
				for (auto& sampler : samplers) {
					ssamplers.emplace_back(sampler.handle());
				}
				binding.pImmutableSamplers = ssamplers.data();
			#if defined(VK_EXT_descriptor_indexing)
				(void)ext_flags;
			#endif
			#if defined(VK_VALVE_mutable_descriptor_type)
				mutables.emplace_back(uint32_t(types.size()), types.data());
			#endif
			}

		#if defined(VK_EXT_descriptor_indexing)
			create_info.pNext = &binding_flags_info;
		#endif
		#if defined(VK_VALVE_mutable_descriptor_type)
			binding_flags_info.pNext = &mutable_descriptor_info;
		#endif

			VK_ VkDescriptorSetLayout layout;
			VK_ vkCreateDescriptorSetLayout(handle_of<device>(this), &create_info, N::allocator(), &layout)
				| popup{"[DESCRIPTOR SET LAYOUT] Create descriptor set layout failure."};
			set_layouts_.emplace_back(layout, ::std::move(value));
			return layout;
		}

		VK_ VkPipelineLayout create(default_pipeline_layout value) {
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
			VK_ vkCreatePipelineLayout(handle_of<device>(this), &create_info, N::allocator(), &result)
				| popup{"[PIPELINE LAYOUT] Create pipeline layout failure."};
			pipe_layouts_.emplace_back(result, ::std::move(value));
			return result;
		}

	protected:
		VK_ VkDeviceCreateInfo info;
		VK_ VkPhysicalDeviceFeatures features = {};
		vector<VK_ VkDeviceQueueCreateInfo> queues;

	private:
		uint32_t device_index_;
		VK_ VkPhysicalDevice phydv_;
		copyable_if_null<VK_ VkDevice> handle_ = nullptr;

		// TODO: maybe add sampler also.

		vectors<VK_ VkDescriptorSetLayout, default_descriptor_set_layout> set_layouts_;
		vectors<VK_ VkPipelineLayout, default_pipeline_layout> pipe_layouts_;
	};

	using namespace device_extensions;

	template<typename N>
	struct m<queue_family, N> : N {
		constexpr m(queue_family family, auto&&...others)
			: N{ forward_(others)... } 
			, priorities(1.0f, family.count) {
			assert(::std::ranges::find_if(N::queues, 
				[&](auto& value) { return value.queueFamilyIndex == family.family; }) == N::queues.end()); // not allow different queue_family with same family in inherit chain.
			N::queues.emplace_back(VK_ VkDeviceQueueCreateInfo {
				.sType = VK_ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = family.family,
				.queueCount = family.count,
				.pQueuePriorities = priorities.data(),
			});
		}
		
	protected: 
		::std::vector<float> priorities;
	};
}