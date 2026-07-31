#pragma once

#include "execution.hpp"

// BEGIN BIND_POINTS.

VKTL_EXPORT_ namespace vktl::detail {
	inline constexpr auto BIND_POINTS_SCOPE = EXECUTION_SCOPE + 0x1u;

	template<::std::size_t index = 0>
	struct buffer_usage_t {};
	template<>
	struct buffer_usage_t<0> {
		template<typename T>
		constexpr auto operator()(T usage) const noexcept {
			using namespace view_usage;
			VK_ VkBufferUsageFlags vk_flags = 0u;

			if ((usage & uniform) == uniform) {
				vk_flags |= VK_ VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			}
			if ((usage & copy_src) == copy_src) {
				vk_flags |= VK_ VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			}
			if ((usage & copy_dst) == copy_dst) {
				vk_flags |= VK_ VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			}

			const auto is_writable = bool(usage & shader_write);
			// const auto is_readable = bool(usage & shader_read);

			if ((usage & texel) == texel) {
				if (is_writable) {
					vk_flags |= VK_ VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
				}
				else {
					vk_flags |= VK_ VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
				}
			}
			else if (is_writable) {
				vk_flags |= VK_ VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			}

			if ((usage & vertex) == vertex) {
				vk_flags |= VK_ VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			}
			if ((usage & index) == index) {
				vk_flags |= VK_ VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			}
			if ((usage & indirect) == indirect) {
				vk_flags |= VK_ VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
			}

			if constexpr (::std::invocable<buffer_usage_t<1u>, T>)
				vk_flags |= buffer_usage_t<1u>()(usage);

			return vk_flags;
		}
	};
	template<::std::size_t index = 0>
	inline constexpr buffer_usage_t<index> buffer_usage{};


	template<::std::size_t index = 0>
	struct image_usage_t {};

	template<>
	struct image_usage_t<0> {
		template<typename T>
		constexpr auto operator()(T usage) const noexcept {
			using namespace view_usage;
			VK_ VkImageUsageFlags flags = 0u;

			constexpr auto depth_stencil = depth | stencil;

			if ((usage & copy_src) == copy_src) {
				flags |= VK_ VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			}
			if ((usage & copy_dst) == copy_dst) {
				flags |= VK_ VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			}

			if ((usage & shader_write) == shader_write) {
				flags |= VK_ VK_IMAGE_USAGE_STORAGE_BIT;
			}
			if ((usage & sampled) == sampled) {
				flags |= VK_ VK_IMAGE_USAGE_SAMPLED_BIT;
			}
			// if ((usage & shader_read) == shader_read) {
			// 	vk_flags |= VK_ VK_IMAGE_USAGE_SAMPLED_BIT;
			// }

			assert((usage & render_target) ^ (usage & depth_stencil));

			if ((usage & render_target) == render_target) {
				flags |= VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			}
			if ((usage & depth_stencil) == depth_stencil) {
				flags |= VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			}

			if ((usage & input) == input) {
				flags |= VK_ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
			}

			if constexpr (::std::invocable<image_usage_t<1u>, T>) {
				flags |= image_usage_t<1u>()(usage);
			}

			return flags;
		}
	};
	template<::std::size_t index = 0>
	inline constexpr image_usage_t<index> image_usage{};

	inline constexpr ::std::uint32_t compress_bits(::std::uint16_t bits) {
		return (bits == static_cast<::std::uint16_t>(-1)) ? 0 :
			(bits == 8) ? 1 :
			(bits == 16) ? 2 :
			(bits == 24) ? 3 :
			(bits == 32) ? 4 :
			(bits == 64) ? 5 : 7;
	}

	inline constexpr::std::uint32_t compress_type(::std::uint16_t type) {
		return (type == texture_bits_type::undefined) ? 0 :
			(type == texture_bits_type::unorm) ? 1 :
			(type == texture_bits_type::uint) ? 2 :
			(type == texture_bits_type::snorm) ? 3 :
			(type == texture_bits_type::sint) ? 4 :
			(type == texture_bits_type::sfloat) ? 5 : 7;
	}

	inline constexpr::std::uint32_t pack_color_key(
		::std::uint16_t order,
		::std::uint16_t rbits, ::std::uint16_t gbits, ::std::uint16_t bbits, ::std::uint16_t abits,
		::std::uint16_t rtype, ::std::uint16_t gtype, ::std::uint16_t btype, ::std::uint16_t atype)
	{
		return (static_cast<::std::uint32_t>(order) << 24) |
			(compress_bits(rbits) << 20) |
			(compress_bits(gbits) << 17) |
			(compress_bits(bbits) << 14) |
			(compress_bits(abits) << 11) |
			(compress_type(rbits == static_cast<::std::uint16_t>(-1) ? texture_bits_type::undefined : rtype) << 8) |
			(compress_type(gbits == static_cast<::std::uint16_t>(-1) ? texture_bits_type::undefined : gtype) << 5) |
			(compress_type(bbits == static_cast<::std::uint16_t>(-1) ? texture_bits_type::undefined : btype) << 2) |
			(compress_type(abits == static_cast<::std::uint16_t>(-1) ? texture_bits_type::undefined : atype));
	}

	inline constexpr::std::uint32_t color_case(::std::uint16_t order,
		::std::uint16_t rbits,
		::std::uint16_t gbits,
		::std::uint16_t bbits,
		::std::uint16_t abits,
		::std::uint16_t type) {
		return pack_color_key(order, rbits, gbits, bbits, abits, type, type, type, type);
	}

	inline constexpr::std::uint32_t compress_depth_type(::std::uint16_t type) {
		return (type == static_cast<::std::uint16_t>(-1)) ? 0 : compress_type(type);
	}

	inline constexpr::std::uint32_t pack_depth_key(::std::uint16_t dbits, ::std::uint16_t dtype, ::std::uint16_t sbits, ::std::uint16_t stype) {
		return (compress_bits(dbits) << 12) |
			(compress_depth_type(dbits == static_cast<::std::uint16_t>(-1) ? static_cast<::std::uint16_t>(-1) : dtype) << 8) |
			(compress_bits(sbits) << 4) |
			(compress_depth_type(sbits == static_cast<::std::uint16_t>(-1) ? static_cast<::std::uint16_t>(-1) : stype));
	}


	template<::std::size_t index = 0>
	struct to_format_ {
		template<typename T>
		constexpr auto operator()(T const& fmt) const noexcept requires(index == 0u) {
			using namespace texture_bits_type;
			constexpr ::std::uint16_t N = -1;

			if constexpr (requires { fmt.order; }) {
				using namespace texture_order;
				switch (pack_color_key(fmt.order, fmt.rbits, fmt.gbits, fmt.bbits, fmt.abits, fmt.rtype, fmt.gtype, fmt.btype, fmt.atype))
				{
				case color_case(rgba, 8, N, N, N, unorm): return VK_ VK_FORMAT_R8_UNORM;
				case color_case(rgba, 8, N, N, N, snorm): return VK_ VK_FORMAT_R8_SNORM;
				case color_case(rgba, 8, N, N, N, uint):  return VK_ VK_FORMAT_R8_UINT;
				case color_case(rgba, 8, N, N, N, sint):  return VK_ VK_FORMAT_R8_SINT;

				case color_case(rgba, 16, N, N, N, unorm):  return VK_ VK_FORMAT_R16_UNORM;
				case color_case(rgba, 16, N, N, N, snorm):  return VK_ VK_FORMAT_R16_SNORM;
				case color_case(rgba, 16, N, N, N, uint):   return VK_ VK_FORMAT_R16_UINT;
				case color_case(rgba, 16, N, N, N, sint):   return VK_ VK_FORMAT_R16_SINT;
				case color_case(rgba, 16, N, N, N, sfloat): return VK_ VK_FORMAT_R16_SFLOAT;

				case color_case(rgba, 32, N, N, N, uint):   return VK_ VK_FORMAT_R32_UINT;
				case color_case(rgba, 32, N, N, N, sint):   return VK_ VK_FORMAT_R32_SINT;
				case color_case(rgba, 32, N, N, N, sfloat): return VK_ VK_FORMAT_R32_SFLOAT;

				case color_case(rgba, 8, 8, N, N, unorm): return VK_ VK_FORMAT_R8G8_UNORM;
				case color_case(rgba, 8, 8, N, N, snorm): return VK_ VK_FORMAT_R8G8_SNORM;
				case color_case(rgba, 8, 8, N, N, uint):  return VK_ VK_FORMAT_R8G8_UINT;
				case color_case(rgba, 8, 8, N, N, sint):  return VK_ VK_FORMAT_R8G8_SINT;

				case color_case(rgba, 16, 16, N, N, unorm):  return VK_ VK_FORMAT_R16G16_UNORM;
				case color_case(rgba, 16, 16, N, N, snorm):  return VK_ VK_FORMAT_R16G16_SNORM;
				case color_case(rgba, 16, 16, N, N, uint):   return VK_ VK_FORMAT_R16G16_UINT;
				case color_case(rgba, 16, 16, N, N, sint):   return VK_ VK_FORMAT_R16G16_SINT;
				case color_case(rgba, 16, 16, N, N, sfloat): return VK_ VK_FORMAT_R16G16_SFLOAT;

				case color_case(rgba, 32, 32, N, N, uint):   return VK_ VK_FORMAT_R32G32_UINT;
				case color_case(rgba, 32, 32, N, N, sint):   return VK_ VK_FORMAT_R32G32_SINT;
				case color_case(rgba, 32, 32, N, N, sfloat): return VK_ VK_FORMAT_R32G32_SFLOAT;

				case color_case(rgba, 8, 8, 8, N, unorm): return VK_ VK_FORMAT_R8G8B8_UNORM;
				case color_case(rgba, 8, 8, 8, N, snorm): return VK_ VK_FORMAT_R8G8B8_SNORM;
				case color_case(rgba, 8, 8, 8, N, uint):  return VK_ VK_FORMAT_R8G8B8_UINT;
				case color_case(rgba, 8, 8, 8, N, sint):  return VK_ VK_FORMAT_R8G8B8_SINT;

				case color_case(rgba, 16, 16, 16, N, unorm):  return VK_ VK_FORMAT_R16G16B16_UNORM;
				case color_case(rgba, 16, 16, 16, N, snorm):  return VK_ VK_FORMAT_R16G16B16_SNORM;
				case color_case(rgba, 16, 16, 16, N, uint):   return VK_ VK_FORMAT_R16G16B16_UINT;
				case color_case(rgba, 16, 16, 16, N, sint):   return VK_ VK_FORMAT_R16G16B16_SINT;
				case color_case(rgba, 16, 16, 16, N, sfloat): return VK_ VK_FORMAT_R16G16B16_SFLOAT;

				case color_case(rgba, 8, 8, 8, 8, unorm): return VK_ VK_FORMAT_R8G8B8A8_UNORM;
				case color_case(rgba, 8, 8, 8, 8, snorm): return VK_ VK_FORMAT_R8G8B8A8_SNORM;
				case color_case(rgba, 8, 8, 8, 8, uint):  return VK_ VK_FORMAT_R8G8B8A8_UINT;
				case color_case(rgba, 8, 8, 8, 8, sint):  return VK_ VK_FORMAT_R8G8B8A8_SINT;

				case color_case(rgba, 16, 16, 16, 16, unorm):  return VK_ VK_FORMAT_R16G16B16A16_UNORM;
				case color_case(rgba, 16, 16, 16, 16, snorm):  return VK_ VK_FORMAT_R16G16B16A16_SNORM;
				case color_case(rgba, 16, 16, 16, 16, uint):   return VK_ VK_FORMAT_R16G16B16A16_UINT;
				case color_case(rgba, 16, 16, 16, 16, sint):   return VK_ VK_FORMAT_R16G16B16A16_SINT;
				case color_case(rgba, 16, 16, 16, 16, sfloat): return VK_ VK_FORMAT_R16G16B16A16_SFLOAT;

				case color_case(rgba, 32, 32, 32, 32, uint):   return VK_ VK_FORMAT_R32G32B32A32_UINT;
				case color_case(rgba, 32, 32, 32, 32, sint):   return VK_ VK_FORMAT_R32G32B32A32_SINT;
				case color_case(rgba, 32, 32, 32, 32, sfloat): return VK_ VK_FORMAT_R32G32B32A32_SFLOAT;

				case color_case(bgra, 8, 8, 8, 8, unorm): return VK_ VK_FORMAT_B8G8R8A8_UNORM;
				case color_case(bgra, 8, 8, 8, 8, snorm): return VK_ VK_FORMAT_B8G8R8A8_SNORM;
				case color_case(bgra, 8, 8, 8, 8, uint):  return VK_ VK_FORMAT_B8G8R8A8_UINT;
				case color_case(bgra, 8, 8, 8, 8, sint):  return VK_ VK_FORMAT_B8G8R8A8_SINT;

				default: break;
				}
			}
			else if constexpr (requires { fmt.dbits; }) {
				switch (pack_depth_key(fmt.dbits, fmt.dtype, fmt.sbits, fmt.stype)) {

				case pack_depth_key(16, unorm, N, N):     return VK_ VK_FORMAT_D16_UNORM;
				case pack_depth_key(24, unorm, N, N):     return VK_ VK_FORMAT_X8_D24_UNORM_PACK32;
				case pack_depth_key(32, sfloat, N, N):    return VK_ VK_FORMAT_D32_SFLOAT;

				case pack_depth_key(N, N, 8, uint):       return VK_ VK_FORMAT_S8_UINT;
				case pack_depth_key(16, unorm, 8, uint):  return VK_ VK_FORMAT_D16_UNORM_S8_UINT;
				case pack_depth_key(24, unorm, 8, uint):  return VK_ VK_FORMAT_D24_UNORM_S8_UINT;
				case pack_depth_key(32, sfloat, 8, uint): return VK_ VK_FORMAT_D32_SFLOAT_S8_UINT;

				default: break;
				}
			}

			if constexpr (::std::invocable<to_format_<1u>, T const&>) {
				auto res = to_format_<1u>()(fmt);
				return res;
			}
			else
				return VK_ VK_FORMAT_UNDEFINED;
		}
	};
	template<::std::size_t index = 0>
	inline constexpr to_format_<index> to_format{};

	template<::std::size_t index>
	struct buffer_access_t {
		template<typename T>
		constexpr auto operator()(T usage) const noexcept requires(index == 0u) {
			VK_ VkAccessFlags flags = 0;

			if (usage & view_usage::uniform)      flags |= VK_ VK_ACCESS_UNIFORM_READ_BIT | VK_ VK_ACCESS_SHADER_READ_BIT;
			if (usage & view_usage::copy_src)     flags |= VK_ VK_ACCESS_TRANSFER_READ_BIT;
			if (usage & view_usage::copy_dst)     flags |= VK_ VK_ACCESS_TRANSFER_WRITE_BIT;
			// if (usage & view_usage::shader_read)  flags |= VK_ VK_ACCESS_SHADER_READ_BIT;
			if (usage & view_usage::shader_write) flags |= VK_ VK_ACCESS_SHADER_WRITE_BIT;

			if (usage & view_usage::vertex)   flags |= VK_ VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			if (usage & view_usage::index)    flags |= VK_ VK_ACCESS_INDEX_READ_BIT;
			if (usage & view_usage::indirect) flags |= VK_ VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

			if (usage & view_usage::texel) {
				if (usage & view_usage::shader_write) {
					flags |= VK_ VK_ACCESS_SHADER_WRITE_BIT;
				}
				else {
					flags |= VK_ VK_ACCESS_SHADER_READ_BIT;
				}
			}

			if constexpr (::std::invocable<buffer_access_t<1>, T>) {
				flags |= buffer_access_t<1>()(usage);
			}

			return flags;
		}
	};
	template<::std::size_t index = 0u>
	constexpr buffer_access_t<index> buffer_access = {};

	template<::std::size_t index>
	struct image_access_t {
		template<typename T>
		constexpr auto operator()(T usage) const noexcept requires(index == 0u) {
			if constexpr (::std::same_as<view_usage::type, T>) {
				VK_ VkAccessFlags flags = 0;

				if (usage & view_usage::copy_src) {
					flags |= VK_ VK_ACCESS_TRANSFER_READ_BIT;
				}
				if (usage & view_usage::copy_dst) {
					flags |= VK_ VK_ACCESS_TRANSFER_WRITE_BIT;
				}

				if (usage & view_usage::color) {
					if (usage & view_usage::shader_read) {
						flags |= VK_ VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
					}
					else {
						flags |= VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					}
				}
				else if (usage & (view_usage::depth | view_usage::stencil)) {
					if (usage & view_usage::shader_read) {
						flags |= VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
					}
					else {
						flags |= VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
					}
				}

				if (usage & view_usage::shader_write)    flags |= VK_ VK_ACCESS_SHADER_WRITE_BIT;
				if (usage & view_usage::sampled) flags |= VK_ VK_ACCESS_SHADER_READ_BIT;

				if constexpr (::std::invocable<image_access_t<1>, T>) {
					flags |= image_access_t<1>()(usage);
				}

				return flags;
			}
		}
	};
	template<::std::size_t index = 0u>
	constexpr image_access_t<index> to_image_access = {};

	template <::std::size_t index = 0>
	struct buffer_descriptor_type_t {
		template <typename T>
		constexpr auto operator()(T value) const noexcept requires(index == 0u) {
			if constexpr (::std::same_as<T, view_usage::type>) {
				using namespace view_usage;

				if ((value & texel) == texel) {
					if ((value & shader_write) != 0) {
						return VK_ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
					}
					return VK_ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				}

				if ((value & uniform) != 0) {
					return VK_ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				}

				if ((value & vertex) == vertex ||
					(value & index) == index ||
					(value & indirect) == indirect) {
					return VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
				}

				if ((value & shader_write) != 0 || (value & shader_read) != 0) {
					return VK_ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				}

				if constexpr (::std::invocable<buffer_descriptor_type_t<1u>, T>) {
					return buffer_descriptor_type_t<1u>()(value);
				}

				return VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
			}
			else if constexpr (::std::same_as<VK_ VkDescriptorType, T>) {
				bool is_ = false;
				if constexpr (::std::invocable<buffer_descriptor_type_t<1u>, T>) {
					is_ = buffer_descriptor_type_t<1u>()(value);
				}
				return value == VK_ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
					|| value == VK_ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
					|| value == VK_ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
					|| value == VK_ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
					|| is_;
			}
		}
	};

	template <::std::size_t index = 0>
	inline constexpr buffer_descriptor_type_t<index> buffer_descriptor_type{};

	template <::std::size_t index = 0>
	struct image_descriptor_type_t {
		template <typename T>
		constexpr auto operator()(T value) const noexcept requires(index == 0u) {
			if constexpr (::std::same_as<T, view_usage::type>) {
				using namespace view_usage;

				if ((value & shader_write) != 0) {
					return VK_ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				}

				if ((value & sampled) == sampled) {
					return VK_ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				}

				if ((value & texel) != 0) {
					return VK_ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; // need sampler.
				}

				if ((value & input) == input) {
					return VK_ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				}

				if constexpr (::std::invocable<image_descriptor_type_t<1u>, T>) {
					return image_descriptor_type_t<1u>()(value);
				}

				return VK_ VK_DESCRIPTOR_TYPE_MAX_ENUM;
			}
			else if constexpr (::std::same_as<VK_ VkDescriptorType, T>) {
				bool is_ = false;
				if constexpr (::std::invocable<image_descriptor_type_t<1u>, T>) {
					is_ = image_descriptor_type_t<1u>()(value);
				}
				return value == VK_ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					|| value == VK_ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
					|| value == VK_ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
					|| value == VK_ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
					|| is_;
			}
			else if constexpr (::std::invocable<image_descriptor_type_t<1u>, T>) {
				return image_descriptor_type_t<1u>()(value);
			}
		}
	};
	template <::std::size_t index = 0>
	inline constexpr image_descriptor_type_t<index> image_descriptor_type{};

	template<::std::size_t index>
	struct to_aspect_mask_ {
		template<typename U>
		constexpr auto operator()(U usages) const noexcept requires (index == 0u) {
			if constexpr (::std::same_as<U, resource_attrs::type>) {
				auto flags = VK_ VkImageAspectFlags(0u);
				if constexpr (::std::invocable<to_aspect_mask_<index + 1u>, U>) {
					flags |= to_aspect_mask_<index + 1u>()(usages);
				}
				return flags
					| ((usages & view_usage::depth) ? VK_ VK_IMAGE_ASPECT_DEPTH_BIT : 0u)
					| ((usages & view_usage::stencil) ? VK_ VK_IMAGE_ASPECT_STENCIL_BIT : 0u)
					| ((usages & view_usage::color) ? VK_ VK_IMAGE_ASPECT_COLOR_BIT : 0);
			}
			else if constexpr (::std::invocable<to_aspect_mask_<index + 1u>, U>) {
				return to_aspect_mask_<index + 1u>()(usages);
			}
		}
	};
	template<::std::size_t index = 0u>
	constexpr to_aspect_mask_<index> to_aspect_mask{};

	template<::std::size_t index>
	struct to_dependency_ {
		template<typename T>
		constexpr auto operator()(T value) const noexcept {
			if constexpr (::std::same_as<resource_attrs::type>) {
				auto flags = VK_ VkDependencyFlags(0u);
				if ((value & resource_attrs::global_frame) == 0u
					&& (value & (view_usage::color | view_usage::depth | view_usage::stencil | view_usage::input)) != 0) {
					flags |= VK_ VK_DEPENDENCY_BY_REGION_BIT;
				}
				if (value & resource_attrs::device_local) {
					flags |= VK_ VK_DEPENDENCY_DEVICE_GROUP_BIT_KHR;
				}
				
				return flags;
			}
		}
	};
	template<::std::size_t index>
	constexpr to_dependency_<index> to_dependency{};

	template<::std::size_t index>
	struct revise_format_ {
		enum class DType { None, D16, D24, D32 };
		enum class SType { None, S8 };
		enum class ColorFamily {
			None, R8, R8G8, R8G8B8A8, B8G8R8A8,
			R16, R16G16, R16G16B16A16,
			R32, R32G32, R32G32B32A32
		};
		struct FormatProps {
			ColorFamily     color;
			DType           depth;
			SType           stencil;
			VK_ VkFormat base;
		};

		//
		// i dont know why intellisense doing this. 
		// 
		static constexpr FormatProps get_props(VK_ VkFormat f) {
			using CF = ColorFamily;
			using DT = DType;
			using ST = SType;

			switch (f) {
				// R8
				case VK_ VK_FORMAT_R8_UNORM :
					case VK_ VK_FORMAT_R8_SNORM :
						case VK_ VK_FORMAT_R8_UINT :
							case VK_ VK_FORMAT_R8_SINT :
								case VK_ VK_FORMAT_R8_SRGB :
									return { CF::R8, DT::None, ST::None, VK_ VK_FORMAT_R8_UNORM };

									// R8G8
									case VK_ VK_FORMAT_R8G8_UNORM :
										case VK_ VK_FORMAT_R8G8_SNORM :
											case VK_ VK_FORMAT_R8G8_UINT :
												case VK_ VK_FORMAT_R8G8_SINT :
													case VK_ VK_FORMAT_R8G8_SRGB :
														return { CF::R8G8, DT::None, ST::None, VK_ VK_FORMAT_R8G8_UNORM };

														// R8G8B8A8
														case VK_ VK_FORMAT_R8G8B8A8_UNORM :
															case VK_ VK_FORMAT_R8G8B8A8_SNORM :
																case VK_ VK_FORMAT_R8G8B8A8_UINT :
																	case VK_ VK_FORMAT_R8G8B8A8_SINT :
																		case VK_ VK_FORMAT_R8G8B8A8_SRGB :
																			return { CF::R8G8B8A8, DT::None, ST::None, VK_ VK_FORMAT_R8G8B8A8_UNORM };

																			// B8G8R8A8
																			case VK_ VK_FORMAT_B8G8R8A8_UNORM :
																				case VK_ VK_FORMAT_B8G8R8A8_SNORM :
																					case VK_ VK_FORMAT_B8G8R8A8_UINT :
																						case VK_ VK_FORMAT_B8G8R8A8_SINT :
																							case VK_ VK_FORMAT_B8G8R8A8_SRGB :
																								return { CF::B8G8R8A8, DT::None, ST::None, VK_ VK_FORMAT_B8G8R8A8_UNORM };

																								// R16
																								case VK_ VK_FORMAT_R16_UNORM :
																									case VK_ VK_FORMAT_R16_SNORM :
																										case VK_ VK_FORMAT_R16_UINT :
																											case VK_ VK_FORMAT_R16_SINT :
																												case VK_ VK_FORMAT_R16_SFLOAT :
																													return { CF::R16, DT::None, ST::None, VK_ VK_FORMAT_R16_UNORM };

																													// R16G16
																													case VK_ VK_FORMAT_R16G16_UNORM :
																														case VK_ VK_FORMAT_R16G16_SNORM :
																															case VK_ VK_FORMAT_R16G16_UINT :
																																case VK_ VK_FORMAT_R16G16_SINT :
																																	case VK_ VK_FORMAT_R16G16_SFLOAT :
																																		return { CF::R16G16, DT::None, ST::None, VK_ VK_FORMAT_R16G16_UNORM };

																																		// R16G16B16A16
																																		case VK_ VK_FORMAT_R16G16B16A16_UNORM :
																																			case VK_ VK_FORMAT_R16G16B16A16_SNORM :
																																				case VK_ VK_FORMAT_R16G16B16A16_UINT :
																																					case VK_ VK_FORMAT_R16G16B16A16_SINT :
																																						case VK_ VK_FORMAT_R16G16B16A16_SFLOAT :
																																							return { CF::R16G16B16A16, DT::None, ST::None, VK_ VK_FORMAT_R16G16B16A16_UNORM };

																																							// R32
																																							case VK_ VK_FORMAT_R32_UINT :
																																								case VK_ VK_FORMAT_R32_SINT :
																																									case VK_ VK_FORMAT_R32_SFLOAT :
																																										return { CF::R32, DT::None, ST::None, VK_ VK_FORMAT_R32_SFLOAT };

																																										// R32G32
																																										case VK_ VK_FORMAT_R32G32_UINT :
																																											case VK_ VK_FORMAT_R32G32_SINT :
																																												case VK_ VK_FORMAT_R32G32_SFLOAT :
																																													return { CF::R32G32, DT::None, ST::None, VK_ VK_FORMAT_R32G32_SFLOAT };

																																													// R32G32B32A32
																																													case VK_ VK_FORMAT_R32G32B32A32_UINT :
																																														case VK_ VK_FORMAT_R32G32B32A32_SINT :
																																															case VK_ VK_FORMAT_R32G32B32A32_SFLOAT :
																																																return { CF::R32G32B32A32, DT::None, ST::None, VK_ VK_FORMAT_R32G32B32A32_SFLOAT };

																																																case VK_ VK_FORMAT_D16_UNORM :
																																																	return { CF::None, DT::D16, ST::None, VK_ VK_FORMAT_D16_UNORM };
																																																	case VK_ VK_FORMAT_X8_D24_UNORM_PACK32 :
																																																		return { CF::None, DT::D24, ST::None, VK_ VK_FORMAT_X8_D24_UNORM_PACK32 };
																																																		case VK_ VK_FORMAT_D32_SFLOAT :
																																																			return { CF::None, DT::D32, ST::None, VK_ VK_FORMAT_D32_SFLOAT };
																																																			case VK_ VK_FORMAT_S8_UINT :
																																																				return { CF::None, DT::None, ST::S8, VK_ VK_FORMAT_S8_UINT };
																																																				case VK_ VK_FORMAT_D16_UNORM_S8_UINT :
																																																					return { CF::None, DT::D16, ST::S8, VK_ VK_FORMAT_D16_UNORM_S8_UINT };
																																																					case VK_ VK_FORMAT_D24_UNORM_S8_UINT :
																																																						return { CF::None, DT::D24, ST::S8, VK_ VK_FORMAT_D24_UNORM_S8_UINT };
																																																						case VK_ VK_FORMAT_D32_SFLOAT_S8_UINT :
																																																							return { CF::None, DT::D32, ST::S8, VK_ VK_FORMAT_D32_SFLOAT_S8_UINT };

																																																						default:
																																																							return { CF::None, DT::None, ST::None, VK_ VK_FORMAT_UNDEFINED };
			}
		}

		static constexpr auto make_depth(DType d, bool s) {
			switch (d) {
			case DType::D16: return s ? VK_ VK_FORMAT_D16_UNORM_S8_UINT : VK_ VK_FORMAT_D16_UNORM;
			case DType::D24: return s ? VK_ VK_FORMAT_D24_UNORM_S8_UINT : VK_ VK_FORMAT_X8_D24_UNORM_PACK32;
			case DType::D32: return s ? VK_ VK_FORMAT_D32_SFLOAT_S8_UINT : VK_ VK_FORMAT_D32_SFLOAT;
			default:         return s ? VK_ VK_FORMAT_S8_UINT : VK_ VK_FORMAT_UNDEFINED;
			}
		}

		template<typename F>
		constexpr bool operator()(F& create_format, F& view_format)
			const noexcept requires(index == 0u) {
			auto c = VK_ VkFormat(create_format);
			auto v = VK_ VkFormat(view_format);

			auto pc = get_props(c);
			auto pv = get_props(v);

			VK_ VkFormat resolved_create = VK_ VK_FORMAT_UNDEFINED;
			VK_ VkFormat resolved_view = (v == VK_ VK_FORMAT_UNDEFINED) ? c : v;

			if (c == VK_ VK_FORMAT_UNDEFINED) {
				resolved_create = (v == VK_ VK_FORMAT_UNDEFINED) ? VK_ VK_FORMAT_UNDEFINED
					: (pv.base != VK_ VK_FORMAT_UNDEFINED ? pv.base : v);
			}
			else if (v == VK_ VK_FORMAT_UNDEFINED || c == v) {
				resolved_create = c;
				resolved_view = c;
			}
			else if (pc.color != ColorFamily::None && pc.color == pv.color) {
				resolved_create = pc.base;
			}
			else if (pc.color == ColorFamily::None && pv.color == ColorFamily::None) {
				auto depth = pc.depth != DType::None ? pc.depth : pv.depth;
				if (pc.depth != DType::None && pv.depth != DType::None && pc.depth != pv.depth)
					depth = DType::None;

				resolved_create = make_depth(depth,
					pc.stencil != SType::None || pv.stencil != SType::None);
			}

			if (resolved_create == VK_ VK_FORMAT_UNDEFINED || resolved_view == VK_ VK_FORMAT_UNDEFINED) {
				if constexpr (::std::invocable<revise_format_<index + 1>, F&, F&>)
					return revise_format_<index + 1>()(resolved_create, resolved_view);
			}

			create_format = F(resolved_create);
			view_format = F(resolved_view);
			return true;
		}
	};
	template<::std::size_t index = 0>
	constexpr revise_format_<index> revise_format{};

	template<::std::size_t index = 0>
	struct revise_view_t {
		// static constexpr auto VK_IMAGE_VIEW_TYPE_UNDEFINED = VK_ VkImageViewType(~0);

		static constexpr bool type_compatible(VK_ VkImageViewType view_type, VK_ VkImageType image_type,
			uint32_t arrayLayers, VK_ VkImageCreateFlags flags) {
			switch (view_type) {
				case VK_ VK_IMAGE_VIEW_TYPE_1D :
					return image_type == VK_ VK_IMAGE_TYPE_1D && arrayLayers == 1;
					case VK_ VK_IMAGE_VIEW_TYPE_1D_ARRAY :
						return image_type == VK_ VK_IMAGE_TYPE_1D && arrayLayers > 1;
						case VK_ VK_IMAGE_VIEW_TYPE_2D :
							return image_type == VK_ VK_IMAGE_TYPE_2D && arrayLayers == 1;
							case VK_ VK_IMAGE_VIEW_TYPE_2D_ARRAY :
								return image_type == VK_ VK_IMAGE_TYPE_2D && arrayLayers > 1;
								case VK_ VK_IMAGE_VIEW_TYPE_CUBE :
									return image_type == VK_ VK_IMAGE_TYPE_2D && arrayLayers == 6;
									case VK_ VK_IMAGE_VIEW_TYPE_CUBE_ARRAY :
										return image_type == VK_ VK_IMAGE_TYPE_2D && arrayLayers > 6 && (arrayLayers % 6 == 0);
										case VK_ VK_IMAGE_VIEW_TYPE_3D :
											return image_type == VK_ VK_IMAGE_TYPE_3D;
										default:
											return false;
			}
		}

		static constexpr bool has_depth_aspect(VK_ VkFormat format) noexcept {
			switch (format) {
				case VK_ VK_FORMAT_D16_UNORM :
					case VK_ VK_FORMAT_X8_D24_UNORM_PACK32 :
						case VK_ VK_FORMAT_D32_SFLOAT :
							case VK_ VK_FORMAT_D16_UNORM_S8_UINT :
								case VK_ VK_FORMAT_D24_UNORM_S8_UINT :
									case VK_ VK_FORMAT_D32_SFLOAT_S8_UINT :
										return true;
									default:
										return false;
			}
		}

		static constexpr bool has_stencil_aspect(VK_ VkFormat format) noexcept {
			switch (format) {
				case VK_ VK_FORMAT_S8_UINT :
					case VK_ VK_FORMAT_D16_UNORM_S8_UINT :
						case VK_ VK_FORMAT_D24_UNORM_S8_UINT :
							case VK_ VK_FORMAT_D32_SFLOAT_S8_UINT :
								return true;
							default:
								return false;
			}
		}

		template<typename C, typename V>
		constexpr void operator()(C& create, V& view) const noexcept requires(index == 0u) {
			if constexpr (::std::same_as<C, VK_ VkImageCreateInfo> && ::std::same_as<V, VK_ VkImageViewCreateInfo>) {
				if (!type_compatible(view.viewType, create.imageType, create.arrayLayers)) {
					if (create.flags & VK_ VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) {
						view.viewType = (create.arrayLayers > 6) ? VK_ VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_ VK_IMAGE_VIEW_TYPE_CUBE;
					}
					else {
						switch (create.imageType) {
							case VK_ VK_IMAGE_TYPE_1D :
								view.viewType = (create.arrayLayers > 1) ? VK_ VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_ VK_IMAGE_VIEW_TYPE_1D;
								break;
							case VK_ VK_IMAGE_TYPE_2D :
								view.viewType = (create.arrayLayers > 1) ? VK_ VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_ VK_IMAGE_VIEW_TYPE_2D;
								break;
							case VK_ VK_IMAGE_TYPE_3D :
								view.viewType = VK_ VK_IMAGE_VIEW_TYPE_3D;
								break;
							default:
								break;
						}
					}
				}

				revise_format<>(create.format, view.format);

				auto& range = view.subresourceRange;
				if (range.aspectMask == 0) {
					if (create.usage & VK_ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
						if (has_depth_aspect(create.format)) {
							range.aspectMask |= VK_ VK_IMAGE_ASPECT_DEPTH_BIT;
						}
						if (has_stencil_aspect(create.format)) {
							range.aspectMask |= VK_ VK_IMAGE_ASPECT_STENCIL_BIT;
						}

						if (range.aspectMask == 0) {
							range.aspectMask = VK_ VK_IMAGE_ASPECT_DEPTH_BIT;
						}
					}
					else {
						range.aspectMask = VK_ VK_IMAGE_ASPECT_COLOR_BIT;
					}
				}

				if (range.baseMipLevel >= create.mipLevels) {
					// assert(!"view.baseMipLevel exceeds image mipLevels");
					range.baseMipLevel = 0;
				}

				const auto max_remaining_levels = create.mipLevels - range.baseMipLevel;
				if (range.levelCount == 0 || range.levelCount == VK_REMAINING_MIP_LEVELS) {
					range.levelCount = max_remaining_levels;
				}
				else if (range.levelCount > max_remaining_levels) {
					range.levelCount = max_remaining_levels;
				}

				if (range.baseArrayLayer >= create.arrayLayers) {
					// assert(!"view.baseArrayLayer exceeds image arrayLayers");
					range.baseArrayLayer = 0;
				}

				const auto max_remaining_layers = create.arrayLayers - range.baseArrayLayer;
				if (range.layerCount == 0 || range.layerCount == VK_REMAINING_ARRAY_LAYERS) {
					range.layerCount = max_remaining_layers;
				}
				else if (range.layerCount > max_remaining_layers) {
					range.layerCount = max_remaining_layers;
				}

				if constexpr (requires { revise_view_t<1u>()(create, view); }) {
					revise_view_t<1u>()(create, view);
				}
			}
			else if constexpr (::std::same_as<C, VK_ VkBufferCreateInfo> || ::std::same_as<V, VK_ VkBufferViewCreateInfo>) {
				if (view.range == 0) {
					view.range = create.size;
				}
				if (view.offset == 0) {
					view.offset = 0;
				}

				constexpr auto valid_texel_usages =
					VK_ VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
					VK_ VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;

				assert(bool(create.usage & valid_texel_usages) &&
					"[INTERNAL ERROR] Only texel buffer and storage buffer can make buffer view.");

				if constexpr (requires { revise_view_t<1u>()(create, view); }) {
					revise_view_t<1u>()(create, view);
				}
			}
			else if constexpr (requires { revise_view_t<1u>()(create, view); }) {
				revise_view_t<1u>()(create, view);
			}
		}
	};
	template<::std::size_t index = 0>
	constexpr revise_view_t<index> revise_view{};

	template<::std::size_t index = 0u>
	struct needs_descriptor_t {};

	template<>
	struct needs_descriptor_t<0u> {
		template<typename T>
		constexpr bool operator()(T flags) const noexcept {
			if constexpr (::std::same_as<T, VK_ VkPipelineStageFlags>) {
				constexpr VK_ VkPipelineStageFlags shader_stages =
					VK_ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
					VK_ VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
					VK_ VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
					VK_ VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
					VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
					VK_ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
					VK_ VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT |
					VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

				bool enabled = true;
				if constexpr (::std::invocable<needs_descriptor_t<1>, T>)
					enabled = needs_descriptor_t<1>()(flags);

				return enabled && (flags & shader_stages) != 0;
			}
			else if constexpr (::std::same_as<T, VK_ VkShaderStageFlags>) {
				return flags != static_cast<VK_ VkShaderStageFlags>(0);
			}
			else {
				return false;
			}
		}
	};

	template<::std::size_t index = 0u>
	constexpr needs_descriptor_t<index> needs_descriptor_set{};

	template<::std::size_t index = 0u>
	struct to_visibility_t {};
	template<>
	struct to_visibility_t<0u> {
		template<typename T>
		constexpr VK_ VkShaderStageFlags operator()(T flags) const noexcept {
			if constexpr (::std::same_as<T, VK_ VkPipelineStageFlags>) {
				VK_ VkShaderStageFlags visibility = 0;
				if (flags & VK_ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT) {
					visibility |= VK_ VK_SHADER_STAGE_VERTEX_BIT;
				}
				if (flags & VK_ VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT) {
					visibility |= VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
				}
				if (flags & VK_ VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT) {
					visibility |= VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
				}
				if (flags & VK_ VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT) {
					visibility |= VK_ VK_SHADER_STAGE_GEOMETRY_BIT;
				}
				if (flags & VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) {
					visibility |= VK_ VK_SHADER_STAGE_FRAGMENT_BIT;
				}
				if (flags & VK_ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
					visibility |= VK_ VK_SHADER_STAGE_COMPUTE_BIT;
				}

				if (flags & VK_ VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT) {
					visibility |= VK_ VK_SHADER_STAGE_ALL_GRAPHICS;
				}
				if (flags & VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) {
					visibility |= VK_ VK_SHADER_STAGE_ALL;
				}

				if constexpr (::std::invocable<to_visibility_t<1>, T>)
					visibility |= to_visibility_t<1>()(flags);

				return visibility;
			}
			else if constexpr (::std::same_as<T, VK_ VkShaderStageFlags>) {
				return flags;
			}
			else {
				return static_cast<VK_ VkShaderStageFlags>(0);
			}
		}
	};
	template<::std::size_t index = 0u>
	constexpr to_visibility_t<index> to_visiblity{};

	// duplicated element will be erased.

	template<::std::size_t index>
	struct decide_memory_properties_ {

		static constexpr auto impl_usage(view_usage::type usage) noexcept {
			using namespace view_usage;

			auto flags = VK_ VkMemoryPropertyFlags(0u);

			const bool has_cpu_read = bool(usage & cpu_read);
			const bool has_cpu_write = bool(usage & cpu_write);

			// const auto has_gpu_usage = bool(usage & (
			// 	uniform | shader_read | shader_write |
			// 	vertex | index | indirect |
			// 	sampled | depth_stencil |
			// 	render_target | input
			// ));

			if (has_cpu_read || has_cpu_write) {
				flags |= VK_ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
				if (has_cpu_write) {
					flags |= VK_ VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
				}
				if (has_cpu_read) {
					flags |= VK_ VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
				}
			}

			if (!has_cpu_read && !has_cpu_write) {
				flags |= VK_ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			}

			return flags;
		}

		static constexpr auto revise_flags(auto originalFlags) noexcept {
			auto fixedFlags = originalFlags;

			if (bool(fixedFlags & VK_ VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)) {
				fixedFlags &= ~(VK_ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					VK_ VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
					VK_ VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
				fixedFlags |= VK_ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			}

			if (bool(fixedFlags & VK_ VK_MEMORY_PROPERTY_PROTECTED_BIT)) {
				fixedFlags &= ~(VK_ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					VK_ VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
					VK_ VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
			}

			if (!(fixedFlags & VK_ VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
				fixedFlags &= ~(VK_ VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_ VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
			}
			else if (bool(fixedFlags & VK_ VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) &&
				bool(fixedFlags & VK_ VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) {
				fixedFlags &= ~VK_ VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
				// fixedFlags &= ~VK_ VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			}
			return fixedFlags;
		}

		template<typename T>
		constexpr auto operator()(T info)
			const noexcept requires(index == 0u) {
			auto flags = VK_ VkMemoryPropertyFlags(0u);

			constexpr auto invoked = ::std::invocable<decide_memory_properties_<index + 1u>, T>;
			if constexpr (invoked) {
				flags = decide_memory_properties_<1u>()(info);
			}

			if (flags == 0u) {
				if constexpr (requires { info.usages; }) {
					if constexpr (::std::same_as<decltype(info.usages), view_usage::type>) {
						flags = decide_memory_properties_::impl_usage(info.usages);
					}
				}
				else {
					static_assert(!invoked, "No need to call decide_memory_properties.");
				}
			}
			return flags;
		}
	};
	template<::std::size_t index = 0>
	constexpr decide_memory_properties_<index> decide_memory_properties{};

	template<::std::size_t index = 0>
	struct to_shader_stage_ {
		template<typename T>
		constexpr auto operator()(T pipeline_stages) const noexcept requires(index == 0u) {
			auto result = VK_ VkShaderStageFlags(0);

			if (pipeline_stages & VK_ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) {
				return static_cast<VK_ VkShaderStageFlags>(VK_ VK_SHADER_STAGE_ALL);
			}
			if (pipeline_stages & VK_ VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT) {
				return static_cast<VK_ VkShaderStageFlags>(VK_ VK_SHADER_STAGE_ALL_GRAPHICS);
			}

			if (pipeline_stages & VK_ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT) {
				result |= VK_ VK_SHADER_STAGE_VERTEX_BIT;
			}
			if (pipeline_stages & VK_ VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT) {
				result |= VK_ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			}
			if (pipeline_stages & VK_ VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT) {
				result |= VK_ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			}
			if (pipeline_stages & VK_ VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT) {
				result |= VK_ VK_SHADER_STAGE_GEOMETRY_BIT;
			}
			if (pipeline_stages & VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) {
				result |= VK_ VK_SHADER_STAGE_FRAGMENT_BIT;
			}
			if (pipeline_stages & VK_ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
				result |= VK_ VK_SHADER_STAGE_COMPUTE_BIT;
			}

			if constexpr (::std::invocable<decide_image_layout_<index + 1u>, T>) {
				result |= to_shader_stage_<index + 1u>()(pipeline_stages);
			}

			return result;
		}
	};
	template<::std::size_t index = 0>
	constexpr to_shader_stage_<0> to_shader_stage{};

	namespace bp {
		inline constexpr struct frame_count_ {
			template<typename T>
			constexpr auto operator()(T* pthis) const noexcept {
				using pure = ::std::remove_cv_t<T>;
				if constexpr (requires { pure::frame_count(); }) {
					return pure::frame_count();
				}
				else {
					return pthis->frame_count();
				}
			}
		} frame_count{};

		struct get_empty_ {
			static constexpr auto size(auto ptr) {
				return 0u;
			}
		};

		inline constexpr struct get_buffers_ : get_empty_ {
			static constexpr auto& handles(auto* ptr) noexcept
				requires (requires { ptr->buffers(); } || requires { ptr->buffers_; }) {
				if constexpr (requires { ptr->buffers(); }) {
					return ptr->buffers();
				}
				else {
					return ptr->buffers_;
				}
			}

			static constexpr auto size(auto const* ptr) noexcept
				requires (requires { ptr->buffer_size_; } || requires{ ptr->image_count();  }
			|| requires { ::std::ranges::size(handles(ptr)); }) {
				if constexpr (requires { ptr->buffer_size_; }) {
					return ptr->buffer_size_;
				}
				else if constexpr (requires { ptr->image_count(); }) {
					return ptr->image_count();
				}
				else {
					return::std::ranges::size(handles(ptr));
				}
			}

			static constexpr auto& indices(auto* ptr) noexcept
				requires (requires { ptr->buffer_indices_; }) {
				return ptr->buffer_indices_;
			}
		} get_buffers{};

		inline constexpr struct get_images_ : get_empty_ {
			static constexpr auto handles(auto* ptr) noexcept
				requires (requires { ptr->images(); } || requires { ptr->images_; }) {
				if constexpr (requires { ptr->images(); }) {
					return ptr->images();
				}
				else {
					return ptr->images_;
				}
			}

			static constexpr auto size(auto const* ptr) noexcept
				requires (requires { ptr->image_size_; } || requires { ptr->image_count(); }
			|| requires { ::std::ranges::size(handles(ptr)); }) {
				if constexpr (requires { ptr->image_size_; }) {
					return ptr->image_size_;
				}
				else if constexpr (requires { ptr->image_count(); }) {
					return ptr->image_count();
				}
				else {
					return::std::ranges::size(handles(ptr));
				}
			}

			static constexpr auto& indices(auto* ptr) noexcept
				requires (requires { ptr->image_indices_; }) {
				return ptr->image_indices_;
			}
		} get_images{};

		inline constexpr struct get_samplers_ : get_empty_ {
			static constexpr auto& handles(auto* ptr) noexcept
				requires (requires { ptr->samplers(); } || requires { ptr->samplers_; }) {
				if constexpr (requires { ptr->samplers(); }) {
					return ptr->samplers();
				}
				else {
					return ptr->samplers_;
				}
			}

			static constexpr auto size(auto const* ptr) noexcept
				requires (requires { ptr->sampler_count(); }) {
				return ptr->sampler_count();
			}
		} get_samplers{};

		inline constexpr struct get_set_ : get_empty_ {
			static constexpr auto& handles(auto* ptr) noexcept
				requires(requires { ptr->sets_; } || requires { ptr->sets(); }) {
				if constexpr (requires { ptr->sets_; }) {
					return ptr->sets_;
				}
				else {
					return ptr->sets();
				}
			}

			static constexpr auto size(auto const* ptr) noexcept
				requires (requires { ptr->set_size_; } || requires { ptr->set_count(); }) {
				if constexpr (requires{ ptr->set_size_; }) {
					return ptr->set_size_;
				}
				else if constexpr (){
					return ptr->set_count();
				}
			}
		} get_set{};

		inline constexpr struct get_set_layout_ : get_empty_ {
			static constexpr auto& handles(auto* ptr) noexcept
				requires(requires { ptr->set_layouts_; } || requires { ptr->set_layouts(); }) {
				if constexpr (requires { ptr->set_layouts_; }) {
					return ptr->set_layouts_;
				}
				else {
					return ptr->set_layouts();
				}
			}

			static constexpr auto size(auto const* ptr) noexcept
				requires (requires { ptr->set_layout_size_; } || requires { ptr->set_layout_count(); }) {
				if constexpr (requires{ ptr->set_layout_size_; }) {
					return ptr->set_layout_size_;
				}
				else {
					return ptr->set_layout_count();
				}
			}
		} get_set_layout{};

		inline constexpr struct get_handle_ {
			static constexpr auto&& direct(auto pthis, auto fn, auto frame_index, auto index) {
				if constexpr (requires {fn.indices(pthis); }) {
					return fn.handles(pthis)[fn.indices(pthis)[frame_index * fn.size(pthis) + index]];
				}
				else {
					return fn.handles(pthis)[frame_index * fn.size(pthis) + index];
				}
			}

			template<typename T>
			static constexpr auto only_direct() noexcept {
				if constexpr (requires {::std::remove_cv_t<T>::bind_points_index; }) {
					return::std::remove_cv_t<T>::bind_points_index != 0u;
				} 
				else {
					return false;
				}
			}

			template<typename T>
			constexpr auto&& operator()(T* pthis, auto fn, auto frame_index, auto index) const noexcept {
				if constexpr (only_direct<T>()) {
					const auto next_count = fn.size(pthis->next());
					assert(index < fn.size(pthis) + next_count); // index out of range.
					assert(frame_index < frame_count(pthis)); // frame index out of range.
					if (index >= next_count) {
						return direct(pthis, fn, frame_index, index - next_count);
					}
					else {
						return get_handle_()(pthis->next(), fn, frame_index, index);
					}
				}
				else {
					assert(index < fn.size(pthis) + fn.size(pthis->next())); // index out of range.
					assert(frame_index < frame_count(pthis)); // frame index out of range.
					return direct(pthis, fn, frame_index, index);
				}
			}
		} get_object{};

		inline constexpr struct get_buffer_view_ {
			static constexpr auto&& views(auto pthis)
				requires (requires { pthis->buffer_views(); } || requires { pthis->buffer_views_; })
			{
				if constexpr (requires { pthis->buffer_views(); }) {
					return pthis->buffer_views();
				}
				else {
					return (pthis->buffer_views_);
				}
			}

			static constexpr auto size(auto pthis)
				requires (requires { pthis->buffer_size(); }
			|| requires {::std::ranges::size(views(pthis)); }) {
				if constexpr (requires { pthis->buffer_size(); }) {
					return pthis->buffer_size();
				}
				else {
					return::std::ranges::size(views(pthis));
				}
			}

			static constexpr auto&& set_jump(auto pthis)
				requires (requires { pthis->buffer_set_jump(); } || requires { pthis->buffer_set_jump_; })
			{
				if constexpr (requires { pthis->buffer_set_jump(); }) {
					return pthis->buffer_set_jump();
				}
				else {
					return (pthis->buffer_set_jump_);
				}
			}
		} get_buffer_view{};

		inline constexpr struct get_image_view_ {
			static constexpr auto&& views(auto pthis)
				requires (requires { pthis->image_views(); } || requires { pthis->image_views_; })
			{
				if constexpr (requires { pthis->image_views(); }) {
					return pthis->image_views();
				}
				else {
					return (pthis->image_views_);
				}
			}

			static constexpr auto size(auto pthis)
				requires (requires { pthis->image_size(); } || requires { ::std::ranges::size(views(pthis)); })
			{
				if constexpr (requires { pthis->image_size(); }) {
					return pthis->image_size();
				}
				else {
					return::std::ranges::size(views(pthis));
				}
			}

			static constexpr auto&& set_jump(auto pthis)
				requires (requires { pthis->image_set_jump(); } || requires { pthis->image_set_jump_; })
			{
				if constexpr (requires { pthis->image_set_jump(); }) {
					return pthis->image_set_jump();
				}
				else {
					return (pthis->image_set_jump_);
				}
			}
		} get_image_view{};

		inline constexpr struct get_view_ {
			static constexpr auto&& direct(auto pthis, auto fn, auto frame_index, auto set, auto index) {
				return fn.handles(pthis)[fn.size(pthis) * frame_index + fn.set_jump(pthis)[set] + index];
			}
			template<typename T>
			constexpr auto&& operator()(T* pthis, auto fn, auto frame_index, auto set, auto index) const noexcept {
				if constexpr (::std::remove_cv_t<T>::bind_points_index != 0u) {
					const auto next_count = fn.size(pthis->next());
					assert(index < fn.size(pthis) + next_count); // index out of range.
					assert(frame_index < frame_count(pthis)); // frame index out of range.
					if (index >= next_count) {
						return direct(pthis, fn, frame_index, set, index - next_count);
					}
					else {
						return get_view_()(pthis->next(), fn, frame_index, set, index);
					}
				}
				else {
					assert(index < fn.size(pthis) + fn.size(pthis->next())); // index out of range.
					assert(frame_index < frame_count(pthis)); // frame index out of range.
					return direct(pthis, fn, frame_index, index);
				}
			}
		} get_view{};
	}

	using namespace bind_points_extensions;

	template<>
	struct meta_of<bind_points> {
		static constexpr auto type_id = make_type_id(BIND_POINTS_SCOPE, 0x0u);
		using order = order::at_middle;
		using extend = void;

		using buffer_view_store = unbinded<unattached<unused<VK_ VkBufferViewCreateInfo>>>;
		using image_view_store = unbinded<unattached<unused<VK_ VkImageViewCreateInfo>>>;

		template<typename T>
		struct queue_shared : T {
			constexpr queue_shared() = default;
			constexpr queue_shared(T const& info)
				: T{ info }
			{
			}

			constexpr void relocate() noexcept {
				this->queueFamilyIndexCount = ::std::uint32_t(queue_famlies.size());
				this->pQueueFamilyIndices = queue_famlies.data();
			}

			::std::vector<::std::uint32_t> queue_famlies;
		};
		using image_store = queue_shared<VK_ VkImageCreateInfo>;
		using buffer_store = queue_shared<VK_ VkBufferCreateInfo>;
		using sampler_store = VK_ VkSamplerCreateInfo;

		template<typename T>
		struct info : T {
			template<typename>
			friend struct info;

			constexpr info(auto&& infos)
				: T{ forward_(infos) } {
			}

			template<typename U>
			constexpr auto receive(U create)
				requires(!::std::is_void_v<decltype(::std::declval<info&>().template create_vec<U>())>) {
				return append_create(create_vec<U>(), ::std::move(create));
			}

			template<typename U>
			constexpr auto receive(unbinded<U> view)
				requires(!::std::is_void_v<decltype(::std::declval<info&>().template view_vec<U>())>) {
				auto inside = view.set > set_offset;
				if (inside) {
					auto size = view.set - set_offset;
					layouts.resize(size, VK_ VkDescriptorSetLayoutCreateInfo {
						.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
					});
				}
				append_binding(bindings, bindings_meta, view, set_offset);
				return append_view(create_vec<U>(), view_vec<U>(), last_visited<U>(), usage_fn<U>(), ::std::move(view));
			}
			template<typename U>
			constexpr auto receive(unattached<U> view)
				requires(!::std::is_void_v<decltype(::std::declval<info&>().template view_vec<U>())>) {
				return append_view(create_vec<U>(), view_vec<U>(), last_visited<U>(), usage_fn<U>(), ::std::move(view));
			}

			// new pass.
			constexpr auto receive() {
				buffer_last_pass_visited.emplace_back();
				image_last_pass_visited.emplace_back();
			}

			void relocate() {
				for (auto& buffer : buffers) {
					buffer.relocate();
				}
				for (auto& image : images) {
					image.relocate();
				}

				auto itb = bindings.begin();
				for (auto& layout : layouts) {
					layout.bindingCount = ::std::uint32_t(itb->size());
					layout.pBindings = itb->data();
					itb++;
				}
				T::relocate();
			}

			constexpr void setup_backward(infomation_of<swapchain> auto& other) {
				image_offset++;
			}

			constexpr void setup_backward(infomation_of<bind_points> auto& other) {
				if (bind_point_index <= other.bind_point_index) {
					bind_point_index = other.bind_point_index + 1u;
					buffer_offset = buffer_offset + other.buffers.size();
					image_offset = other.image_offset + other.images.size();
					sampler_offset = other.sampler_offset + other.samplers.size();
					buffer_view_offset = other.buffer_view_offset + other.buffer_views.size();
					image_view_offset = other.image_view_offset + other.image_views.size();
					set_offset = other.set_offset + other.layouts.size();
				}
			}

			constexpr void set_connectable() {
				bind_point_index = 0u;
				buffer_offset = 0u;
				image_offset = 0u;
				sampler_offset = 0u;
				buffer_view_offset = 0u;
				image_view_offset = 0u;
				set_offset = 0u;
				buffer_last_pass_visited.clear();
				image_last_pass_visited.clear();
			}

			::std::uint16_t
				bind_point_index = 0u,
				buffer_offset = 0u,
				image_offset = 0u,
				sampler_offset = 0u,
				buffer_view_offset = 0u,
				image_view_offset = 0u,
				set_offset = 0u;

			safe<::std::vector<buffer_store>> buffers;
			safe<::std::vector<image_store>> images;
			::std::vector<::std::vector<::std::uint16_t>>
				buffer_last_pass_visited, image_last_pass_visited;

			safe<::std::vector<sampler_store>> samplers;

			safe<::std::vector<image_view_store>> image_views;
			safe<::std::vector<buffer_view_store>> buffer_views;

			::std::vector<VK_ VkDescriptorSetLayoutCreateInfo> layouts;
			::std::vector<::std::vector<VK_ VkDescriptorSetLayoutBinding>> bindings;
			::std::vector<::std::vector<binding_block_info>> bindings_meta;

		protected:
			template<typename U>
			constexpr decltype(auto) create_vec() noexcept {
				if constexpr (::std::derived_from<U, VK_ VkBufferCreateInfo>
					|| ::std::derived_from<U, VK_ VkBufferViewCreateInfo>) {
					return (buffers);
				}
				else if constexpr (::std::derived_from<U, VK_ VkImageCreateInfo>
					|| ::std::derived_from<U, VK_ VkImageViewCreateInfo>) {
					return (images);
				}
				else if constexpr (::std::derived_from<U, VK_ VkSamplerCreateInfo>) {
					return (samplers);
				}
			}
			template<typename U>
			static constexpr auto usage_fn() noexcept {
				if constexpr (::std::derived_from<U, VK_ VkBufferCreateInfo>
					|| ::std::derived_from<U, VK_ VkBufferViewCreateInfo>) {
					return buffer_usage<>;
				}
				else if constexpr (::std::derived_from<U, VK_ VkImageCreateInfo>
					|| ::std::derived_from<U, VK_ VkImageViewCreateInfo>) {
					return image_usage<>;
				}
			}

			template<typename U>
			constexpr decltype(auto) view_vec() noexcept {
				if constexpr (::std::derived_from<U, VK_ VkImageViewCreateInfo>) {
					return (image_views);
				}
				else if constexpr (::std::derived_from<U, VK_ VkBufferViewCreateInfo>) {
					return (buffer_views);
				}
			}

			template<typename U>
			constexpr auto create_offset() noexcept {
				if constexpr (::std::derived_from<U, VK_ VkImageViewCreateInfo>) {
					return image_offset;
				}
				else if constexpr (::std::derived_from<U, VK_ VkBufferViewCreateInfo>) {
					return buffer_offset;
				}
			}

			template<typename U>
			constexpr decltype(auto) last_visited() noexcept {
				if constexpr (::std::derived_from<U, VK_ VkImageViewCreateInfo>) {
					return (image_last_pass_visited.back());
				}
				else if constexpr (::std::derived_from<U, VK_ VkBufferViewCreateInfo>) {
					return (buffer_last_pass_visited.back());
				}
			}


		private:
			template<typename U>
			static constexpr auto descriptor_type(auto usage) {
				if constexpr (::std::derived_from<::std::remove_cvref_t<U>, VK_ VkImageViewCreateInfo>) {
					return image_descriptor_type<>(usage);
				}
				else if constexpr (::std::derived_from<::std::remove_cvref_t<U>, VK_ VkBufferViewCreateInfo>) {
					return buffer_descriptor_type<>(usage);
				}
				else {
					return VK_ VK_DESCRIPTOR_TYPE_SAMPLER;
				}
			}

			constexpr auto inside(auto offset, auto size, auto index) {
				return offset <= index && index < offset + size;
			}

			static constexpr void append_descriptor(auto& binds, auto& metas, auto bind, auto meta) {
				insert_buffer_(descriptor_policy(), binds, metas, binds.begin(), metas.begin(), bind, meta);
			}

			template<typename V>
			static constexpr void append_binding(auto& bindings, auto& binding_meta, V const& view, auto offset) {
				if (offset > view.set || view.set == invalid) {
					return;
				}
				if (view.set >= bindings.size()) {
					bindings.resize(view.set);
					binding_meta.resize(view.set);
				}

				auto& binds = bindings[view.set];
				auto& metas = binding_meta[view.set];
				VK_ VkDescriptorSetLayoutBinding bind{
					.binding = view.binding,
					.descriptorType = descriptor_type<V>(view.usages),
					.descriptorCount = 1u,
					.stageFlags = to_shader_stage<>(view.stages),
					.pImmutableSamplers = nullptr,
				};

				::std::uint16_t sampler_index = invalid;
				if constexpr (requires { view.sampler_index; }) {
					sampler_index = view.sampler_index;
				}

				binding_block_info meta{
					.ori_indices{view.index},
					.sampler_indices = sampler_index == invalid
						? ::std::vector<::std::uint16_t>()
						: ::std::vector<::std::uint16_t>{sampler_index},
				};
				insert_buffer_(descriptor_policy(),
					binds, metas, binds.begin(), metas.begin(), bind, meta);
			}

			static constexpr auto view_skip(auto it, auto const& view) {
				if constexpr (requires{ view.set; }) {
					return it->set < view.set || (it->set == view.set && it->index < view.index);
				}
				else {
					return it->index < view.index;
				}
			}
			static constexpr auto append_view(auto& creates, auto& views, auto& last_visited, auto usage_fn, auto offset, auto view) {
				auto it = views.begin();
				while (it != view.end() && view_skip(it, view)) { it++; }
				if (it != view.end() && it->set != view.set && it->index != view.index && it->usages != view.usages) {
					it = views.insert(it, ::std::move(view));
					it->usages = view.usages;
					if constexpr (requires{ view.set; }) {
						it->set = view.set;
						it->binding = view.binding;
					}
					else {
						it->set = invalid;
						it->binding = invalid;
					}
					if constexpr (requires { view.sampler_index; }) {
						it->sampler_index = view.sampler_index;
					}
					else {
						it->sampler_index = invalid;
					}
				}

				if (view.index < creates.size() + offset) {
					creates[view.index - offset].usage = usage_fn(view.usages);
				}
				auto itlv = last_visited.begin();
				while (itlv != last_visited.end() && *itlv < view.index) { itlv++; }
				if (itlv == last_visited.end() || itlv != view.index) {
					last_visited.insert(itlv, view.index);
				}
				return::std::distance(views.begin(), it);
			}

			static constexpr auto create_skip(auto it, auto const& create) {
				if constexpr (requires { create.size; }) {
					return it->size < create.size;
				}
				else if constexpr (requires { create.extent; }) {
					return it->extent.width * it->extent.height * it->extent.depth * it->mipLevels * it->arrayLayers
						< create.extent.width * create.extent.height * create.extent.depth * it->mipLevels * it->arrayLayers;
				}
				else {
					return true;
				}
			}
			static constexpr auto create_emplace(auto& vec, auto& queues, auto it, auto itq, auto create) {
				it = vec.insert(it, ::std::move(create));
				itq = queues.insert(itq, {});
				return::std::distance(vec.begin(), it);
			}
			static constexpr auto append_create(auto& vec, auto& queues, auto create) {
				auto it = vec.begin();
				auto itq = queues.begin();
				while (it != vec.end() && create_skip(it, create)) { it++; itq++; }
				return create_emplace(vec, queues, it, itq, ::std::move(create));
			}
		};

		template<typename T>
		struct make : T {
			friend bp::get_buffers_;
			friend bp::get_images_;
			friend bp::get_buffer_view_;
			friend bp::get_image_view_;
			friend bp::get_view_;
			friend bp::get_handle_;

			using base = T;

			static constexpr::std::uint16_t bind_points_index = []() constexpr {
				if constexpr (have_parent_v<T, bind_points>) {
					return object_parent_t<T, bind_points>::bind_points_index + 1u;
				}
				else {
					return 0u;
				}
			}();

			template<infomation_of<bind_points> F>
			constexpr make(F&& info, auto&&...others)
				: T{ forward_(others)... } {
				auto hdv = T::template parent<device>()->device_handle();
				auto fc = info.frame_count;
				if constexpr (::std::invocable<bp::frame_count_, decltype(next())>) {
					fc = bp::frame_count(next());
				}

				buffer_size_ = ::std::uint16_t(info.buffers.size());
				buffers_.resize(buffer_size_ * fc);
				image_size_ = ::std::uint16_t(info.images.size());
				images_.resize(image_size_ * fc);

				samplers_.resize(info.samplers.size());

				buffer_view_size_ = ::std::uint16_t(info.buffer_views.size());
				buffer_views_.resize(buffer_view_size_ * fc);
				image_view_size_ = ::std::uint16_t(info.image_views.size());
				image_views_.resize(image_view_size_ * fc);

				try {
					bool first = true;
					::std::span buffers{ info.buffers };
					::std::span images{ info.images };
					for (auto fic = 0u; fic < fc; fic++) {
						auto local_buffers
							= ::std::span(buffers_.begin() + info.buffers.size() * fic, info.buffers.size());
						do_make(hdv, first, buffers, local_buffers, buffers_);

						auto local_images
							= ::std::span(images_.begin() + info.images.size() * fic, info.images.size());
						do_make(hdv, first, images, local_images , images_);

						first = false;
					}

					// unbind at back.

					auto ps = samplers_.begin();
					for (auto& cs : info.samplers) {
						VK_ vkCreateSampler(hdv, &cs, T::allocator(), &*ps++)
							| popup{ "[BINDPOINT] Create sampler failure." };
					}

					first = true;
					::std::span buffer_views{ info.buffer_views };
					::std::span image_views{ info.image_views };
					for (auto fic = 0u; fic < fc; fic++) {
						auto set_index{ 0u };
						for (; set_index < info.layouts.size(); set_index++) {
							if (first) {
								buffer_set_jump_.push_back(::std::uint16_t(buffer_views_.size()));
								image_set_jump_.push_back(::std::uint16_t(image_views_.size()));
							}
							do_make_view(hdv, first, set_index, fic, buffer_views, buffer_views_);
							do_make_view(hdv, first, set_index, fic, image_views, image_views_);
						}
						first = false;
					}

					buffer_set_jump_.push_back(::std::uint16_t(buffer_views_.size()));
					image_set_jump_.push_back(::std::uint16_t(image_views_.size()));
				}
				catch (...) {
					clear();
					throw;
				}
			}

			~make() { clear(); }

			constexpr auto buffer_count(::std::uint32_t = 0u) const noexcept {
				return next_count(bp::get_buffers) + buffer_size_;
			}
			constexpr auto& buffer(::std::uint32_t frame_index, ::std::uint32_t index) noexcept {
				return bp::get_object(this, bp::get_buffers, frame_index, index);
			}
			constexpr auto& buffer(::std::uint32_t frame_index, ::std::uint32_t index) const noexcept {
				return bp::get_object(this, bp::get_buffers, frame_index, index);
			}

			constexpr auto image_count(::std::uint32_t = 0u) const noexcept {
				return next_count(bp::get_images) + buffer_size_;
			}
			constexpr auto& image(::std::uint32_t frame_index, ::std::uint32_t index) noexcept {
				return bp::get_object(this, bp::get_images, frame_index, index);
			}
			constexpr auto& image(::std::uint32_t frame_index, ::std::uint32_t index) const noexcept {
				return bp::get_object(this, bp::get_images, frame_index, index);
			}

			constexpr auto sampler_count() const noexcept {
				return next_count(bp::get_samplers) + ::std::uint16_t(samplers_.size());
			}
			constexpr auto& sampler(::std::uint32_t index) const noexcept {
				return bp::get_object(this, bp::get_samplers, 0u, index);
			}

			constexpr auto buffer_view_count() const noexcept { return buffer_view_size_; }
			constexpr decltype(auto) buffer_view(::std::uint32_t frame_index,
				::std::uint32_t set, ::std::uint32_t index) const noexcept {
				return bp::get_view(this, bp::get_buffer_view, frame_index, set, index);
			}

			constexpr auto image_view_count() const noexcept { return image_view_size_; }
			constexpr decltype(auto) image_view(::std::uint32_t frame_index,
				::std::uint32_t set, ::std::uint32_t index) const noexcept {
				return bp::get_view(this, bp::get_image_view, frame_index, set, index);
			}

			constexpr auto frame_count() const noexcept {
				if constexpr (::std::invocable<bp::frame_count_, decltype(next())>) {
					return bp::frame_count(next());
				}
				else {
					return buffers_.size() / buffer_size_;
				}
			}

			void send(event::update_buffer_view update) {
				base::send(update);
			}

			void send(event::update_image_view update) {
				base::send(update);
			}

		protected:
			::std::uint16_t buffer_size_;
			::std::uint16_t image_size_;
			::std::uint16_t buffer_view_size_;
			::std::uint16_t image_view_size_;

			::std::vector<buffer_handle> buffers_;
			::std::vector<image_handle>  images_;
			::std::vector<VK_ VkSampler> samplers_;

			::std::vector<::std::uint16_t>    buffer_set_jump_;
			::std::vector<buffer_view_handle> buffer_views_;
			::std::vector<::std::uint16_t>    image_set_jump_;
			::std::vector<image_view_handle>  image_views_;

		private:
			constexpr auto next_count(auto fn) const noexcept {
				if constexpr (requires { fn.size(next()); }) {
					return fn.size(next());
				}
				else {
					return 0u;
				}
			}
			constexpr auto next() const noexcept {
				if constexpr (have_parent_v<T, swapchain>) {
					return T::template parent<swapchain>();
				}
				else {
					return T::template parent<bind_points>();
				}
			}

			void clear() {
				auto hdv = T::template parent<device>()->device_handle();
				for (auto const& view : inplace_unique(buffer_views_)) {
					VK_ vkDestroyBufferView(hdv, view, T::allocator());
				}
				for (auto const& view : inplace_unique(image_views_)) {
					VK_ vkDestroyImageView(hdv, view, T::allocator());
				}
				for (auto const& sampler : inplace_unique(samplers_)) {
					VK_ vkDestroySampler(hdv, sampler, T::allocator());
				}
				for (auto const& buffer : inplace_unique(buffers_)) {
					VK_ vkDestroyBuffer(hdv, buffer, T::allocator());
				}
				for (auto const& image : inplace_unique(images_)) {
					VK_ vkDestroyImage(hdv, image, T::allocator());
				}
			}

			// dst_span is frame index's subspan.
			template<typename R>
			auto do_make(auto hdv, bool first, R& src, auto& dst) {
				auto pdst = dst.begin();
				auto index{ 0u };
				for (auto const& create : src) {

					// maybe more batch.

					// if (!first && (create.usages & resource_attrs::is_static)) {
					// 	indices.emplace_back(indices[index]);
					// 	continue;
					// }

					using value_type = ::std::remove_cvref_t<::std::ranges::range_value_t<R>>;
					if constexpr (::std::derived_from<value_type, VK_ VkBufferCreateInfo>) {
						VK_ vkCreateBuffer(hdv, &create, T::allocator(), &*pdst++)
							| popup("[BINDPOINT] Create buffer failure.");

						auto& access = pdst->accesses.emplace_back();
						access.offset = 0u;
						access.size = create.size;
					}
					else if constexpr (::std::derived_from<value_type, VK_ VkImageCreateInfo>) {
						VK_ vkCreateImage(hdv, &create, T::allocator(), &*pdst++)
							| popup("[BINDPOINT] Create image failure.");

						auto& access = pdst->accesses.emplace_back();
						static_cast<VK_ VkImageSubresourceRange&>(access)
							= VK_ VkImageSubresourceRange{
								.aspectMask = to_aspect_mask<>(create.usages),
								.levelCount = create.mipLevels,
								.layerCount = create.arrayLayers,
						};
						access.layout = create.initialLayout;
					}

					pdst->flags = create.flags;
					pdst->share_mode = create.sharingMode;
					pdst->usages = create.usages;

					indices.emplace_back(::std::uint32_t(dst.size() - 1u));
					index++;
				}
			}

			template<typename T>
			auto do_make_view_local(auto hdv
				, bool first
				, auto frame_index
				, auto src // buffer/image view infos.
				, auto local // frame local (buffer/image)_views_ 
				, auto& whole // all (buffer/image)_views_
			) {
				auto pdst = local.begin();
				auto index{ 0u };
				for (auto temp : src) {

					// maybe more batch.

					// if (!first && (temp.usages & resource_attrs::is_static)) {
					// 	*pdst = whole.at(index);
					// 	continue;
					// }

					// Cannot create dynamic view on static handle.
					// assert(!((temp.usages & view_usage::is_static) 
					//	^ (hsrc[temp.index] & view_usage::is_static)));

					using value_type = ::std::ranges::range_value_t<::std::remove_cvref_t<decltype(src)>>;
					if constexpr (::std::derived_from<value_type, VK_ VkImageViewCreateInfo>) {
						pdst->handle.value = temp.image = image(frame_index, temp.index);
						VK_ vkCreateImageView(hdv, &temp, T::allocator(), &*pdst++)
							| popup{ "[BINDPOINT] Create image view failure." };
						pdst->range = temp.subresourceRange;
					}
					else if constexpr (::std::derived_from<value_type, VK_ VkBufferViewCreateInfo>) {
						pdst->handle.value = temp.buffer = buffer(frame_index, temp.index);
						if (temp.usages & view_usage::texel) {
							VK_ vkCreateBufferView(hdv, &temp, T::allocator(), &*pdst++)
								| popup{ "[BINDPOINT] Create buffer view failure." };
						}
						pdst->offset = temp.offset;
						pdst->size = temp.size;
					}

					index++;
				}
			}

			template<typename T>
			auto do_make_view(auto hdv
				, bool first
				, auto set_index
				, auto frame_index
				, ::std::vector<T> const& src // buffer/image view infos.
				, auto& whole // all (buffer/image)_views_
			) {
				auto begin = src.begin();
				auto end = begin + 1u;
				while (begin != src.end() && begin->set != set_index) { begin++; }
				while (end != src.end() && end->set == set_index) { end++; }
				auto local = ::std::span{ whole }
					.subspan(src.size() * frame_index + ::std::distance(src.begin(), begin), ::std::distance(begin, end));
				do_make_view_local(hdv, first, frame_index, src, local, whole);
			}
		};
	};

	template<>
	struct meta_of<bind_static> {
		static constexpr auto type_id = make_type_id(BIND_POINTS_SCOPE, 0x00011);
		static constexpr auto name = fixed_string{ "bind_static" };
		using order = order::at_middle;
		using extend = bind_points;

		template<typename T>
		using info = T;

		struct mem_index {
			::std::uint32_t memory_index;
		};

		struct mem_item {
			::std::uint32_t buffer_mem_idx = invalid;
			::std::uint32_t image_mem_idx;

			::std::size_t size;
			::std::size_t alignment;

			::std::uint32_t ori_index;
			bool is_optimal = false;
		};

		template<typename T>
		struct make : T {
			friend bp::get_set_;
			friend bp::get_set_layout_;

			template<infomation_of<bind_points> F>
			make(F&& info, auto&&...infos) : T{ forward_(info), forward_(infos)... } {
				auto fc = T::frame_count();
				auto pdv = T::template parent<device>();
				auto hdv = pdv->device_handle();
				auto phydv = pdv->physical_device();
				auto phydv_props = phydv.get_property(properties{});
				auto bindings = forward_like<F>(info.bindings);
				auto layouts = forward_like<F>(info.layouts);

				::std::span buffers_span{ info.buffers };
				::std::span images_span{ info.images };
				::std::span bindings_meta_span{ info.bindings_meta };
				::std::span buffer_views_span{ info.buffer_views };
				::std::span image_views_span{ info.image_views };

				try {
					auto mem_props = phydv.get_property(memory_properties{});
					allocate_and_bind_memory(hdv,
						mem_props.memory_properties,
						phydv_props.properties,
						buffers_span,
						images_span
					);
					if (buffer_views_span.size() || image_views_span.size()) {
						allocate_set(hdv, phydv_props, layouts, bindings, info.bindings_meta);
						write_descriptor_set(hdv,
							phydv_props.properties,
							layouts,
							bindings_meta_span,
							buffers_span,
							images_span,
							buffer_views_span,
							image_views_span
						);
					}
				}
				catch (...) {
					clear();
					throw;
				}
			}

			~make() { clear(); }

			constexpr auto set_count() { return bp::get_set.size(next()) + set_layouts_.size(); }
			// get descriptor set.
			constexpr decltype(auto) set(::std::uint16_t frame_index, ::std::uint16_t index) const noexcept {
				return bp::get_object(this, bp::get_set, frame_index, index);
			}
			constexpr decltype(auto) set_layout(::std::uint16_t index) const noexcept {
				return bp::get_object(this, bp::get_set_layout, 0u, index);
			};

		protected:
			using T::next;

			move_only<VK_ VkDescriptorPool> pool_;
			::std::vector<VK_ VkDescriptorSetLayout> set_layouts_;
			::std::vector<VK_ VkDescriptorSet> sets_;

			::std::vector<VK_ VkDeviceMemory> memories_;
			::std::vector<range<mem_index>> buffer_infos_; // buffer to memory index and offset.
			::std::vector<range<mem_index>> image_infos_; // image to memory index and offset.

		private:
			void clear() {
				auto hdv = T::template parent<device>()->device_handle();
				if (pool_.value) {
					VK_ vkDestroyDescriptorPool(hdv, pool_, T::allocator());
					for (auto lay : dlays_) {
						VK_ vkDestroyDescriptorSetLayout(hdv, lay, T::allocator());
					}
				}
				for (auto mem : memories_) {
					VK_ vkFreeMemory(hdv, mem, T::allocator());
				}
			}

			constexpr static auto make_pool_info(auto const& layouts // vector<VkDescriptorSetLayoutCreateInfo>
				, ::std::vector<VK_ VkDescriptorPoolSize>& sizes 
				, auto frame_count = 1u) {
				for (const auto& layout : layouts) {
					for (const auto& binding : ::std::span(layout.pBindings, layout.bindingCount)) {
						auto it = sizes.begin();
						while (it != sizes.end() && it->type != binding.descriptorType) { it++; }
						if (it == sizes.end()) {
							it = sizes.insert(it, VK_ VkDescriptorPoolSize {
								.type = binding.descriptorType,
								.descriptorCount = binding.descriptorCount * frame_count
							});
						}
						else {
							it->descriptorCount += binding.descriptorCount * frame_count;
						}
					}
				}

				return VK_ VkDescriptorPoolCreateInfo{
					.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
					.maxSets = ::std::uint32_t(layouts.size()) * frame_count,
					.poolSizeCount = ::std::uint32_t(sizes.size()),
					.pPoolSizes = sizes.data()
				};
			}

			void allocate_set(VkDevice device
				, auto const& props
				, auto& layouts
				, auto& bindings
				, auto const& meta) {
				auto fc = T::frame_count();
				set_layouts_.resize(layouts.size()); {
					auto pdl = set_layouts_.data();
					auto itb = bindings.begin(); // vector<VK_ VkDescriptorSetLayoutBinding>
					auto itm = meta.begin(); // vector<binding_block_info> 
					auto itl = layouts.begin(); // VK_ VkDescriptorSetLayoutCreateInfo.
					for (; itl != layouts.end(); itb++, itm++, itl++) {
						auto itbb = itb->begin();
						auto itmm = itm->begin();
						::std::vector<VK_ VkSampler> samplers;
						for (; itbb != itb->end(); itbb++, itmm++) {
							auto begin = samplers.size();
							samplers.reserve(begin + itmm->sampler_indices.size());
							for (auto index : itmm->sampler_indices) {
								samplers.emplace_back(this->sampler(index));
							}
							itbb->pImmutableSamplers = samplers.data() + begin;
						}
						itl->pBindings = itb->data();
						itl->bindingCount = ::std::uint32_t(itb->size());
						VK_ vkCreateDescriptorSetLayout(device, &*itl, T::allocator(), pdl++)
							| popup{ "[BINDPOINT] Descriptor set layout create failure." };
					}
				}

				::std::vector<VK_ VkDescriptorPoolSize> sizes;
				auto cinfo = make_pool_info(layouts, sizes, fc);
				VK_ vkCreateDescriptorPool(device, &cinfo, T::allocator(), &pool_)
					| popup{ "[BINDPOINT] Cannot create descriptor pool." };

				sets_.resize(layouts.size() * fc);
				auto dlays_size = ::std::uint32_t(set_layouts_.size());
				while (fc--) {
					VK_ VkDescriptorSetAllocateInfo alloc_info{
						.sType = VK_ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
						.descriptorPool = pool_,
						.descriptorSetCount = dlays_size,
						.pSetLayouts = set_layouts_.data()
					};
					VK_ vkAllocateDescriptorSets(device, &alloc_info, sets_.data() + fc * set_layouts_.size())
						| popup("[BINDPOINT] Allocate descriptor sets failure.");
				}
			}

			template<typename U>
			void decide_single(VkDevice device
				, auto const& props
				, auto& mem_ids // single object's memory infomations.
				, auto& items   // 
				, ::std::span<U const> creates // create infos.
				, auto const& handles // object handles,
				, auto fc // frame count,
				, auto getter // memory getter.
			) {
				auto itc = creates.begin();
				auto ith = handles.begin();
				for (; itc != creates.end(); itc++, ith++) {
					if constexpr (requires { itc->width; }) {
						assert(!(itc->flags
							& (VK_ VK_IMAGE_CREATE_SPARSE_ALIASED_BIT
								| VK_ VK_IMAGE_CREATE_SPARSE_BINDING_BIT
								| VK_ VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT))); // bind static is not allow sparse binding.
						assert(!(itc->flags & VK_ VK_IMAGE_CREATE_ALIAS_BIT)); // bind static is not support resource alias.
					}
					else {
						assert(!(itc->flags
							& (VK_ VK_BUFFER_CREATE_SPARSE_ALIASED_BIT
								| VK_ VK_BUFFER_CREATE_SPARSE_BINDING_BIT
								| VK_ VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT))); // bind static is not allow sparse binding.
					}


					VK_ VkMemoryRequirements req;
					getter(device, *ith, &req);

					auto flags = decide_memory_properties<>(*itc);

					auto index{ 0u };
					for (auto c : ::std::span{ props.memoryTypes, props.memoryTypeCount }) {
						if ((c.propertyFlags & flags) == flags && bool(req.memoryTypeBits & (1 << index))) {
							break;
						}
						index++;
					}
					if (index >= props.memoryTypeCount) {
						VK_ VK_ERROR_FEATURE_NOT_PRESENT | popup{ "[BINDPOINTS] No device index is supported for some resource." };
					}

					bool is_optimal = false;
					if constexpr (::std::derived_from<U, VK_ VkImageCreateInfo>) {
						is_optimal = itc->tiling == VK_ VK_IMAGE_TILING_OPTIMAL;
					}

					auto itm = mem_ids.begin();
					auto iti = items.begin();

					while (itm != mem_ids.end() && *itm < index) {
						itm++; iti++;
					}
					if (itm == mem_ids.end() || *itm != index) {
						itm = mem_ids.insert(itm, index);
						iti = items.insert(iti, {});
					}

					auto it = iti->begin();
					while (it != iti->end() && is_optimal != it->is_optimal && it->size < req.size) {
						it++;
					}
					for (auto i = 0u; i < fc; i++) {
						it = iti->insert(it, mem_item{
							.alignment = req.alignment,
							.size = req.size,
							.is_optimal = is_optimal,
							.ori_index = ::std::distance(handles.begin(), ith) + i * creates.size(),
						});

						if constexpr (::std::derived_from<U, VK_ VkBufferCreateInfo>) {
							it->buffer_mem_idx = index;
						}
						else if constexpr (::std::derived_from<U, VK_ VkImageCreateInfo>) {
							it->image_mem_idx = index;
						}
					}
				}
			}

			template<typename BC, typename IC>
			void allocate_and_bind_memory(VK_ VkDevice device
				, VK_ VkPhysicalDeviceMemoryProperties const& props
				, VK_ VkPhysicalDeviceProperties const& pdvprops
				, auto buffer_create_infos
				, auto image_create_infos) {
				auto& buffers = T::buffers_;
				auto& images = T::images_;
				buffer_infos_.resize(buffers.size());
				image_infos_.resize(images.size());

				::std::vector<::std::uint32_t> mem_ids;
				::std::vector<::std::vector<mem_item>> items;

				const auto fc = T::frame_count();
				make::decide_single(device
					, mem_ids, items
					, props, buffer_create_infos
					, buffers, fc, &VK_ vkGetBufferMemoryRequirements);
				make::decide_single(device
					, mem_ids, items
					, props, image_create_infos
					, images, fc, &VK_ vkGetImageMemoryRequirements);

				::std::vector<::std::uint32_t> mem_idx_map; // memory idx to mem_item's index.
				for (auto index = 0u; auto idx : mem_ids) {
					if (index >= mem_idx_map.size()) {
						mem_idx_map.resize(index + 1u);
					}
					mem_idx_map[idx] = index;
				}

				::std::vector<::std::size_t> sizes(mem_ids.size());
				bool last_is_optimal = false;
				auto its = sizes.begin();
				auto itm = mem_ids.begin();
				auto ititms = items.begin();
				for (; its != sizes.end(); itm++, its++, ititms++) {
					for (auto ititm = ititms->begin(); ititm != ititms->end(); ititm++) {
						auto offset = *its;

						if (last_is_optimal != ititm->is_optimal) {
							offset = align_up(offset, pdvprops.limits.bufferImageGranularity);
						}
						offset = align_up(offset, ititm->alignment);

						auto infos = &buffer_infos_;
						auto mem_idx = ititm->buffer_mem_idx;
						if (mem_idx == invalid) {
							infos = &image_infos_;
							mem_idx = ititm->image_mem_idx;
						}

						auto& info = infos->operator[](ititm->ori_index);
						info.memory_index = mem_idx;
						info.offset = offset;
						info.count = ititm->size;

						last_is_optimal = ititm->is_optimal;
						*its = offset + ititm->size;
					}
				}

				auto& device_memories = this->memories_;
				device_memories.resize(mem_ids.size());

				auto it_size = sizes.begin();
				auto it_id = mem_ids.begin();
				auto it_mem = device_memories.begin();
				for (; it_size != sizes.end(); ++it_size, ++it_id, ++it_mem) {
					auto total_size = *it_size;
					auto mem_type_index = *it_id;

					if (total_size == 0) continue; // TODO: maybe never occured.

					VK_ VkMemoryAllocateInfo alloc_info{
						.sType = VK_ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
						.pNext = nullptr,
						.allocationSize = total_size,
						.memoryTypeIndex = mem_type_index
					};
					VK_ vkAllocateMemory(device, &alloc_info, nullptr, &(*it_mem))
						| popup{ "[BIND POINTS] Allocate memory failure." };
				}

				auto it_id_check = mem_ids.begin();
				auto ititms_check = items.begin();
				for (auto i = 0u; i < mem_ids.size(); ++i, ++it_id_check, ++ititms_check) {
					for (auto const& item : *ititms_check) {
						auto infos = &buffer_infos_;
						if (item.buffer_mem_idx == invalid) {
							infos = &image_infos_;
						}

						auto& info = infos->operator[](item.ori_index);
						info.memory_index = i;
					}
				}

				auto it_buf = buffers.begin();
				auto it_info = buffer_infos_.begin();
				for (; it_buf != buffers.end(); ++it_buf, ++it_info) {
					VK_ vkBindBufferMemory(
						device,
						*it_buf,
						device_memories[it_info->memory_index],
						it_info->offset
					);
				}

				auto it_img = images.begin();
				auto it_img_info = image_infos_.begin();
				for (; it_img != images.end(); ++it_img, ++it_img_info) {
					VK_ vkBindImageMemory(
						device,
						*it_img,
						device_memories[it_img_info->memory_index],
						it_img_info->offset
					);
				}
			}

			template<typename BC, typename IC, typename BV, typename IV>
			void write_descriptor_set(VK_ VkDevice device
				, VK_ VkPhysicalDeviceProperties const& pdvprops
				, auto const& layouts // layout create infos.
				, auto const& map
				, auto buffers
				, auto images
				, auto buffer_views
				, auto image_views) {
				const auto fc = T::frame_count();
				::std::vector<VK_ VkWriteDescriptorSet> writes;
				::std::vector<::std::vector<VK_ VkDescriptorBufferInfo>> buffer_infos;
				::std::vector<::std::vector<VK_ VkBufferView>> tbuffer_views; // texel buffers.
				::std::vector<::std::vector<VK_ VkDescriptorImageInfo>> image_infos;
				for (auto fi = 0u; fi < fc; fi++) {
					auto itly = layouts.begin();
					auto itmap = map.begin() + layouts.size() * fi;
					for (; itly != layouts.end(); itmap++, itly++) {
						auto itme = itmap->begin();
						for (auto& binding : ::std::span{ itly->pBindings, itly->bindingCount }) {
							auto& write = writes.emplace_back(VK_ VkWriteDescriptorSet{
								.sType = VK_ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
								.dstSet = this->sets_[::std::distance(layouts.begin(), itly)],
								.dstBinding = binding.binding,
								.descriptorCount = binding.descriptorCount,
								.descriptorType = binding.descriptorType,
							});

							auto& indices = *itme++;
							if (buffer_descriptor_type<>(binding.descriptorType)) {
								auto& buf_infos = buffer_infos.emplace_back();

								::std::vector<VK_ VkBufferView>* tbuf_views = nullptr;
								if (binding.descriptorType == VK_ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
									|| binding.descriptorType == VK_ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
									tbuf_views = &tbuffer_views.emplace_back();
								}

								for (auto idx : indices) {
									auto& view = buffer_views[idx];
									auto buffer_idx = view.index + buffers.size() * fi;
									buf_infos.push_back(VK_ VkDescriptorBufferInfo {
										.buffer = T::buffer(buffer_idx),
										.offset = view.offset,
										.range = view.count,
									});
									if (tbuf_views) {
										tbuf_views->push_back(T::buffer_view(idx + buffer_views.size() * fi));
									}
								}

								write.pBufferInfo = buf_infos.data();
								if (tbuf_views) {
									write.pTexelBufferView = tbuf_views->data();
								}
							}
							else {
								auto& imgs = images.emplace_back();
								for (auto idx : indices) {
									auto& view = image_views.at(idx);
									imgs.emplace_back(VK_ VkDescriptorImageInfo {
										.sampler = view.sampler_index == invalid ? T::sampler(view.sampler_index) : nullptr,
										.imageView = T::image_view(idx + fi * image_views.size()),
										.layout = to_image_layout<>(view.usages),
									});
								}
								write.pImageInfo = imgs.data();
							}
						}
					}
				}

				VK_ vkUpdateDescriptorSets(device, 
					::std::uint32_t(writes.size()), writes.data(), 0u, nullptr);
			}
		};
	};

	template<>
	struct meta_of<bind_dynamic> {
		static constexpr auto type_id = make_type_id(BIND_POINTS_SCOPE, 0x0050);
		static constexpr auto name = fixed_string{"bind_dynamic"};

		using order = order::at_middle;
		using extend = bind_points;

		template<typename T>
		struct info : T {
			static_assert(always_false<T>, "bind dynamic is not designed yet.");
		};
	};

	using namespace buffer_extensions;
	template<>
	struct meta_of<buffer> {
		static constexpr auto type_id = make_type_id(BIND_POINTS_SCOPE, 0x1000);
		static constexpr auto name = fixed_string{ "buffer" };
		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct info : control_connectable<T> {
			using base = control_connectable<T>;
			using resource_tag = void;

			constexpr info(buffer const& info, auto const& infos)
				: base{ infos }
				, buffer_info{} {
				buffer_info.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				buffer_info.size = info.size;
			}
			constexpr info(auto const& infos) : info{ get_by<buffer>(infos), infos } {}

			constexpr void setup(infomation_of<bind_points> auto& bp_info) {
				if (this->connectable()) {
					bp_info.receive(buffer_info);
					this->set_connectable(false);
				}
			}

			constexpr auto& create() noexcept { return buffer_info; }
			constexpr auto& create() const noexcept { return buffer_info; }

			transformed<unused<VK_ VkBufferCreateInfo>, T> buffer_info;
		};

		template<typename T>
		using make = skipped_make<T>;
	};

	using namespace image_extensions;
	template<>
	struct meta_of<image> {
		static constexpr auto type_id = make_type_id(BIND_POINTS_SCOPE, 0x2000);
		static constexpr auto name = fixed_string{"image"};
		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct info : control_connectable<T> {
			using resource_tag = void;

			constexpr info(image const& info, auto const& infos)
				: T{ infos }
				, image_info{} {
				auto const is_1d = (info.height == ::std::uint32_t(-1));
				auto const is_2d = (!is_1d && info.depth == ::std::uint16_t(-1));
				auto const image_type = is_1d
					? VK_ VK_IMAGE_TYPE_1D
					: is_2d
					? VK_ VK_IMAGE_TYPE_2D
					: VK_ VK_IMAGE_TYPE_3D;
				auto const actual_height = is_1d ? 1u : info.height;
				auto const actual_depth = (is_1d || is_2d) ? 1u : static_cast<::std::uint32_t>(info.depth);

				image_info.sType = VK_ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				image_info.pNext = nullptr;
				image_info.format = VK_ VK_FORMAT_R8G8B8A8_UNORM;
				image_info.mipLevels = 1;
				image_info.arrayLayers = 1;
				image_info.samples = VK_ VK_SAMPLE_COUNT_1_BIT;
				image_info.tiling = VK_ VK_IMAGE_TILING_OPTIMAL;
				image_info.extent.height = actual_height;
				image_info.extent.depth = actual_depth;
				image_info.imageType = image_type;
			}
			constexpr info(auto const& infos) : info(get_by<image>(infos), infos) {}

			constexpr void receive(VK_ VkFormat format) noexcept { image_info.format = format; }

			constexpr void setup(infomation_of<bind_points> auto& bp) {
				if (this->connectable()) {
					bp.receive(image_info);
					this->set_connectable(false);
				}
			}

			constexpr auto& create() noexcept { return image_info; }
			constexpr auto& create() const noexcept { return image_info; }

			transformed<unused<VK_ VkImageCreateInfo>, T> image_info;
		};

		template<typename T>
		using make = skipped_make<T>;
	};

	template<typename T>
		requires(requires { typename T::resource_tag; })
	struct meta_of<is_static>::info<T> : T {
		constexpr info(auto&& info) : T{ forward_(info) } {
			this->create().usages |= resource_attrs::is_static;
		}
	};


}

#if !defined(VK_NO_WINDOWS)

namespace vktl::detail {
	using namespace swapchain_extensions;
	struct swapchain_image_handle : image_handle {
		::std::uint16_t frame_index = 0u;
	};
	template<>
	struct meta_of<swapchain> {
		static constexpr auto type_id = make_type_id(BIND_POINTS_SCOPE, 0x50000u);
		static constexpr auto name = fixed_string{ "swapchain" };

		using extend = void;
		using order = order::at_middle;

		static constexpr auto device_extensions = ::std::array{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		template<typename T>
		struct info : T {
			constexpr info(swapchain const& sw, auto&& info)
				: T{ forward_(info) }
				, swapchain{
					.sType = VK_ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
					.minImageCount = sw.min_frame_count,
					.imageExtent = {
						.width = sw.width,
						.height = sw.height
					},
					.imageArrayLayers = 1,
					.imageUsage = VK_ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // some may allow depth stencil, but this is normal usage.
					.imageSharingMode = VK_ VK_SHARING_MODE_EXCLUSIVE,
					.preTransform = VK_ VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
					.compositeAlpha = VK_ VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
					.presentMode = VK_ VK_PRESENT_MODE_FIFO_KHR,
					.clipped = VK_TRUE,
				} {
			}
			constexpr info(auto&& infos)
				: info(get_by<T>(infos), forward_(infos))
			{
			}

			constexpr void receive(VK_ VkFormat format) { swapchain.imageFormat = format; }

			constexpr void connect(infomation_of<device> auto& info) {
				info.append_extensions(device_extensions);
			}

			VK_ VkSwapchainCreateInfoKHR swapchain;
			::std::uint16_t format_index = invalid;
			::std::uint16_t present_mode_index = invalid;
			::std::uint16_t fence_index = invalid;
			::std::uint16_t semaphore_index = invalid;
		};

		template<typename T>
		struct make : T {
			template<infomation_of<swapchain> F>
			make(F&& info, auto&&...others)
				: T{ forward_(others)... } {
				auto win = T::template parent<window>();
				auto dv = T::template parent<device>();

				auto phydv = dv->physical_device();
				auto formats = phydv.get_property(surface_format{ win->surface_handle() });
				auto present_modes = phydv.get_property(surface_present_mode{ win->surface_handle() });

				auto hdv = dv->device_handle();
				auto swapchain = info.swapchain;
				::std::uint32_t
					format_index = info.format_index == invalid ? 0u : info.format_index,
					present_mode = info.present_mode_index == invalid ? 0u : info.present_mode_index;

				swapchain.imageFormat = formats[format_index].format;
				swapchain.imageColorSpace = formats[format_index].colorSpace;
				swapchain.presentMode = present_modes[present_mode];
				swapchain.surface = win->surface_handle();
				if (swapchain.imageExtent.width == invalid) {
					swapchain.imageExtent.width = win->width();
				}
				if (swapchain.imageExtent.height == invalid) {
					swapchain.imageExtent.height = win->height();
				}
				VK_ vkCreateSwapchainKHR(hdv, &swapchain, T::allocator(), &handle_.value)
					| popup("[SWAPCHAIN] Create swapchain failure.");
				::std::vector<VK_ VkImage> images;
				invoke(images, &VK_ vkGetSwapchainImagesKHR, hdv, handle_)
					| popup("[SWAPCHAIN] Cannot get_by swapchain images.");

				images_.reserve(images.size());
				::std::uint16_t frame_index = 0u;
				for (auto& image : images) {
					auto& handle = images_.emplace_back();
					handle.value = image;
				}
				ensure_frame_index();
				fence_index_ = info.fence_index;
				semaphore_index_ = info.semaphore_index;
			}

			~make() { clear(); }

			constexpr auto swapchain_handle() const noexcept { return handle_.value; }

			constexpr auto frame_count() const noexcept { return::std::uint16_t(images_.size()); }
			constexpr auto image_count() const noexcept { return 1u; }
			auto& image(::std::uint16_t, ::std::uint32_t) const noexcept {
				return images_[ensure_frame_index()];
			}
			auto& image(::std::uint16_t frame_index, ::std::uint32_t) noexcept {
				return images_[ensure_frame_index()];
			}

			constexpr auto frame_index() const noexcept {
				return erasure_frame_index();
			}

			void send(event::execute e) {
				T::send(e);
				VK_ VkFence fence = VK_NULL_HANDLE;
				VK_ VkSemaphore semaphore = VK_NULL_HANDLE;
				if constexpr (T::template have_parent<execution>()) {
					auto exec = T::template parent<execution>();
					if (semaphore_index_ != invalid) {
						semaphore = exec->semaphore(semaphore_index_);
					}
					if (fence_index_ != invalid) {
						fence = exec->fence(fence_index_);
					}
				}
				::std::uint64_t timeout = semaphore || fence ? 0u : maximum;
				VK_ vkAcquireNextImageKHR(T::template parent<device>()->device_handle(),
					handle_, timeout, semaphore, fence, &frame_index_)
					| popup("[SWAPCHAIN] swapchain index acquisition failure.");
				if (timeout != maximum) {
					frame_index_ = invalid;
				}
			}

		protected:
			::std::uint16_t fence_index_ = invalid;
			::std::uint16_t semaphore_index_ = invalid;
			::std::uint32_t frame_index_ = invalid;
			move_only<VK_ VkSwapchainKHR> handle_{ VK_NULL_HANDLE };
			::std::vector<swapchain_image_handle> images_;

		private:
			auto erasure_frame_index() {
				auto result = ::std::uint16_t(frame_index_);
				if (result == invalid) {
					auto hdv = T::template parent<device>()->device_handle();
					VK_ VkFence fence = VK_NULL_HANDLE;
					VK_ VkSemaphore semaphore = VK_NULL_HANDLE;
					if constexpr (T::template have_parent<execution>()) {
						auto exec = T::template parent<execution>();
						if (fence_index_ != invalid) {
							exec->send(event::wait_fences{ {fence_index_} });
						}
						else if constexpr (requires { exec->send(::std::declval<event::wait_semaphores>()); }) {
							if (semaphore_index_ != invalid) {
								exec->send(event::wait_semaphores{ {semaphore_index_} });
							}
						}
					}

					for (auto& img : images_) {
						img.frame_index = result;
					}
				}
				return result;
			}

			void clear() {
				VK_ vkDestroySwapchainKHR(
					T::template parent<device>()->device_handle(), handle_, T::allocator());
				images_.clear();
			}
		};

	};
}
#	else
#		error "Other platform's swapchain component is not finished."
#	endif
#endif