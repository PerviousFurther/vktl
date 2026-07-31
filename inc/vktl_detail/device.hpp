#pragma once

VKTL_EXPORT_ namespace vktl::detail {
    namespace physical_device_properties {
        struct property_disable {
            static constexpr auto enhance = false;
        };
        struct property_enable {
            static constexpr auto enhance = true;
        };

        namespace queue_family_properties_extensions {
            struct queue_global_priorites {};
        }
        struct queue_family_properties {};

        namespace surface_present_mode_extensions {
        }
        struct surface_present_mode { VK_ VkSurfaceKHR surface; };

        namespace surface_format_extensions {

        }
        struct surface_format { VK_ VkSurfaceKHR surface; };

        namespace memory_properties_extensions {
            struct memory_budget_properties {};
        }
        struct memory_properties {
        };

        namespace properties_extensions {
        }
        struct properties {
        };
    }
    namespace physical_device_extensions {
    }
    struct physical_device {
        VK_ VkPhysicalDevice handle;
    };
}

VKTL_EXPORT_ namespace vktl::detail {
    inline constexpr auto INSTANCE_SCOPE = COMMON_SCOPE + 0x1u;

    template<typename T>
    struct have_layers_and_extensions : T {

        constexpr have_layers_and_extensions(auto&& infos)
            : T{ forward_(infos) }
        {
        }

        constexpr void append_layers(::std::span<const char* const> span) {
            append_chars(layers, span);
        }
        constexpr void append_extensions(::std::span<const char* const> span) {
            append_chars(extensions, span);
        }

        // fuck vulkan.
        ::std::vector<const char*> layers;
        ::std::vector<const char*> extensions;

    private:
        constexpr void append_chars(auto& vec, auto span) {
            for (auto str : span) {
                bool duplicate = false;
                for (auto dstr : vec) {
                    if (::std::string_view{ dstr } == ::std::string_view{ str }) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    vec.emplace_back(str);
                }
            }
        }
    };

    struct instance_layers_getter {
        template<typename T>
        constexpr decltype(auto) operator()(T const& v) const noexcept {
            if constexpr (requires { meta_of<T>::instance_layers; })
                return meta_of<T>::instance_layers;
            else if constexpr (requires { meta_of<T>::instance_layers(); })
                return meta_of<T>::instance_layers();
            else if constexpr (requires { meta_of<T>::instance_layers(v); })
                return meta_of<T>::instance_layers(v);
        }
    };
    struct instance_extensions_getter {
        template<typename T>
        constexpr decltype(auto) operator()(T const& v) const noexcept {
            if constexpr (requires { meta_of<T>::instance_extensions; })
                return meta_of<T>::instance_extensions;
            else if constexpr (requires { meta_of<T>::instance_extensions(); })
                return meta_of<T>::instance_extensions();
            else if constexpr (requires { meta_of<T>::instance_extensions(v); })
                return meta_of<T>::instance_extensions(v);
        }
    };
    template<> struct meta_of<instance> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0u);
        static constexpr auto name = fixed_string{ "instance" };

        using order = order::at_middle;
        using extend = void;

        template<typename T>
        struct info : have_layers_and_extensions<T> {
            constexpr info(instance const& ins, auto&& infos)
                : have_layers_and_extensions<T>{ forward_(infos) }
                , app_info{
                    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                    .pApplicationName = ins.name,
                    .applicationVersion = ins.version,
                    .pEngineName = "VKTL@MaoQwe",
                    .engineVersion = ::std::uint32_t('pre0'),
                }
                , ins_info{
                    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                } {
                auto version = max_api_version<0>(infos, VK_API_VERSION_1_0);
                app_info.apiVersion = version;

                auto lays = enumerate_chars<instance_layers_getter>(infos);
                this->layers.reserve(lays.size());
                for (auto slay : ::std::move(lays))
                    this->layers.emplace_back(slay);
                auto exts = enumerate_chars<instance_extensions_getter>(infos);
                this->extensions.reserve(exts.size());
                for (auto sext : ::std::move(exts))
                    this->extensions.emplace_back(sext);
            }
            constexpr info(auto&& infos) : info{ get_by<instance>(forward_(infos)), forward_(infos) } {}

            void relocate() {
                T::relocate();
                ins_info.pApplicationInfo = &app_info;
                ins_info.enabledExtensionCount = ::std::uint32_t(this->extensions.size());
                ins_info.enabledLayerCount = ::std::uint32_t(this->layers.size());
                ins_info.ppEnabledExtensionNames = this->extensions.data();
                ins_info.ppEnabledLayerNames = this->layers.data();
            }

            VK_ VkApplicationInfo app_info;
            VK_ VkInstanceCreateInfo ins_info;
        };

        template<typename T>
        struct make : T {
            make(contains<instance> auto&& info, auto&&...infos)
                : T{ forward_(infos)... } {
                VK_ vkCreateInstance(&info.ins_info, T::allocator(), &handle_)
                    | popup{ "[INSTANCE] Create instance failed." };

                reinit_phydvs();
            }

            ~make() { VK_ vkDestroyInstance(this->handle_, T::allocator()); }

            auto num_physical_devices() { return phydvs_.size(); }

            template<typename...Ts>
            auto physical_device(::std::size_t index, Ts...vals) {
                if (phydvs_.empty()) {
                    reinit_phydvs();
                }
                struct physical_device phydv { phydvs_.at(index) };
                return describe(ref{ T::as_this() }, phydv, ::std::move(vals)...);
            }

            bool handle_error(VK_ VkResult error) {
                switch (error) {
                case VK_ERROR_DEVICE_LOST:
                    reinit_phydvs();
                    break;
                default:
                    break;
                }
                return false;
            }

            auto instance_handle() const noexcept { return handle_.value; }

        private:
            void reinit_phydvs() {
                invoke(phydvs_, VK_ vkEnumeratePhysicalDevices, handle_)
                    | popup{ "[INSTANCE] Enumerate physical device failed." };
            }

        private:
            ::std::vector<VK_ VkPhysicalDevice> phydvs_;
            move_only<VK_ VkInstance> handle_{ VK_NULL_HANDLE };
        };
    };

#define ins_fn_(name, instance) ((PFN_##name)VK_ vkGetInstanceProcAddr(instance, #name))

    using namespace instance_extensions;

    template<> struct meta_of<debug_utils> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0x1u);
        static constexpr auto name = fixed_string{ "debug_utils" };
        using order = order::at_middle;
        using extend = instance;

        static constexpr auto instance_extensions = ::std::array{
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        };

        template<typename T>
        struct info : T {
            constexpr info(debug_utils const& utils, auto const& infos)
                : T{ infos }
                , debug_messenger{
                    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                } {
            }
            constexpr info(auto const& infos) : info{ get_by<debug_utils>(infos), infos } {}

            VK_ VkDebugUtilsMessengerCreateInfoEXT debug_messenger;
        };

        template<typename T>
        struct make : T {
            make(contains<instance, debug_utils> auto const& infos) : T{ infos } {
                auto fn = ins_fn_(vkCreateDebugUtilsMessengerEXT, T::handle());
                if (!fn) {
                    throw error{ VK_ERROR_FEATURE_NOT_PRESENT, "[INSTANCE DEBUG_UTILS] Cannot find create debug utils function." };
                }
                fn(T::handle(), &infos.debug_messenger, T::allocator(), &messager_)
                    | popup{ "[INSTANCE DEBUG_UTILS] create debug messagner failed." };
            }

            ~make() {
                ins_fn_(vkDestroyDebugUtilsMessengerEXT, T::handle())(T::handle(), messager_, T::allocator());
            }

        private:
            move_only<VK_ VkDebugUtilsMessengerEXT> messager_{ VK_NULL_HANDLE };
        };
    };

    template<>
    struct meta_of<validation_layer> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0x2u);
        static constexpr auto name = fixed_string{ "validation_layers" };
        using order = order::discard;
        using extend = instance;

        static constexpr auto instance_layers = ::std::array{
            "VK_LAYER_KHRONOS_validation"
        };

        template<typename T>
        using info = T;

        template<typename T>
        using make = T;
    };


    // PHY DEVICE


    using namespace physical_device_extensions;
    template<> struct meta_of<physical_device> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0x10000);
        static constexpr auto name = fixed_string{ "physical_device" };

        using order = order::at_last;
        using extend = void;

        using handle_type = VK_ VkPhysicalDevice;

        template<typename T>
        struct info : T {
            constexpr info(auto const& infos)
                : T{ infos }, handle_{ get_by<physical_device>(infos).handle } {
            }

            template<typename P, typename...PExts>
            constexpr auto get_property(P main_props, PExts...properties_extensions) {
                if constexpr (sizeof...(PExts) > 0u) {
                    return describe(ref{ this->as_this() },
                        ::std::move(properties_extensions)..., ::std::move(main_props),
                        inherit_from<physical_device_properties::property_enable>{});
                }
                else {
                    return describe(ref{ this->as_this() },
                        ::std::move(properties_extensions)..., ::std::move(main_props),
                        inherit_from<physical_device_properties::property_disable>{});
                }
            }

            auto handle() const noexcept { return handle_; }

        protected:
            handle_type handle_ = nullptr;
        };
    };

    inline constexpr struct phydv_property_ {
        template<typename Type, typename Func, typename Func2, typename...Args>
        static constexpr auto invoke(auto pthis, Type& result, Func func1, Func2 func2, Args...args) noexcept {
            if constexpr (::std::ranges::range<Type>) {
                if constexpr (::std::same_as<typename trait_vkfn<Func>::result_type, ::std::ranges::range_value_t<Type>>) {
                    return invoke_dispatch(pthis, result, func1, ::std::move(args)...);
                }
                else {
                    return invoke_dispatch(pthis, result, func2, ::std::move(args)...);
                }
            }
            else {
                if constexpr (::std::same_as<typename trait_vkfn<Func>::result_type, Type>) {
                    return invoke_dispatch(pthis, result, func1, ::std::move(args)...);
                }
                else {
                    return invoke_dispatch(pthis, result, func2, ::std::move(args)...);
                }
            }
        }

        template<typename Type, typename Func, typename...Args>
        static constexpr auto invoke_dispatch(auto pthis, Type& result, Func func, Args...args) noexcept {
            VK_ VkResult r = VK_ VK_SUCCESS;
            auto hphydv = pthis->parent()->handle();
            if constexpr (::std::ranges::range<Type>) {
                using rt = ::std::invoke_result_t<Func, decltype(hphydv), Args..., ::std::uint32_t*, ::std::ranges::range_value_t<Type>*>;
                ::std::uint32_t count;
                if constexpr (::std::same_as<rt, VK_ VkResult>) {
                    r = func(hphydv, args..., &count, nullptr);
                }
                else {
                    func(hphydv, args..., &count, nullptr);
                }

                if (r != VK_ VK_SUCCESS)
                    return r;

                result.resize(count);
                pthis->connect(result);

                if constexpr (::std::same_as<rt, VK_ VkResult>)
                    r = func(hphydv, args..., &count, result.data());
                else
                    func(hphydv, args..., &count, result.data());
            }
            else {
                pthis->connect(result);
                if constexpr (::std::same_as<
                    ::std::invoke_result_t<Func, decltype(hphydv), Args..., Type*>
                    , VK_ VkResult>) {
                    r = func(hphydv, args..., &result);
                }
                else {
                    func(hphydv, args..., &result);
                }

            }
            return r;
        }

        // connect prev's pNext to &next.
        // since it need to resize if is range, def_val must not end. (for initialize sType or other handle).
        template<typename Src, typename Dst, typename Default = ::std::nullptr_t>
        static constexpr auto connect(Src& prev, Dst& next, Default def_val = nullptr) {
            if constexpr (::std::ranges::range<Src> && ::std::ranges::range<Dst>) {
                static_assert(!::std::is_null_pointer_v<Default>, "range must specified default value.");
                next.resize(prev.size(), def_val);
                if constexpr (requires { ::std::ranges::range_value_t<Src>().pNext;  }) {
                    auto ptr = next.data();
                    for (auto& val : prev) { val.pNext = ptr++; }
                }
            }
            else if constexpr (!::std::ranges::range<Src> && !::std::ranges::range<Dst>) {
                if constexpr (requires { prev.pNext = &next; }) {
                    prev.pNext = &next;
                }
            }
            else { static_assert(always_false<Src>, "Physical device properties cannot be connected."); }
        }
    } phydv_property{};

    template<typename T>
    struct basic_phydv_property : T {
        constexpr auto const& operator[](::std::size_t index) const noexcept
            requires(requires{ T::value()[index]; }) {
            return T::value()[index];
        }
    };

    using namespace physical_device_properties;

    template<> struct meta_of<queue_family_properties> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0x11000);
        static constexpr auto name = fixed_string{ "queue_family_properties" };

        using order = order::at_last;
        using extend = void;

        template<typename T>
        struct detail : T {
            using structure = ::std::conditional_t<T::enhance, VK_ VkQueueFamilyProperties2, VK_ VkQueueFamilyProperties>;

            detail(auto const& infos) : T{ infos } {
                phydv_property.invoke(this, this,
                    queue_family_properties,
                    &VK_ vkGetPhysicalDeviceQueueFamilyProperties,
                    &VK_ vkGetPhysicalDeviceQueueFamilyProperties2);
            }

            constexpr auto& value() const noexcept { return queue_family_properties; }

            ::std::vector<structure> queue_family_properties;
        };
        template<typename T>
        using info = basic_phydv_property<detail<T>>;
    };

    using namespace queue_family_properties_extensions;

    template<> struct meta_of<queue_global_priorites> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0x11001);
        static constexpr auto name = fixed_string{ "queue_global_priorites" };
        using extend = queue_family_properties;
        using structure = VK_ VkQueueFamilyGlobalPriorityPropertiesKHR;

        template<typename T> struct info : T {
            template<typename C>
            void connect(::std::vector<C>& value) {
                phydv_property.connect(value, queue_global_priorites, structure{
                    .sType = VK_ VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES,
                    });
                T::connect(queue_global_priorites);
            }

            constexpr auto& value() const noexcept { return queue_global_priorites; }

            ::std::vector<structure> queue_global_priorites;
        };
    };

    template<> struct meta_of<surface_present_mode> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0x12000);
        static constexpr auto name = fixed_string{ "surface_present_mode" };
        using order = order::at_last;
        using extend = void;

        template<typename T>
        struct detail : T {
            using structure = VK_ VkPresentModeKHR;

            detail(auto const& infos) : T{ infos } {
                auto surface = get_by<surface_present_mode>(infos).surface;
                assert(surface);
                // if constexpr (T::enhance) {
                // 	VK_ VkPhysicalDeviceSurfaceInfo2KHR info{
                // 		.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
                // 		.surface = surface,
                // 	};
                // 	T::connect(info);
                // 	phydv_property.invoke(this, surface_present_modes, VK_ vkGetPhysicalDeviceSurfacePresentModes2EXT, nullptr, &info)
                // 		| popup("[PHYSICAL-DEVICE] Get surface present mode failure.");
                // }
                // else {
                phydv_property.invoke(this, surface_present_modes, VK_ vkGetPhysicalDeviceSurfacePresentModesKHR, nullptr, surface)
                    | popup("[PHYSICAL-DEVICE] Get surface present mode failure.");
                // }
            }

            constexpr auto& value() const noexcept { return surface_present_modes; }

            ::std::vector<structure> surface_present_modes;
        };

        template<typename T>
        using info = basic_phydv_property<detail<T>>;
    };

    template<> struct meta_of<surface_format> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0x13000);
        static constexpr auto name = fixed_string{ "surface_format" };
        using order = order::at_last;
        using extend = void;

        template<typename T>
        struct detail : T {
            using structure = ::std::conditional_t<T::enhance, VK_ VkSurfaceFormat2KHR, VK_ VkSurfaceFormatKHR>;

            detail(auto const& infos) : T{ infos } {
                auto surface = get_by<surface_format>(infos).surface;
                assert(surface);
                if constexpr (T::enhance) {
                    VK_ VkPhysicalDeviceSurfaceInfo2KHR surface_info{
                        .sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
                        .surface = surface,
                    };
                    T::connect(surface_info);
                    phydv_property.invoke(this, surface_formats, &VK_ vkGetPhysicalDeviceSurfaceFormats2KHR, nullptr, surface_info);
                }
                else {
                    phydv_property.invoke(this, surface_formats, &VK_ vkGetPhysicalDeviceSurfaceFormatsKHR, nullptr, surface);
                }
            }

            constexpr auto& value() const noexcept { return surface_formats; }

            ::std::vector<structure> surface_formats;
        };

        template<typename T>
        using info = basic_phydv_property<detail<T>>;
    };


    template<>
    struct meta_of<memory_properties> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0x14000);
        static constexpr auto name = fixed_string{ "memory_properties" };

        using order = order::at_middle;
        using extend = void;

        template<typename T>
        struct info : T {
            using structure = ::std::conditional_t<T::enhance, VK_ VkPhysicalDeviceMemoryProperties2, VK_ VkPhysicalDeviceMemoryProperties>;

            info(auto const& infos) : T{ infos } {
                if constexpr (T::enhance) {
                    memory_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                    T::connect(memory_properties);
                }
                phydv_property.invoke(this, memory_properties
                    , &VK_ vkGetPhysicalDeviceMemoryProperties
                    , &VK_ vkGetPhysicalDeviceMemoryProperties2);
            }

            constexpr auto& value() const noexcept { return memory_properties; }

            structure memory_properties{};
        };
    };

    using namespace memory_properties_extensions;

    template<>
    struct meta_of<memory_budget_properties> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0x14001);
        static constexpr auto name = fixed_string{ "memory_budget_properties" };

        using order = order::at_middle;
        using extend = memory_properties;

        template<typename T>
        struct info : T {
            using structure = VK_ VkPhysicalDeviceMemoryBudgetPropertiesEXT;

            template<typename O>
            void connect(O& value) {
                phydv_property.connect(value, memory_budget_properties);
            }

            structure memory_budget_properties{
                .sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
            };
        };
    };

    template<>
    struct meta_of<physical_device_properties::properties> {
        static constexpr auto type_id = make_type_id(INSTANCE_SCOPE, 0x15000);
        static constexpr auto name = fixed_string{ "properties" };

        using order = order::at_middle;
        using extend = void;

        template<typename T>
        struct info : T {
            using structure = ::std::conditional_t<T::enhance, VK_ VkPhysicalDeviceProperties2, VK_ VkPhysicalDeviceProperties>;

            constexpr info(auto const& infos) : T{ infos } {
                if constexpr (T::enhance) {
                    properties.sType = VK_ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                    T::connect(properties);
                }
                phydv_property.invoke(this, properties
                    , VK_ vkGetPhysicalDeviceProperties
                    , VK_ vkGetPhysicalDeviceProperties2);
            }

            constexpr auto& value() const noexcept { return properties; }

            structure properties;
        };
    };
}

VKTL_EXPORT_ namespace vktl::detail {
    inline constexpr auto DEVICE_SCOPE = INSTANCE_SCOPE + 0x1u;


    using namespace device_extensions;
    struct device_layers_getter {
        template<typename T>
        constexpr decltype(auto) operator()(T const& v) const noexcept {
            if constexpr (requires { meta_of<T>::device_layers; }) return meta_of<T>::device_layers;
            else if constexpr (requires { meta_of<T>::device_layers(); }) return meta_of<T>::device_layers();
            else if constexpr (requires { meta_of<T>::device_layers(v); }) return meta_of<T>::device_layers(v);
        }
    };
    struct device_extensions_getter {
        template<typename T>
        constexpr decltype(auto) operator()(T const& v) const noexcept {
            if constexpr (requires { meta_of<T>::device_extensions; }) return meta_of<T>::device_extensions;
            else if constexpr (requires { meta_of<T>::device_extensions(); }) return meta_of<T>::device_extensions();
            else if constexpr (requires { meta_of<T>::device_extensions(v); }) return meta_of<T>::device_extensions(v);
        }
    };
    template<> struct meta_of<device> {
        static constexpr auto type_id = make_type_id(DEVICE_SCOPE, 0);
        static constexpr auto name = fixed_string{ "device" };
        using order = order::at_middle;
        using extend = void;

        template<typename T>
        struct info : have_layers_and_extensions<T> {
            constexpr info(device const& dev, auto const& infos)
                : have_layers_and_extensions<T>{ infos }
                , dev_index{ dev.index }
                , device_info{
                    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                } {
                auto lays = enumerate_chars<device_layers_getter>(infos);
                this->layers.reserve(lays.size());
                for (auto slay : ::std::move(lays)) {
                    this->layers.emplace_back(slay);
                }

                auto exts = enumerate_chars<device_extensions_getter>(infos);
                this->extensions.reserve(exts.size());
                for (auto sext : ::std::move(exts)) {
                    this->extensions.emplace_back(sext);
                }
            }
            constexpr info(auto const& infos) : info{ get_by<device>(infos), infos } {}

            constexpr void relocate() {
                device_info.queueCreateInfoCount = ::std::uint32_t(queue_infos.size());
                device_info.pQueueCreateInfos = queue_infos.data();
                device_info.enabledExtensionCount = ::std::uint32_t(this->extensions.size());
                device_info.ppEnabledExtensionNames = this->extensions.data();
                device_info.enabledLayerCount = ::std::uint32_t(this->layers.size());
                device_info.ppEnabledLayerNames = this->layers.data();

                auto it = queue_infos.begin();
                auto itr = queue_priorities.begin();
                while (it != queue_infos.end()) {
                    (it++)->pQueuePriorities = (itr++)->data();
                }
            }

            constexpr auto receive(VK_ VkDeviceQueueCreateInfo info) {
                auto itr = queue_priorities.begin();
                auto it = queue_infos.begin();
                while (it != queue_infos.end() && it->queueFamilyIndex < info.queueFamilyIndex) { it++; itr++; }
                if (it == queue_infos.end() || it->queueFamilyIndex != info.queueFamilyIndex) { // need insert.
                    it = queue_infos.insert(it, ::std::move(info));
                    if (info.pQueuePriorities) {
                        queue_priorities.emplace(itr, info.pQueuePriorities, info.pQueuePriorities + info.queueCount);
                    }
                    else {
                        queue_priorities.emplace(itr, info.queueCount, 1.0f);
                    }
                }
                else { // overwrite.
                    it->pNext = info.pNext;
                    it->flags = info.flags;
                    if (info.pQueuePriorities) {
                        *itr = ::std::vector(info.pQueuePriorities, info.pQueuePriorities + info.queueCount);
                    }
                    else {
                        *itr = ::std::vector(info.queueCount, 1.0f);
                    }
                    it->queueCount = info.queueCount;
                    it->pQueuePriorities = itr->data();
                }
                return::std::uint16_t(::std::distance(queue_infos.begin(), it));
            }

            ::std::uint32_t dev_index;
            ::std::vector<VK_ VkDeviceQueueCreateInfo> queue_infos;
            ::std::vector<::std::vector<float>> queue_priorities;
            VK_ VkDeviceCreateInfo device_info;
        };

        template<typename T>
        struct make : T {
            constexpr make(infomation_of<device> auto&& info, auto&&...others)
                : T{ forward_(others)... }
                , device_index_{ info.dev_index } {
                auto phydv = this->physical_device();
                VK_ vkCreateDevice(phydv.handle(), &info.device_info, T::allocator(), &handle_)
                    | popup("[DEVICE] Create logical device failed.");
            }

            ~make() { VK_ vkDestroyDevice(this->handle_, T::allocator()); }

            constexpr auto device_handle() const noexcept { return handle_.value; }

            constexpr auto physical_device(auto...ext) {
                return T::template parent<instance>()->physical_device(device_index_, ::std::move(ext)...);
            }

        private:
            ::std::uint32_t device_index_{};
            move_only<VK_ VkDevice> handle_{ VK_NULL_HANDLE };

            ::std::vector<::std::uint32_t> queue_families_;
        };
    };

#define dv_fn_(name, handle) ((PFN_##name)(VK_ vkGetDeviceProcAddr(handle, #name))

    using namespace queue_family_extensions;
    template<> struct meta_of<queue_family> {
        static constexpr auto type_id = make_type_id(DEVICE_SCOPE, 0x100u);
        static constexpr auto name = fixed_string{ "queue_family" };
        using extend = device;

        template<typename T>
        struct info : T {
            constexpr info(queue_family const& qfm, auto const& infos)
                : T{ infos }
                , queue_info{
                    .sType = VK_ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = qfm.family,
                    .queueCount = qfm.count,
                }
            {
            }
            constexpr info(auto const& infos)
                : info{ get_by<queue_family>(infos), infos }
            {
            }

            void connect(infomation_of<device> auto& dv) {
                if (not_connected) {
                    dv.receive(queue_info);
                }
            }
            void set_connectable() { not_connected = true; }

            VK_ VkDeviceQueueCreateInfo queue_info;
            bool not_connected = true;
        };

        template<typename T>
        using make = skipped_make<T>;
    };


    template<> struct meta_of<queue_priority> {
        static constexpr auto type_id = make_type_id(DEVICE_SCOPE, 0x101u);
        static constexpr auto name = fixed_string{ "queue_priority" };
        using extend = queue_family;

        template<typename T>
        struct info : T {
            constexpr info(queue_priority const& gp, auto const& infos)
                : T{ infos }
                , priorities(gp.priorities.begin(), gp.priorities.end())
            {
                relocate();
            }
            constexpr info(auto const& infos)
                : info(get_by<queue_priority>(infos), infos)
            {
            }

            void relocate() {
                T::relocate();
                T::queue_info.pQueuePriorities = priorities.data();
            }

            ::std::vector<float> priorities;
        };
    };
}


// BEGIN HEAP

// VKTL_EXPORT_ namespace vktl::detail {
//     inline constexpr auto HEAP_SCOPE = BIND_POINTS_SCOPE + 0x1u;
// 
//     template<> struct meta_of<memory_heap> {
//         static constexpr auto type_id = make_type_id(HEAP_SCOPE, 0);
//         static constexpr auto name = fixed_string{ "memory_heap" };
//         using order = order::at_middle;
//         using extend = void;
// 
//         template<typename T>
//         struct info : T {
//             static_assert(always_false<T>, "Memory heap is not finished yet.");
// 
//             constexpr info(auto const& info)
//                 : T{ info }
//                 , metrics{ get_by<memory_heap>(info) }
//             {
//             }
// 
//             void connect(object_of<device> auto& dv) {
//                 T::as_self().get_memory_properties(dv);
//             }
// 
//             memory_heap metrics;
//             ::std::vector<::std::uint32_t> candidate_indices;
// 
//         protected:
//             constexpr auto get_memory_properties(auto& dv, auto...ext) {
//                 return dv.physical_device().get_property(memory_properties{}, ext...);
//             }
//         };
// 
//         template<typename T>
//         struct make : T {
// 
//             constexpr make(infomation_of<memory_heap> auto&& info, auto&&...other)
//                 : T{ forward_(other)... }
//                 , metrics{ info.metrics } {
// 
//             }
// 
//         private:
//             memory_heap metrics;
//         };
//     };
// }

#if !defined(VK_NO_WINDOWS)

#pragma region NOBODY LIKE WIN32
#	if defined(_WIN32) 
#	define WIN32_LEAN_AND_MEAN
#	define NOGDICAPMASKS
#	define NOVIRTUALKEYCODES
//#	define NOWINMESSAGES
//#	define NOWINSTYLES
#	define NOSYSMETRICS
//#	define NOMENUS
//#	define NOICONS
//#	define NOKEYSTATES
#	define NOSYSCOMMANDS
#	define NORASTEROPS
//#	define NOSHOWWINDOW
#	define OEMRESOURCE
//#	define NOATOM
#	define NOCLIPBOARD
#	define NOCOLOR
#	define NOCTLMGR
#	define NODRAWTEXT
#	define NOGDI
//#	define NOKERNEL
//#	define NOUSER
#	define NONLS
#	define NOMB
#	define NOMEMMGR
#	define NOMETAFILE
#	define NOMINMAX
//#	define NOMSG
#	define NOOPENFILE
#	define NOSCROLL
#	define NOSERVICE
#	define NOSOUND
#	define NOTEXTMETRIC
//#	define NOWH // window hook.
//#	define NOWINOFFSETS
#	define NOCOMM
#	define NOKANJI
#	define NOHELP
#	define NOPROFILER
#	define NODEFERWINDOWPOS
#	define NOMCX
namespace VK_NAMESPACE {
#	include <Windows.h>
#	include <vulkan/vulkan_win32.h>
}
#	undef WIN32_LEAN_AND_MEAN
#	undef NOGDICAPMASKS
#	undef NOVIRTUALKEYCODES
#	undef NOWINMESSAGES
#	undef NOWINSTYLES
#	undef NOSYSMETRICS
#	undef NOMENUS
#	undef NOICONS
#	undef NOKEYSTATES
#	undef NOSYSCOMMANDS
#	undef NORASTEROPS
#	undef NOSHOWWINDOW
#	undef OEMRESOURCE
#	undef NOATOM
#	undef NOCLIPBOARD
#	undef NOCOLOR
#	undef NOCTLMGR
#	undef NODRAWTEXT
#	undef NOGDI
#	undef NOKERNEL
#	undef NOUSER
#	undef NONLS
#	undef NOMB
#	undef NOMEMMGR
#	undef NOMETAFILE
#	undef NOMINMAX
#	undef NOMSG
#	undef NOOPENFILE
#	undef NOSCROLL
#	undef NOSERVICE
#	undef NOSOUND
#	undef NOTEXTMETRIC
#	undef NOWH
#	undef NOWINOFFSETS
#	undef NOCOMM
#	undef NOKANJI
#	undef NOHELP
#	undef NOPROFILER
#	undef NODEFERWINDOWPOS
#	undef NOMCX
#pragma endregion

// WIN32 GRAPHICS

VKTL_EXPORT_ namespace vktl::detail {
    static constexpr auto SURFACE_SCOPE = INSTANCE_SCOPE;

    using namespace windows_extensions;
    template<>
    struct meta_of<window> {
        static constexpr auto type_id = make_type_id(SURFACE_SCOPE, 0x10000u);
        static constexpr auto name = fixed_string{ "window" };
        using extend = void;
        using order = order::at_middle;

        static constexpr auto instance_extensions = ::std::array{
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME
        };

        template<typename T>
        struct info : T {
            info(auto const& infos)
                : T{ infos }
                , surface{
                    .sType = VK_ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                    .hinstance = VK_ GetModuleHandle(nullptr),
                    .hwnd = static_cast<VK_ HWND>(get_by<window>(infos).handle),
                }
            {
            }

            void connect(contains<instance> auto& info) {
                info.append_extensions(instance_extensions);
            }

            VK_ VkWin32SurfaceCreateInfoKHR surface;
        };

        template<typename N>
        struct make : N {
            make(auto&& info, auto&&...others) : N{ forward_(others)... } {
                assert(info.surface.hwnd && info.surface.hinstance);
                VK_ vkCreateWin32SurfaceKHR(N::template parent<instance>()->instance_handle(),
                    &info.surface, N::allocator(), &surface_.value)
                    | popup("[WINDOW] Create surface failure.");

                VK_ RECT rt;
                VK_ GetClientRect(info.surface.hwnd, &rt);
                width_ = ::std::uint32_t(rt.right - rt.left);
                height_ = ::std::uint32_t(rt.bottom - rt.top);
            }

            ~make() { VK_ vkDestroySurfaceKHR(N::template parent<instance>()->instance_handle(), surface_, N::allocator()); }

            constexpr auto surface_handle() const noexcept { return surface_.value; }

            constexpr auto width() const noexcept { return width_; }
            constexpr auto height() const noexcept { return height_; }

        protected:
            move_only<VK_ VkSurfaceKHR> surface_;
            ::std::uint32_t width_, height_;
        };
    };
}

#	else
#		error "Other platform's swapchain component is not finished."
#	endif
#endif

