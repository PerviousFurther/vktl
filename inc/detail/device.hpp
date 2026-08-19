#pragma once

// --- Agents specification -------------------------------------------------
// Descriptor-set layouts are normalized value objects cached permanently by
// the device. A pass and a bind set keep independent schema values/handles;
// neither stores references into the other. Pipeline set numbers map through
// `default_bind_set_schema::set_layout_indices`, whose holes remain `invalid`.
// Pipeline-layout creation materializes an empty layout for every such hole.
// --------------------------------------------------------------------------

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
			assert(sampler.empty() || binding.descriptorCount == sampler.size());
			auto it = layouts.begin();
			while (it != layouts.end() && it.get<0u>().binding < binding.binding) {
				++it;
			}

			bool is_mutable = false;
			if (it != layouts.end() && it.get<0u>().binding == binding.binding) {
				auto& current = it.get<0u>();
				assert(current.descriptorCount == binding.descriptorCount);
				current.stageFlags |= binding.stageFlags;
			#if defined(VK_VALVE_mutable_descriptor_type)
				if (current.descriptorType != binding.descriptorType) {
					auto& current_types = it.get<3u>();
					insert(current_types, current.descriptorType);
					insert(current_types, binding.descriptorType);
					for (auto type : types) insert(current_types, type);
					current.descriptorType = VK_ VK_DESCRIPTOR_TYPE_MUTABLE_VALVE;
					is_mutable = true;
				}
			#else
				assert(current.descriptorType == binding.descriptorType);
			#endif
			#if defined(VK_EXT_descriptor_indexing)
				it.get<2u>() |= flags;
				if ((it.get<2u>() & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT) != 0u) {
					this->flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
				}
			#endif
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

			return descriptor_scope{
				.index = uint32_t(::std::distance(layouts.begin(), it)),
				.element = 0u,
				.is_mutable = is_mutable,
			};
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

	//struct default_bind_set_schema {
	//	vector<default_descriptor_set_layout> layout_infos;
	//	vector<uint16_t> set_layout_indices;
	//
	//	constexpr bool operator==(default_bind_set_schema const& other) const noexcept {
	//		return layout_infos == other.layout_infos
	//			&& set_layout_indices == other.set_layout_indices;
	//	}
	//};

	struct default_pipeline_layout {
		VK_ VkPipelineLayoutCreateFlags flags = 0u;
		vector<VK_ VkDescriptorSetLayout> layouts;
		vector<VK_ VkPushConstantRange> push_constants;

		constexpr bool operator==(default_pipeline_layout const& other) const noexcept {
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
			, info{ .sType = VK_ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO } { 
			device_index_ = d.index;
			(void)this->create(default_descriptor_set_layout{});
		}

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
				pipe_layouts_.clear();
				set_layouts_.clear();
				VK_ vkDestroyDevice(::std::exchange(handle_.value, VK_NULL_HANDLE), N::allocator());
			}
		}

		auto handle() const noexcept { return handle_.value; }
		auto physical_device() const noexcept { return phydv_; }

		VK_ VkDescriptorSetLayout create(default_descriptor_set_layout value) {
			auto _ = locker_of(this);
			for (auto set_layout : set_layouts_) {
				if (get<1>(set_layout) == value) {
					return get<0>(set_layout);
				}
			}
			
			vector<VK_ VkDescriptorSetLayoutBinding> bindings(
				value.layouts.column<0u>().begin(), value.layouts.column<0u>().end());
			VK_ VkDescriptorSetLayoutCreateInfo create_info {
				.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.flags = value.flags,
				.bindingCount = uint32_t(bindings.size()),
				.pBindings = bindings.data(),
			};

			vector<vector<VK_ VkSampler>> static_samplers;
			static_samplers.reserve(bindings.size());
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
			auto binding_index = 0u;
			for (auto&& [stored_binding, samplers
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
				bindings[binding_index++].pImmutableSamplers =
					ssamplers.empty() ? nullptr : ssamplers.data();
				(void)stored_binding;
			#if defined(VK_EXT_descriptor_indexing)
				(void)ext_flags;
			#endif
			#if defined(VK_VALVE_mutable_descriptor_type)
				mutables.emplace_back(uint32_t(types.size()), types.data());
			#endif
			}

			void* extension_chain = nullptr;
		#if defined(VK_VALVE_mutable_descriptor_type)
			mutable_descriptor_info.mutableDescriptorTypeListCount = uint32_t(mutables.size());
			mutable_descriptor_info.pMutableDescriptorTypeLists = mutables.data();
			mutable_descriptor_info.pNext = extension_chain;
			extension_chain = &mutable_descriptor_info;
		#endif
		#if defined(VK_EXT_descriptor_indexing)
			binding_flags_info.pNext = extension_chain;
			extension_chain = &binding_flags_info;
		#endif
			create_info.pNext = extension_chain;

			VK_ VkDescriptorSetLayout layout;
			VK_ vkCreateDescriptorSetLayout(handle_, &create_info, N::allocator(), &layout)
				| popup{"[DESCRIPTOR SET LAYOUT] Create descriptor set layout failure."};
			set_layouts_.emplace_back(layout, ::std::move(value));
			return layout;
		}
		VK_ VkPipelineLayout create(default_pipeline_layout value) {
			auto _ = locker_of(this);
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
			VK_ vkCreatePipelineLayout(handle_, &create_info, N::allocator(), &result)
				| popup{"[PIPELINE LAYOUT] Create pipeline layout failure."};
			pipe_layouts_.emplace_back(result, ::std::move(value));
			return result;
		}

		// the function designed to check whether the queue can be obtain.
		bool contain_queue(uint32_t family, uint32_t index) {
			auto it = ::std::ranges::find_if(queues, 
				[&](auto const& queue) { queue.queueFamilyIndex == family });
			if (it != queues.end()) {
				return it->queueCount > index;
			}
			else {
				return false;
			}
		}

	protected:
		VK_ VkDeviceCreateInfo info;
		VK_ VkPhysicalDeviceFeatures features = {};
		vector<VK_ VkDeviceQueueCreateInfo> queues;

	private:
		uint32_t device_index_ = 0u;
		VK_ VkPhysicalDevice phydv_{ VK_NULL_HANDLE };
		copyable_if_null<VK_ VkDevice> handle_{ VK_NULL_HANDLE };

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
