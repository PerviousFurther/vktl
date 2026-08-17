#pragma once

// Interface style: an instance descriptor composes layers, extensions, and
// diagnostics while exposing loader and physical-device queries to children.
// Implementation: string lists are version-partitioned and Vulkan creation is
// deferred until the complete object chain has relocated its pointers.

#if defined(VK_API_VERSION_1_4)
# define VKTL_MAX_API_VERSION VK_API_VERSION_1_4
#elif defined(VK_API_VERSION_1_3)
# define VKTL_MAX_API_VERSION VK_API_VERSION_1_3
#elif defined(VK_API_VERSION_1_2)
# define VKTL_MAX_API_VERSION VK_API_VERSION_1_2
#elif defined(VK_API_VERSION_1_1)
# define VKTL_MAX_API_VERSION VK_API_VERSION_1_1
#else
# define VKTL_MAX_API_VERSION VK_API_VERSION_1_0
#endif

VKTL_EXPORT_ namespace vktl::detail {
	template<>
	struct is_host<device> : ::std::true_type {};
	template<>
	struct is_host<instance> : ::std::true_type {};

	using namespace instance_extensions;

	namespace api {
		inline constexpr uint16_t max_minor = VK_API_VERSION_MINOR(VKTL_MAX_API_VERSION);
		inline constexpr uint16_t max_major = VK_API_VERSION_MAJOR(VKTL_MAX_API_VERSION);
	}

	template<typename N>
	struct basic_cstr_ext : N {
		constexpr basic_cstr_ext(auto&&...values)
			: N{ forward_(values)... }
		{}

	protected:
		bool append_cstr(const char* value, uint16_t minor) {
			assert(value != nullptr && "Value pointer must not be null");
			constexpr auto make_sv = [](char const* elem) { return::std::string_view{ elem }; };

			minor = (::std::clamp)(minor, uint16_t(0u), uint16_t(api::max_minor));

			auto section_begin = strings_.begin() + splits_[minor];
			auto section_end = minor + 1 > api::max_minor ? strings_.end() : strings_.begin() + splits_[minor + 1];

			auto it = ::std::ranges::lower_bound(section_begin, section_end, value, {}, make_sv);
			if (it == section_end || make_sv(*it) != make_sv(value)) {
				strings_.insert(it, value);

				for (auto& val : ::std::span{ splits_.begin(), splits_.begin() + minor }) {
					val++;
				}
				return true;
			} 
			else {
				return false;
			}
		}

		auto cstr_span(uint16_t minor) const noexcept {
			minor = (::std::clamp)(minor, uint16_t(1u), uint16_t(api::max_minor + 1u));
			const auto index = minor;
			return ::std::span{ strings_ }.subspan(splits_[index]);
		}

	protected:
		vector<const char*> strings_;
		array<uint16_t, api::max_minor + 1u> splits_{};
	};

	template<typename N>
	struct basic_layers : basic_cstr_ext<N> {
		using base = basic_cstr_ext<N>;
		constexpr basic_layers(auto&&...values)
			: base{ forward_(values)... }
		{}

		bool append_layers(const char* layer, uint16_t disabled_minor = api::max_minor) {
			return this->append_cstr(layer, disabled_minor);
		}

	protected:
		auto layers(uint16_t version) const noexcept {
			return this->cstr_span(version);
		}
	};

	template<typename N>
	struct basic_extensions : basic_cstr_ext<N> {
		using base = basic_cstr_ext<N>;
		constexpr basic_extensions(auto&&...values)
			: base{ forward_(values)... }
		{}

		bool append_extensions(const char* extension, uint16_t disabled_minor = api::max_minor) {
			return this->append_cstr(extension, disabled_minor);
		}

	protected:
		auto extensions(uint16_t version) const noexcept {
			return this->cstr_span(version);
		}
	};

	template<typename N>
	struct m<instance, N> : basic_extensions<basic_layers<N>> {
		using base = basic_extensions<basic_layers<N>>;

		constexpr m(instance const& info, auto&&...others)
			: base{ forward_(others)... }
			, info_{
				.sType = VK_ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
			}
			, app_{
				.sType = VK_ VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName = info.name,
				.applicationVersion = info.version,
				.pEngineName = "VKTL@MaoQWE",
				.engineVersion = VKTL_VERSION,
			} 
		{}

		~m() {
			if (instance_) {
				VK_ vkDestroyInstance(instance_, N::allocator());
			}
		}

		void relocate() {
			N::relocate();
			auto layers = base::layers(api_version_minor());
			auto extensions = base::extensions(api_version_minor());
			info_.enabledLayerCount = uint32_t(layers.size());
			info_.ppEnabledLayerNames = layers.data();
			info_.enabledExtensionCount = uint32_t(extensions.size());
			info_.ppEnabledExtensionNames = extensions.data();
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
			uint32_t version; 
			VK_ vkEnumerateInstanceVersion(&version)
				| popup{"[INSTANCE] Vulkan driver or loader is not supported on this system."};
			return version;
		}

		void api_version_minor(uint32_t minor) noexcept { 
			app_.apiVersion = 
				::std::clamp(VK_MAKE_API_VERSION(0, 1, minor, 0), VK_API_VERSION_1_0, max_api_version());
		}
		uint32_t api_version_minor() noexcept { return VK_API_VERSION_MINOR(app_.apiVersion); }

		auto handle() const noexcept { return instance_; }

		VK_ VkPhysicalDevice physical_device(uint32_t index) const {
			uint32_t count = 0u;
			VK_ vkEnumeratePhysicalDevices(instance_, &count, nullptr)
				| popup{ "[INSTANCE] Enumerate physical devices failure." };
			if (index >= count) {
				throw error{ int(VK_ VK_ERROR_INITIALIZATION_FAILED), "[INSTANCE] Physical device index is out of range." };
			}
			vector<VK_ VkPhysicalDevice> devices(count);
			VK_ vkEnumeratePhysicalDevices(instance_, &count, devices.data())
				| popup{ "[INSTANCE] Enumerate physical devices failure." };
			return devices[index];
		}

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
			, debug_messanger{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, } {
			N::append_extensions(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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
			N::append_layers("VK_LAYER_KHRONOS_validation");
		}
	};
}
