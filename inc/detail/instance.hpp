#pragma once

VKTL_EXPORT_ namespace vktl::detail {
	template<>
	struct is_host<device> : ::std::true_type {};
	template<>
	struct is_host<instance> : ::std::true_type {};

	using namespace instance_extensions;

	template<typename N>
	struct basic_layers_and_extensions : N {
		basic_layers_and_extensions(auto&&...values)
			: N{ forward_(values)... }
		{
		}

		void append_layer(const char* layer) {
			assert(layer); // no need to append layer.
			append(layers, layer);
		}
		void append_extension(const char* extension) {
			assert(extension); // no need to append layer.
			append(extensions, extension);
		}

	protected:
		vector<const char*> layers, extensions;

	private:
		static void append(auto& vec, auto value) {
			for (auto it = vec.begin(); it != vec.end(); ) {
				int result = ::std::strcmp(value, *it);
				if (result < 0) { it++; }
				else if (result == 0u) { break; }
				else if (result > 0u) {
					vec.insert(it, value);
					break;
				}
			}
		}
	};

	template<typename N>
	struct m<instance, N> : basic_layers_and_extensions<N> {
		using base = basic_layers_and_extensions<N>;

		constexpr m(instance const& info, auto&&...others)
			: base{ forward_(others)... }
			, info_{
				.sType = VK_ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO

			}
			, app_{
				.sType = VK_ VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName = info.name,
				.applicationVersion = info.version,
				.pEngineName = "VKTL@MaoQWEn",
				.engineVersion = VKTL_VERSION,
			} {

		}

		~m() {
			if (instance_) {
				VK_ vkDestroyInstance(instance_, N::allocator());
			}
		}

		void relocate() {
			N::relocate();
			info_.enabledLayerCount = uint32_t(base::layers.size());
			info_.ppEnabledLayerNames = base::layers.data();
			info_.enabledExtensionCount = uint32_t(base::extensions.size());
			info_.ppEnabledExtensionNames = base::extensions.data();
			info_.pApplicationInfo = &app_;

		}

		void init() {
			N::init();
			if (!instance_) {
				VK_ vkCreateInstance(&info_, N::allocator(), &instance_)
					| popup{ "[INSTANCE] create instance failure." };
			}
		}

		uint32_t max_api_version() noexcept {
			uint32_t version; VK_ vkEnumerateInstanceVersion(&version);
			return version;
		}

		void api_version(uint32_t value) noexcept { app_.apiVersion = value; }

		auto handle() const noexcept { return instance_; }

	protected:
		VK_ VkInstanceCreateInfo info_;
		VK_ VkApplicationInfo app_;

		copyable_if_null<VK_ VkInstance> instance_{ VK_NULL_HANDLE };
	};

	using namespace instance_extensions;

#define VKTL_INS_FN_(name, instance) ((VK_ PFN_##name)VK_ vkGetInstanceProcAddr(instance, #name))

	template<typename N>
	struct m<debug_utils, N> : N {
		constexpr m(debug_utils const& utils, auto&&...others)
			: N{ forward_(others)... }
			, debug_messanger{
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			} {
			N::append_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		void relocate() {
			N::relocate();
			N::info().pNext = &debug_messanger;
		}

	protected:
		constexpr auto& info() { return debug_messanger; }

		VK_ VkDebugUtilsMessengerCreateInfoEXT debug_messanger;
	};

	template<typename N>
	struct m<validation_layer_, N> : N {
		constexpr m(validation_layer_, auto&&...others)
			: N{ forward_(others)... } {
			N::append_layer("VK_LAYER_KHRONOS_validation");
		}
	};
}