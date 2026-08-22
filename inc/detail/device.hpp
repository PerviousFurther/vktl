#pragma once

// --- Agents specification -------------------------------------------------
// Vulkan 1.1+ device features are stored in a compile-time tuple selected
// from the parent instance version. relocate() must rebuild their pNext chain.
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
	
	struct default_descriptor_set_layout {
		struct scope {
			uint32_t index = 0u; // descriptor range index.
			uint32_t element = 0u; // index inside descriptor range.
			bool is_mutable = false; // descriptor type is mutable.
		};

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
		
		scope add(VK_ VkDescriptorSetLayoutBinding binding, vector<box<vptr::sampler>> sampler = {}
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

			return {
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
	struct basic_device : basic_extensions<N> {
		using base = basic_extensions<N>;

		constexpr basic_device(device const& d, auto&&...others)
			: base{ forward_(others)... } {
			device_index_ = d.index;
		}

		~basic_device() { reset(); }

		void relocate() noexcept {
			N::relocate();

			auto extensions = base::extensions(parent_of<instance>(this)->api_version_minor());
			info.enabledExtensionCount = uint32_t(extensions.size());
			info.ppEnabledExtensionNames = extensions.data();
			info.enabledLayerCount = 0u;
			info.ppEnabledLayerNames = nullptr;

			for (auto it = queues_.begin(); it != queues_.end(); ) {
				auto itnx = it;
				for (auto itn = it + 1; itn != queues_.end(); ) {
					if (itn.get<0u>().queueFamilyIndex == it.get<0u>().queueFamilyIndex) {
						auto& priorities = itn.get<1u>();
						it.get<1u>().insert(it.get<1u>().end(), priorities.begin(), priorities.end());
						it.get<0u>().queueCount += itn.get<0u>().queueCount;

						auto idx = ::std::distance(queues_.begin(), it);
						itn = queues_.erase(itn);
						it = queues_.begin() + idx;
					}
					else {
						if (itnx == it) { itnx = itn; }
						itn++;
					}
				}
				if (itnx != it) {
					it = itnx;
				}
				else {
					break;
				}
			}
			for (uint32_t index = 0u; index < uint32_t(queues_.size()); ++index) {
				queues_.get<0u>(index).pQueuePriorities = queues_.get<1u>(index).data();
			}
			info.queueCreateInfoCount = uint32_t(queues_.size());
			info.pQueueCreateInfos = queues_.data<0u>();
		}

		bool append_extensions(const char* layer, uint16_t disabled_minor = api::max_minor + 1u) {
			assert(!handle_); return base::append_extensions(layer, disabled_minor);
		}

		auto init() {
			N::init();
			auto locker = nullptr;
			if (!handle_) {
				phydv_ = parent_of<instance>(this)->physical_device(device_index_);
				VK_ vkCreateDevice(phydv_, &info, N::allocator(), &handle_) | popup{ "[Device] Create device failure." };
				(void)this->create(default_descriptor_set_layout{});
			}
			return locker;
		}

		auto reset() {
			N::reset();
			auto locker = nullptr;
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
			return locker;
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
			VK_ VkDescriptorSetLayoutCreateInfo create_info{
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
				| popup{ "[DESCRIPTOR SET LAYOUT] Create descriptor set layout failure." };

			
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
				| popup{ "[PIPELINE LAYOUT] Create pipeline layout failure." };

			pipe_layouts_.emplace_back(result, ::std::move(value));
			return result;
		}

		void append(queue info) {
			assert(!handle_); // append operation only enable when not initialized.
			if (queues_.size() && get<0u>(queues_.back()).queueFamilyIndex == info.family) {
				auto& last = get<0u>(queues_.back());
				last.queueCount = (::std::max)(info.index + 1u, last.queueCount);
				get<1u>(queues_.back()).resize(last.queueCount, 1.0f);
			}
			else {
				queues_.emplace_back(VK_ VkDeviceQueueCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = info.family,
					.queueCount = info.index + 1u,
				}, vector<float>(info.index + 1u, 1.0f));
			}
		}

		auto& last_queue() noexcept {
			assert(!handle_); // append operation only enable when not initialized.
			assert(queues_.size()); // are you forget to append queue at first?
			return queues_.back();
		}

		auto& queues() const noexcept { return queues_; }

	protected:
		VK_ VkDeviceCreateInfo info{ .sType = VK_ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };

	private:
		uint32_t device_index_ = 0u;
		VK_ VkPhysicalDevice phydv_{ VK_NULL_HANDLE };
		vectors<VK_ VkDeviceQueueCreateInfo, vector<float>> queues_;
		copyable_if_null<VK_ VkDevice> handle_{ VK_NULL_HANDLE };
		vectors<VK_ VkDescriptorSetLayout, default_descriptor_set_layout> set_layouts_;
		vectors<VK_ VkPipelineLayout, default_pipeline_layout> pipe_layouts_;
	};


	template<typename N>
		requires(!have_parent_of<instance, version1_1>)
	struct m<device, N> : basic_device<N> {
		using base = basic_device<N>;

		m(device info, auto&&...others)
			: base{ info, forward_(others)... }
		{}

		void relocate() {
			base::relocate();
			this->info.pEnabledFeatures = &features;
		}

	protected:
		VK_ VkPhysicalDeviceFeatures features = {};
	};

#if defined(VK_VERSION_1_1)
	template<typename N>
	constexpr auto make_feature_tuple() {
#if defined(VK_VERSION_1_4)
		if constexpr (inside_parent<N, instance, version1_4>) {
			return::std::tuple{
				VK_ VkPhysicalDeviceVulkan11Features{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES },
				VK_ VkPhysicalDeviceVulkan12Features{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES },
				VK_ VkPhysicalDeviceVulkan13Features{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES },
				VK_ VkPhysicalDeviceVulkan14Features{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES },
			};
		}
		else
#endif
#if defined(VK_VERSION_1_3)
		if constexpr (inside_parent<N, instance, version1_3>) {
			return::std::tuple{
				VK_ VkPhysicalDeviceVulkan11Features{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES },
				VK_ VkPhysicalDeviceVulkan12Features{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES },
				VK_ VkPhysicalDeviceVulkan13Features{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES },
			};
		}
		else
#endif
#if defined(VK_VERSION_1_2)
		if constexpr (have_parent_of<N, instance, version1_2>) {
			return ::std::tuple{
				VK_ VkPhysicalDeviceVulkan11Features{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES },
				VK_ VkPhysicalDeviceVulkan12Features{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES },
			};
		}
		else
#endif
		{
			return::std::tuple{
				VK_ VkPhysicalDeviceVulkan11Features{
					.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES },
			};
		}
	}

	template<have_parent_of<instance, version1_1> N>
	struct m<device, N> : basic_device<N> {
		using base = basic_device<N>;

		constexpr m(device info, auto&&...others)
			: base{ info, forward_(others) }
		{
		}

		void relocate() {
			base::relocate();
			void* next = const_cast<void*>(::std::exchange(
				this->info.pNext, static_cast<void const*>(&features)));
			vkconnect(version_features);
			features.pNext = next;
		}

	protected:
		VK_ VkPhysicalDeviceFeatures2KHR features = {
			.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
		};
		decltype(make_feature_tuple<N>())
			version_features = make_feature_tuple<N>();
	};
#endif

	using namespace device_extensions;

}
