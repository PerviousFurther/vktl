#pragma once



VKTL_EXPORT_ namespace vktl::detail {

	template<typename N>
	struct m<device, N> : basic_layers<N> {
		using base = basic_layers<N>;

		constexpr m(device const& d, auto&&...others)
			: base{ forward_(others)... }
			, infos_{} {

			

		}

		~m() { reset(); }

		void relocate() {
			N::relocate();
			infos_.enabledExtensionCount = uint32_t(base::extensions.size());
			infos_.ppEnabledExtensionNames = base::extensions.data();
			infos_.enabledLayerCount = uint32_t(base::layers.size());
			infos_.ppEnabledLayerNames = base::layers.data();
			infos_.queueCreateInfoCount = uint32_t(queues_.size());
			infos_.pQueueCreateInfos = queues_.data();
			infos_.pEnabledFeatures = &features_;
		}

		void init() {
			if (!device_) {
				N::init();
				auto ins = parent_of<instance>(this);
				auto phydv = ins->physical_device(d.index);
				phydv_ = phydv.handle();
				infos_.sType = VK_ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
				VK_ vkCreateDevice(phydv_, &infos_, N::allocator(), &device_)
					| popup{ "[Device] Create device failure." };
			}
		}

		void reset() {
			if (device_) {
				VK_ vkDestroyDevice(device_, N::allocator());
			}
		}

		auto handle() const noexcept { return device_.value; }

	protected:
		VK_ VkPhysicalDevice phydv_;
		VK_ VkDeviceCreateInfo infos_;
		VK_ VkPhysicalDeviceFeatures features_ = {};
		vector<VK_ VkDeviceQueueCreateInfo> queues_;

		copyable_if_null<VK_ VkDevice> device_ = nullptr;
	};
}