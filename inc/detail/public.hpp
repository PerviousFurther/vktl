#pragma once

#if VKTL_HAVE_STD_
#   include <span>
#   include <string_view>
#   include <tuple>
#endif

VKTL_EXPORT_ namespace vktl::detail {
	inline constexpr struct invalid_ {
		template<::std::unsigned_integral T>
		static constexpr auto value = ~T(0u);

		template<::std::unsigned_integral T>
		constexpr operator T() const noexcept { return ~T(0u); }
		template<::std::unsigned_integral T>
		friend constexpr bool operator==(T left, const invalid_) noexcept { return left == value<T>; }
		template<::std::unsigned_integral T>
		friend constexpr bool operator!=(T left, const invalid_) noexcept { return left != value<T>; }
		template<::std::unsigned_integral T>
		friend constexpr bool operator==(const invalid_, T left) noexcept { return left == value<T>; }
		template<::std::unsigned_integral T>
		friend constexpr bool operator!=(const invalid_, T left) noexcept { return left != value<T>; }
	} invalid{}, maximum{};



}

// BEFORE EDIT: 
// 1. extensions of `xxx` should inside namspace `xxx_extensions`
VKTL_EXPORT_ namespace vktl {
	using size_t = ::std::size_t;
	using uint64_t = ::std::uint64_t;
	using uint32_t = ::std::uint32_t;
	using uint16_t = ::std::uint16_t;
	using uint8_t = ::std::uint8_t;

	template<typename T>
	using span = ::std::span<T>;

	template<typename T>
	using cspan = span<::std::add_const_t<T>>;

	struct byte_view : span<::std::byte> {
		using base = span<::std::byte>;

		byte_view() = default;
		byte_view(void* bytes, ::std::size_t size)
			: base{ static_cast<::std::byte*>(bytes), size }
		{
		}
		template<typename T>
		byte_view(T* bytes, ::std::size_t count)
			: base{ reinterpret_cast<::std::byte*>(bytes), count * sizeof(T) }
		{
		}
		template<typename T>
		byte_view(::std::span<T> bytes)
			: base{ reinterpret_cast<::std::byte*>(bytes.data()), bytes.size() * sizeof(T) }
		{
		}
	};

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

				virtual void* allocate(size_t size, size_t alignment) = 0;
				virtual void* reallocate(void* original, size_t size, size_t alignment) = 0;
				virtual void free(void* memory) = 0;

				virtual void internal_allocate(size_t size, const char*) {}
				virtual void internal_free(size_t size, const char*) {}
			} *view{ nullptr } // take as view.
			, * move{ nullptr }; // take ownership of the allocator.
		};

		// some of the object must contain at least one of these object.
		// this describe task's type.

		inline constexpr struct compute_ {} compute;
		inline constexpr struct graphics_ {} graphics;
		inline constexpr struct transfer_ {} transfer;
		inline constexpr struct present_ {} present;

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
		struct repeat { size_t count; };
	}

	struct instance {
		const char* name{ "app-name-here:3" }; // app name.
		uint32_t version{ 1 }; // app version.
	};
	namespace instance_extensions {
		struct debug_utils {};
		// inline constexpr struct monitor_ <instance> {} monitor;
		inline constexpr struct validation_layer_ {} validation_layer;
	}


	namespace windows_extensions {
		using extensions::debug_named;
	}
	struct window {
		// library will not modify the handle.
		void* handle;
	};

	namespace device_extensions {
		using extensions::debug_named;
		struct enbale_budget {};
	}
	struct device {
		uint16_t index; // index of device.
	};

	
	namespace compiler_extensions {
		inline constexpr struct glsl_ {} glsl {};
		inline constexpr struct hlsl_ {} hlsl {};
		struct optimize { uint8_t level; };
	}
	typedef struct shader_handle_tag {} *shader_handle;
	// compiler's should select atleast glsl or dxc.
	inline constexpr struct compiler_ {} compiler;

	// complier need device as parent.

	struct queue_family {
		uint16_t family = 0u;
		uint16_t count = 1u;
	};
	namespace queue_family_extensions {
		using extensions::debug_named;
		struct queue_priority { // not queue-family-global-priorities.
			::std::span<float> priorities;
		};
	}


	// BEGIN EXECUTION.

	namespace queue_duty {
		using type = uint16_t;
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
		uint16_t thread_count;
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
		uint16_t family_index = 0u;
		uint16_t index = 0u;
	};

	namespace task_extensions {
		using extensions::debug_named;

		// in vulkan, free command buffer is an extensions.
		// extend this from task will allow free command buffer.
		struct allow_temporary_command_buffers {};
	}
	template<typename Fn>
	struct task { Fn func; };

	namespace split_extensions {
		using extensions::debug_named;
	}
	struct split {};

	// BEGIN RESOURCE

	namespace image_bits_type {
		using type = uint16_t;
		inline constexpr type undefined = 0x0u;

		inline constexpr type unorm = 0x1u;
		inline constexpr type uint = unorm + 0x1;

		inline constexpr type snorm = 0x100u;
		inline constexpr type sint = snorm + 0x1;
		inline constexpr type sfloat = snorm + 0x3;
	}

	namespace image_bits_order {
		using type = uint16_t;

		inline constexpr type rgba = 0x1;
		inline constexpr type rbga = 0x2;
		inline constexpr type bgra = 0x3;
		inline constexpr type abgr = 0x4;

		inline constexpr type depth_stencil = 0x0u;
	}

	namespace image_format {
		struct format_color {
			uint16_t order = image_bits_order::rgba;

			// atleast one of the value.

			uint16_t rbits = invalid;
			uint16_t gbits = invalid;
			uint16_t bbits = invalid;
			uint16_t abits = invalid;

			uint16_t rtype = image_bits_type::undefined;
			uint16_t gtype = image_bits_type::undefined;
			uint16_t btype = image_bits_type::undefined;
			uint16_t atype = image_bits_type::undefined;
		};

		struct format_depth {
			uint16_t order = image_bits_order::depth_stencil;

			uint16_t dbits = invalid;
			uint16_t sbits = invalid;
			uint16_t xbits = invalid;

			uint16_t dtype = image_bits_type::undefined;
			uint16_t stype = image_bits_type::undefined;
			uint16_t xtype = image_bits_type::undefined;
		};
	}

	namespace resource_attrs {
		using type = uint64_t;

		// indicate image reosurce's dependency is frame global.
		inline constexpr auto global_frame = type(0x1u) << 33;
		// indicate the dependency is device group local.
		inline constexpr auto device_local = type(0x1u) << 34;
		// indicate reosurce allow alias.
		inline constexpr auto allow_alias = type(0x1u) << 35;
		inline constexpr auto sparse = type(0x1u) << 36;
	}

	namespace resource_extensions {
		using extensions::repeat;
		using extensions::named;
		// using extensions::use_existing;

		inline constexpr struct allow_sparse_ {} allow_sparse{};
		// only image can use this value.
		inline constexpr struct allow_alias_ {} allow_alias{};
	}

	namespace buffer_extensions {
		using namespace resource_extensions;
	}
	// create buffer with specified size.
	struct buffer {
		size_t size;
	};

	namespace image_extensions {
		using namespace resource_extensions;
		using namespace image_format;

		// by default, the library will set to 1.
		struct image_miplevel {
			uint16_t count = 1u;
		};

		// by default, the library will set to 1.
		struct image_array_layers {
			uint16_t count = 1u;
		};
	}
	// it must atleast combines `color` or `depth` and other customized format descriptor.
	struct image {
		uint32_t width; // required.
		uint32_t height = maximum; // invalid means the image is image 1D, and depth will be ignored.
		uint16_t depth = maximum; // invalid means the image is image 2D.
	};

	namespace sampler_common {
		using type = uint32_t;
		inline constexpr auto black_background = 0x1u;
	}
	namespace sampler_extensions {
		using resource_extensions::repeat;
	}
	struct sampler {
		uint32_t type; // value in namespace sampler type.
	};

	namespace swapchain_extensions {
		using extensions::named;
	}
	struct swapchain {
		uint16_t min_frame_count;
		// if width is maximum, then get size from window.
		uint32_t width = maximum;
		// if height is maximum, then get size from window.
		uint32_t height = maximum;
	};

	namespace memory_allocator_extensions {
		using extensions::allocator;
		using extensions::debug_named;

		inline constexpr struct buddy_ {} buddy{};
		inline constexpr struct best_fit_ {} best_fit{};
		inline constexpr struct ringed_ {} ringed{};
		// inline constexpr struct 
		inline constexpr struct arena_ {} arena{}, linear{};
	}
	inline constexpr struct memory_allocator_ {} memory_allocator{};

	namespace resource_usage {
		using resource_attrs::type;

		inline constexpr type copy_src = 0x1u << 2;
		inline constexpr type copy_dst = 0x1u << 3;

		// some image or might need to mark read or write to identify more access usage.
		// color and depth stencil are default by write operation, might need to combine read to identify as read operation.

		inline constexpr type shader_read = 0x1u << 4;
		inline constexpr type uniform = shader_read;
		inline constexpr type gpu_read = shader_read;

		inline constexpr type shader_write = 0x1u << 5;
		inline constexpr type structured = shader_write;
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
		// using extensions::use_existing;

		// struct binding  {
		// 	uint16_t set;
		// 	uint16_t binding;
		// };
	}

	namespace buffer_view_extensions {
		using namespace resource_view_extensions;
		using extensions::debug_named;

		using image_format::format_color; // for texel buffer.
		struct buffer_range {
			size_t offset;
			size_t size;
		};
	}
	// if no buffer range, then view the whole buffer as this view.
	struct buffer_view {
		uint16_t index; // index of buffer in bind_points.
		resource_usage::type usage;
	};

	namespace image_view_extensions {
		using namespace resource_view_extensions;
		using extensions::allocator;
		using extensions::debug_named;
		using image_format::format_color;
		using image_format::format_depth;

		struct image_mipmap_range {
			uint32_t begin;
			uint32_t end;
		};
		struct image_array_range {
			uint32_t begin;
			uint32_t end;
		};
		// mark the image view cannot use by region dependency.
		struct frame_global {};
	}
	struct image_view {
		uint16_t index; // index of image in bind_points.
		resource_usage::type usage;
	};


	// BEGIN PASS.

	namespace stage_extensions {
		struct stage_is_dynamic {};
	}

	namespace resource_usage_extensions {
	}
	namespace pipe_extensions {
		using extensions::debug_named;
		using extensions::graphics_;
		using extensions::compute_;
		// using extensions::use_existing;

		struct pipe_bytes {
			byte_view bytes;
		};

		struct inheritance {
			// pipe index inside pass.
			uint32_t index;
		};

		struct subpass {
			// subpass index.
			uint16_t index;
		};
	}
	// if graphics pipeline and need render pass, it default is subpass 0,
	// else use subpass extension.
	inline constexpr struct pipe_ {} pipe;

	namespace push_constants_extensions {
	}
	// push constants usually small.
	struct push_constants {
		uint16_t size;
		uint16_t offset = invalid;
	};

	namespace vertex_input_extensions {
		using namespace stage_extensions;

		struct vertex_binding {
			// if invalid, then automatically as the final one.
			::std::uint16_t binding = invalid;
			// set true to change input rate as per instance change.
			bool each_instance = false;
		};
	}
	struct vertex_input {};

	namespace input_assembly_extensions {
		using namespace stage_extensions;
	}
	union input_assembly {
		uint16_t mask = 0x14u;
		struct {
			uint16_t point : 1;
			uint16_t line : 1;
			uint16_t triangle : 1;
			uint16_t patch : 1;

			// if strip and fan are false, consider as list.

			uint16_t strip : 1;
			uint16_t fan : 1;

			uint16_t adjacent : 1;
		};
	};

	namespace shader_extensions {
		union attachment {
			// default is color attachment, clear and frame-local.
			uint16_t attribute = 0x41;
			struct {
				uint16_t color : 1;
				uint16_t depth : 1;
				uint16_t stencil : 1;
				uint16_t resolve : 1;
				uint16_t input : 1;

				uint16_t load : 1;
				uint16_t clear : 1;
				uint16_t clear_stencil : 1;
				uint16_t frame_global : 1;
			};
		};

		struct customized_entry_point {
			const char* name; // must cstr.
		};
	}

	namespace vertex_shader_extensions {
		using namespace shader_extensions;
	}
	struct vertex_shader {
		shader_handle bytes;
	};

	namespace fragment_shader_extensions {
		using namespace shader_extensions;
	}
	struct fragment_shader {
		shader_handle bytes;
	};


	namespace blend_state_extensions {
	}
	struct blend_state {
		uint32_t index;
	};

	namespace compute_shader_extensions {
		using namespace shader_extensions;
	}
	struct compute_shader {
		shader_handle code;
	};

	namespace command_extensions {
	}

	namespace pass_extensions {
		using extensions::allocator;
		using extensions::debug_named;
		using extensions::graphics;
		using extensions::compute;
		using extensions::transfer;
		using extensions::present;

		inline constexpr struct render_pass_ {} render_pass;
	}
	inline constexpr struct pass_ {} pass;

	// helper class.
	// struct end_pass {};
}


VKTL_EXPORT_ namespace vktl::emit {
	// using vktl::set_event;
	// using vktl::reset_event;

	struct wait_semaphores {
		::std::span<uint16_t> indices;
	};
	struct wait_fences {
		::std::span<uint16_t> indices;
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
	struct refresh {};
	// struct recreate_swapchain{};

}