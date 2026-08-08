#pragma once

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
			assert(value); // no need to append string.
			constexpr auto make_sv = [](char const* elem) { return::std::string_view{ elem }; };
			minor = ::std::clamp(minor, uint16_t(1u), api::max_minor + 1u);
			auto index = api::max_minor - minor;
			auto local_end = strings_.begin() + splits_[index];
			auto it = ::std::ranges::lower_bound(
				strings_.begin(), local_end, value, {}, make_sv);
			if (it == local_end || make_sv(*it) != make_sv(value)) {
				strings_assert(::std::ranges::find_if(vec, value, {}, make_sv) == vec.end()); // not allow duplicated extension/layer at different vulkan version.
				strings_.insert(it, value);
				for (auto& value : ::std::span{ splits_ }.subpan(index)) {
					value;
				}
				return true;
			}
			else {
				return false;
			}
		}

		auto cstr_span(uint16_t index) const noexcept { 
			return::std::span{ strings_ }.subspan(splits_[api::max_minor + 1u - version]); 
		}

	protected:
		vector<const char*> strings_;
		array<uint16_t, api::max_minor + 1u> splits_;
	};

	template<typename N>
	struct basic_layers : basic_cstr_ext<N> {
		using base = basic_cstr_ext<N>;
		constexpr basic_layers(auto&&...values)
			: base{ forward_(values)... }
		{}

		bool append_layer(const char* layer, uint16_t disabled_minor = api::max_minor + 1u) {
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

		bool append_extension(const char* extension, uint16_t disabled_minor = api::max_minor + 1u) {
			return this->append_cstr(extension, disabled_minor);
		}

	protected:
		auto extensions(uint16_t version) const noexcept {
			return this->cstr_span(version);
		}
	};

	template<typename N>
	struct m<instance, N> : basic_layers<basic_extensions<N>> {
		using base = basic_layers<N>;

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
				::std::clamp(VK_MAKE_API_VERSION(0, 1, value, 0), VK_API_VERSION_1_0, max_api_version());
		}
		uint32_t api_version_minor() noexcept { return VK_API_VERSION_MINOR(app_.apiVersion); }

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
			, debug_messanger{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, } {
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