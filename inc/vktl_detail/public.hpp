#pragma once

// --------------PUBLIC SCOPE---------------
// this scope should not contain vulkan objects.
// it should consider as RHI like interfaces.
// BEFORE EDIT: 
// 1. DO NOT add comment that not related with functionality, or mechiasm explain.
// 2. Prefer english comment.

#if defined(VKTL_EXPORT_MODULE)
#	define VKTL_EXPORT_ export 
#else
#	define VKTL_EXPORT_
#endif


#if !defined(VKTL_NO_STD)
#   define VKTL_HAVE_STD_ 1
#else
#   define VKTL_HAVE_STD_ 0
#endif

#if VKTL_HAVE_STD_
#include <span>
#include <string_view>
#endif


// meta objects.

VKTL_EXPORT_ namespace vktl::detail {
    inline constexpr struct invalid_ {
        template<typename T>
        static constexpr auto value = ~T(0u);

        // fuck c++.
        template<::std::unsigned_integral T>
        constexpr operator T() const noexcept { return ~T(0u); }
        // fuck edg dont know const value type -> value type.
        template<::std::unsigned_integral T>
        friend constexpr bool operator==(T left, const invalid_) noexcept { return left == value<T>; }
        template<::std::unsigned_integral T>
        friend constexpr bool operator!=(T left, const invalid_) noexcept { return left != value<T>; }
        template<::std::unsigned_integral T>
        friend constexpr bool operator==(const invalid_, T left) noexcept { return left == value<T>; }
        template<::std::unsigned_integral T>
        friend constexpr bool operator!=(const invalid_, T left) noexcept { return left != value<T>; }
    } invalid{}, maximum{};

    // template<typename T, typename...Rs>
    // constexpr auto make_compose(T&& host, Rs&&...exts) {
    // 	return affine{ static_cast<T&&>(host), static_cast<Rs&&>(exts)... };
    // }

    struct byte_view : ::std::span<::std::byte> {
        byte_view() = default;
        byte_view(void* bytes, ::std::size_t size)
            : ::std::span<::std::byte>{ static_cast<::std::byte*>(bytes), size }
        {}
    };

}

// BEFORE EDIT: 
// 1. extensions of `xxx` should inside namspace `xxx_extensions`
VKTL_EXPORT_ namespace vktl {
    using detail::invalid;
    using detail::maximum;

    struct error {
        int code; // error code.
        const char* message; // ansi zero end string.
    };

    namespace extensions {
        // use for internal api's handle allocation.
        // not for gpu memory allocation.
        struct allocator {
            struct interface {
                virtual ~interface() = default;

                virtual void* allocate(::std::size_t size, ::std::size_t alignment) = 0;
                virtual void* reallocate(void* original, ::std::size_t size, ::std::size_t alignment) = 0;
                virtual void free(void* memory) = 0;

                virtual void internal_allocate(::std::size_t size, const char*) {}
                virtual void internal_free(::std::size_t size, const char*) {}
            } *view{ nullptr } // take as view.
            , *move{ nullptr }; // take ownership of the allocator.
        };

        // some of the object must contain at least one of these object.
        // this describe task's type.

        struct compute {};
        struct graphics {};
        struct transfer {};
        struct present {};

        // for instance, it might use GET_PHYSICAL_DEVICE_2 and so on.
        // required Vulkan 1.1.
        // struct version1_1 {};

        // debug named will also enable in release build.
        // use it is convenient for gpu debug.
        struct debug_named {
            ::std::string_view name;
        };

        // WARNING: currently not supported.
        struct named {
            ::std::string_view name;
        };

        // repeat cannot not use at every object.
        struct repeat {
            ::std::size_t count;
        };

        struct is_static {};

        //struct split {};
    }
    namespace instance_extensions {
        struct debug_utils {};

        struct monitor {};
        struct enhanced_getters {};

        struct validation_layer {};
    }
    struct instance {
        const char* name{ "app-name-here:3" }; // app name.
        ::std::uint32_t version{ 1 }; // app version.
    };

    namespace windows_extensions {
        using extensions::debug_named;
    }
    struct window {
        void* handle;
    };

    namespace device_extensions {
        using extensions::debug_named;
        struct enbale_budget {};
    }
    struct device {
        ::std::uint16_t index; // index of device.
    };

    namespace queue_family_extensions {
        using extensions::debug_named;
        struct queue_priority { // not queue-family-global-priorities.
            ::std::span<float> priorities;
        };
    }
    struct queue_family {
        ::std::uint16_t family = 0u;
        ::std::uint16_t count = 1u;
    };

    // BEGIN EXECUTION.

    namespace queue_duty {
        using type = ::std::uint16_t;
        inline constexpr type compute = 0x1 << 0;
        inline constexpr type transfer = 0x1 << 1;
        inline constexpr type graphics = 0x1 << 2;
        inline constexpr type present = 0x1 << 3;
    }
    namespace execution_extensions {
        using extensions::debug_named;
        using extensions::allocator;
    }
    // execution is thread related.
    // each queue will represent a thread.
    struct execution {
        // required, must specified thread count.
        ::std::uint16_t thread_count;
    };

    namespace semaphore_extensions {
        using extensions::debug_named;
        using extensions::repeat;
    }
    struct semaphore {};

    namespace event_extensions {
        using extensions::debug_named;
        using extensions::repeat;
    }
    struct event {};

    namespace fence_extensions {
        using extensions::debug_named;
        using extensions::repeat;
    }
    struct fence {};

    namespace queue_extensions {
        using extensions::debug_named;
    }
    struct queue {
        queue_duty::type duty;
        // index of `device::queues`.
        ::std::uint16_t family_index = 0u;
        ::std::uint16_t index = 0u;
    };

    namespace task_extensions {
        using extensions::debug_named;

        // in vulkan, free command buffer is an extensions.
        // extend this from task will allow free command buffer.
        struct allow_temporary_command_buffers {};
    }
    struct task {
        // queue inside execution.
        ::std::uint16_t queue_index;
        ::std::uint16_t thread_index;

        // index of fence will emit when submit to gpu.
        ::std::uint16_t fence_index = invalid;
    };

    namespace split_extensions {
        using extensions::debug_named;
    }
    struct split {
    };

    // WANRING: GPU side event is not finished yet.

    // gpu/cpu side wait event.
    struct reset_event {
        ::std::uint32_t index;
        // only gpu side can wait.
        bool wait = false;
    };
    // gpu/cpu side set event.
    struct set_event {
        ::std::uint32_t index;
    };

    // BEGIN RESOURCE

    namespace bind_point_type {
        using type = ::std::uint16_t;
        inline constexpr type buffer = 0x1u;
        inline constexpr type image = buffer + 0x1u;
        inline constexpr type canvas = image + 0x1u;
        inline constexpr type sampler = canvas + 0x1u;
        inline constexpr type static_sampler = sampler + 0x1u;
    }
    namespace texture_bits_type {
        using type = ::std::uint16_t;
        inline constexpr type undefined = 0x0u;

        inline constexpr type unorm = 0x1u;
        inline constexpr type uint = unorm + 0x1;

        inline constexpr type snorm = 0x100u;
        inline constexpr type sint = snorm + 0x1;
        inline constexpr type sfloat = snorm + 0x3;
    }
    namespace image_bits_type = texture_bits_type;

    namespace texture_order {
        using type = ::std::uint16_t;

        inline constexpr type rgba = 0x1;
        inline constexpr type rbga = 0x2;
        inline constexpr type bgra = 0x3;
        inline constexpr type abgr = 0x4;
    }
    namespace image_order = texture_order;

    namespace image_format {

        // can mark -1 to disable corresponding value.

        struct format_color {
            ::std::uint16_t order = image_order::rgba;

            // atleast one of the value.

            ::std::uint16_t rbits = ::std::uint16_t(-1);
            ::std::uint16_t gbits = ::std::uint16_t(-1);
            ::std::uint16_t bbits = ::std::uint16_t(-1);
            ::std::uint16_t abits = ::std::uint16_t(-1);

            ::std::uint16_t rtype = texture_bits_type::undefined;
            ::std::uint16_t gtype = texture_bits_type::undefined;
            ::std::uint16_t btype = texture_bits_type::undefined;
            ::std::uint16_t atype = texture_bits_type::undefined;
        };

        struct format_depth {
            ::std::uint16_t order = 0u;

            ::std::uint16_t dbits = ::std::uint16_t(-1);
            ::std::uint16_t dtype = ::std::uint16_t(-1);

            ::std::uint16_t sbits = ::std::uint16_t(-1);
            ::std::uint16_t stype = ::std::uint16_t(-1);

            ::std::uint16_t xbits = ::std::uint16_t(-1);
            ::std::uint16_t xtype = ::std::uint16_t(-1);
        };
    }

    namespace resource_attrs {
        using type = ::std::uint64_t;

        inline constexpr auto is_static = type(0x1u) << 32;
        inline constexpr auto global_frame = type(0x1u) << 33;
        inline constexpr auto device_local = type(0x1u) << 34;
        inline constexpr auto allow_alias = type(0x1u) << 35;
        inline constexpr auto sparse = type(0x1u) << 36;
    }

    namespace resource_extensions {
        using extensions::is_static;
        using extensions::repeat;
        using extensions::named;

        struct sparse {};
        // only image can use this value.
        struct allow_alias {};
    }

    namespace buffer_extensions {
        using namespace resource_extensions;
    }
    // create buffer with specified size.
    struct buffer {
        ::std::size_t size;
    };

    namespace image_extensions {
        using namespace resource_extensions;
        using namespace image_format;

        // by default, the library will set to 1.
        struct image_miplevel {
            ::std::uint16_t count = 1u;
        };

        // by default, the library will set to 1.
        struct image_array_layers {
            ::std::uint16_t count = 1u;
        };
    }
    // it must atleast combines `color` or `depth` and other customized format descriptor.
    typedef struct image {
        ::std::uint32_t width; // required.
        ::std::uint32_t height = maximum; // invalid means the image is image 1D, and depth will be ignored.
        ::std::uint16_t depth = maximum; // invalid means the image is image 2D.
    } texture;


    namespace sampler_common {
        using type = ::std::uint32_t;
        inline constexpr auto black_background = 0x1u;
    }
    namespace sampler_extensions {
        using resource_extensions::repeat;
    }
    struct sampler {
        ::std::uint32_t type; // value in namespace sampler type.
    };

    // namespace allocation_strategy {
    // 
    // }

    namespace swapchain_extensions {
        using extensions::debug_named;
        using extensions::named;
    }
    struct swapchain {
        ::std::uint16_t min_frame_count;
        ::std::uint16_t fence_index = invalid;
        ::std::uint16_t semaphore_index = invalid;
        // if width and height are maximum, then get_by size from window.
        ::std::uint32_t width = maximum;
        ::std::uint32_t height = maximum;
    };

    namespace bind_points_extensions {
        using extensions::allocator;
        using extensions::debug_named;

        // no memory alias or sparse binding will allow to be performed.
        struct bind_static {};

        template<typename Stratgy>
        struct memory_allocator {
            ::std::span<::std::uint16_t> memory_indices;
        };
        template<typename Stratgy>
        struct descriptor_allocator {};

        struct bind_dynamic {};
    }
    struct bind_points {
        // if parent contains swapchain, this value will be ignored.
        ::std::uint16_t frame_count = invalid;
    };

    namespace view_usage {
        using resource_attrs::type;

        inline constexpr type uniform = 0x1u;
        inline constexpr type copy_src = 0x1u << 2;
        inline constexpr type copy_dst = 0x1u << 3;

        // some image or might need to mark read or write to identify more access usage.
        // color and depth stencil are default by write operation, might need to combine read to identify as read operation.

        inline constexpr type shader_read = 0x1u << 4;
        inline constexpr type gpu_read = shader_read;

        inline constexpr type shader_write = 0x1u << 5;
        inline constexpr type gpu_write = shader_write;

        // both for buffer and image, image will view as image2D (no sampler).
        inline constexpr type texel = 0x1u << 6;

        // these values are not recommand to use this value on image,
        // since only on uma structure might supported.

        inline constexpr type readback = 0x1u << 7;
        inline constexpr type cpu_read = readback;
        inline constexpr type upload = 0x1u << 8;
        inline constexpr type cpu_write = upload;

        // buffer only.

        inline constexpr type vertex = 0x1u << 9;
        inline constexpr type index = 0x1u << 10; // index buffer.
        inline constexpr type indirect = 0x1u << 11; // indirect command buffer.

        // image only.

        // this for internal usage. if need to speicified sampler, use image_view_extensions::sampled instead.
        inline constexpr type sampled = 0x1u << 9;

        inline constexpr type depth = 0x1u << 10;
        inline constexpr type render_target = 0x1u << 11;
        inline constexpr type color = render_target;
        inline constexpr type input = 0x1u << 12; // input attachment.
        inline constexpr type stencil = 0x1u << 13;
        // only enabled if the extension `attachment` inside image_view.
        inline constexpr type resolve = copy_dst;
    }

    // bindings.

    namespace resource_view_extensions {
        using extensions::repeat;
        using extensions::is_static;

        struct binding {
            ::std::uint16_t set;
            ::std::uint16_t binding = invalid;
        };
    }

    namespace buffer_view_extensions {
        using namespace resource_view_extensions;
        using extensions::debug_named;
        using extensions::is_static;
        using image_format::format_color; // for texel buffer.

        struct buffer_range {
            ::std::size_t offset;
            ::std::size_t size;
        };
    }
    // if no buffer range, then view the whole buffer as this view.
    struct buffer_view {
        ::std::uint16_t index; // index of buffer in bind_points.
        view_usage::type usage;
    };

    namespace image_view_extensions {
        using namespace resource_view_extensions;
        using extensions::allocator;
        using extensions::debug_named;
        using image_format::format_color;
        using image_format::format_depth;

        struct image_mipmap_range {
            ::std::uint32_t begin;
            ::std::uint32_t end;
        };
        struct image_array_range {
            ::std::uint32_t begin;
            ::std::uint32_t end;
        };

        struct attachment {
            // attachment index, for render pass.
            ::std::uint16_t index = invalid;
            union {
                ::std::uint16_t attribute = 0x41; // default is color attachment and clear.
                struct {
                    ::std::uint16_t color : 1;
                    ::std::uint16_t depth : 1;
                    ::std::uint16_t stencil : 1;
                    ::std::uint16_t resolve : 1;
                    ::std::uint16_t input : 1;

                    ::std::uint16_t load : 1;
                    ::std::uint16_t clear : 1;
                    ::std::uint16_t clear_stencil : 1;
                };
            };
        };

        // mark the image view cannot use by region dependency.
        struct frame_global {};
    }
    struct image_view {
        ::std::uint16_t index; // index of image in bind_points.
        view_usage::type usage;
    };


    // BEGIN PASS.

    namespace stage_extensions {
        struct stage_is_dynamic {};
    }

    namespace resource_usage_extensions {
    }
    namespace pipe_extensions {
        using extensions::debug_named;
        using extensions::graphics;
        using extensions::compute;

        struct pipe_bytes {
            detail::byte_view bytes;
        };

        struct inheritance {
            // pipe index inside pass.
            ::std::uint32_t index;
        };
    }
    struct pipe {
        ::std::uint32_t subpass = 0u;
    };

    namespace push_constants_extensions {
    }
    struct push_constants {
        ::std::uint32_t size;
    };

    namespace shader_extensions {
        struct customized_entry_point {
            const char* name; // must cstr.
        };
    }

    namespace buffer_view_extensions {
        template<typename...LocationDataType>
            requires(sizeof...(LocationDataType) > 0u)
        struct vertex_binding {
            // if invalid, then automatically as the final one.
            ::std::uint16_t binding = invalid;
            // set true to change input rate as per instance change.
            bool each_instance = false;
        };

        // only allow:
        //  ::std::uint16_t
        //  ::std::uint32_t
        // 
        //  ::std::uint8_t (is extension unless vulkan 1.2).
        template<typename T>
        struct vertex_indices {
        };
    }
    namespace vertex_input_extensions {
        using namespace stage_extensions;
    }
    struct vertex_input {
    };

    namespace input_assembly_extensions {
        using namespace stage_extensions;
    }
    union input_assembly {
        ::std::uint16_t mask = 0x14u;
        struct {
            ::std::uint16_t point : 1;
            ::std::uint16_t line : 1;
            ::std::uint16_t triangle : 1;
            ::std::uint16_t patch : 1;

            // if strip and fan are false, consider as list.

            ::std::uint16_t strip : 1;
            ::std::uint16_t fan : 1;

            ::std::uint16_t adjacent : 1;
        };
    };

    namespace vertex_shader_extensions {
        using namespace shader_extensions;
    }
    struct vertex_shader {
        detail::byte_view bytes;
    };

    namespace fragment_shader_extensions {
        using namespace shader_extensions;
    }
    namespace pixel_shader_extensions = fragment_shader_extensions;
    typedef struct fragment_shader {
        detail::byte_view bytes;
    } pixel_shader;

    namespace compute_shader_extensions {
        using namespace shader_extensions;
    }
    struct compute_shader {
        detail::byte_view bytes;
    };

    namespace command_extensions {
    }

    namespace draw_extensions {
        using namespace command_extensions;

        // rely on vertex input.
        struct draw_on_index {
            // index buffer's index in vertex input.
            ::std::uint16_t index;
            ::std::size_t offset;
        };
        // rely on vertex input.
        struct draw_vertex_by_indices {
            // vertex buffer's indices in vertex input.
            ::std::span<::std::uint16_t> vertex_indices;
            ::std::span<::std::size_t> offsets;
        };
        // rely on vertex input.
        struct draw_vertex_by_range {
            ::std::uint16_t begin = 0u;
            ::std::uint16_t end = invalid;
            // offset of binding.
            ::std::uint16_t binding = 0u;
            ::std::span<::std::size_t> offsets;
        };

        struct indexed {
            ::std::uint32_t index_count;
            ::std::uint32_t instance_count;
            ::std::uint32_t first_index;
            ::std::int32_t  vertex_offset;
            ::std::uint32_t first_instance;
        };
        struct instanced {
            ::std::uint32_t vertex_count;
            ::std::uint32_t instance_count;
            ::std::uint32_t first_vertex;
            ::std::uint32_t first_instance;
        };

        struct draw_clear {
            ::std::span<float> value;
            float depth;
            ::std::uint32_t stencil;
        };
    }
    struct draw {
    };

    namespace copy_flow_extensions {
        struct from_buffer_to_image {
            ::std::uint16_t source_index;
            ::std::uint16_t destination_index;
        };
        struct from_image_to_image {
            ::std::uint16_t source_index;
            ::std::uint16_t destination_index;
        };
        struct from_image_to_buffer {
            ::std::uint16_t source_index;
            ::std::uint16_t destination_index;
        };
        struct from_buffer_to_buffer {
            ::std::uint16_t source_index;
            ::std::uint16_t destination_index;
        };
        struct from_buffer_view_to_buffer_view {
            ::std::uint16_t source_index;
            ::std::uint16_t destination_index;
        };
        struct from_buffer_view_to_image_view {
            ::std::uint16_t source_index;
            ::std::uint16_t destination_index;
        };
        struct from_image_view_to_image_view {
            ::std::uint16_t source_index;
            ::std::uint16_t destination_index;
        };
        struct from_image_view_to_buffer_view {
            ::std::uint16_t source_index;
            ::std::uint16_t destination_index;
        };
    }
    struct copy_flow {
    };

    namespace pass_extensions {
        using extensions::allocator;
        using extensions::debug_named;
        using extensions::graphics;
        using extensions::compute;
        using extensions::transfer;
        struct affine_set {
            // set index in bind points.
            ::std::uint16_t index;
            // set index of pipe's index.
            ::std::uint16_t set;
        };
        struct render_pass {};
    }
    typedef struct pass {} begin_pass;

    // helper class.
    // struct end_pass {};
}


namespace vktl::emit {
    using vktl::set_event;
    using vktl::reset_event;
    
    struct wait_semaphores {
        ::std::span<::std::uint16_t> indices;
    };
    struct wait_fences {
        ::std::span<::std::uint16_t> indices;
    };

    struct update_buffer_view {

    };

    struct update_image_view {

    };

    struct update_buffer {

    };

    struct update_image {

    };

    struct execute {};
    struct record {};
    struct submit {};

    // struct recreate_swapchain{};
}