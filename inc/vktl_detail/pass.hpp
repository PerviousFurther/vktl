#pragma once

#include "bind_points.hpp"
#include "math.hpp"

// BEGIN PASS.

namespace vktl::detail {
	inline constexpr auto PASS_SCOPE = BIND_POINTS_SCOPE + 0x1u;

	template<::std::size_t index = 0>
	struct access_has_read_ {};
	template<>
	struct access_has_read_<0> {
		template<typename T>
		constexpr bool operator()(T flags) const noexcept {
			constexpr auto mask = VK_ VkAccessFlags(
				VK_ VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
				VK_ VK_ACCESS_INDEX_READ_BIT |
				VK_ VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
				VK_ VK_ACCESS_UNIFORM_READ_BIT |
				VK_ VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
				VK_ VK_ACCESS_SHADER_READ_BIT |
				VK_ VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
				VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
				VK_ VK_ACCESS_TRANSFER_READ_BIT |
				VK_ VK_ACCESS_HOST_READ_BIT |
				VK_ VK_ACCESS_MEMORY_READ_BIT);

			bool value = flags & mask;
			if constexpr (::std::invocable<access_has_read_<1>, T>) {
				value = value || access_has_read_<1>()(flags);
			}
			return value;
		}
	};
	template<::std::size_t index = 0u>
	constexpr access_has_read_<index> access_has_read{};

	template<::std::size_t index = 0>
	struct access_has_write_ {};
	template<>
	struct access_has_write_<0> {
		template<typename T>
		constexpr bool operator()(T flags) const noexcept {
			constexpr auto mask = VK_ VkAccessFlags(
				VK_ VK_ACCESS_SHADER_WRITE_BIT |
				VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
				VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
				VK_ VK_ACCESS_TRANSFER_WRITE_BIT |
				VK_ VK_ACCESS_HOST_WRITE_BIT |
				VK_ VK_ACCESS_MEMORY_WRITE_BIT);

			bool value = flags & mask;
			if constexpr (::std::invocable<access_has_write_<1>, T>) {
				value = value || access_has_write_<1>()(flags);
			}
			return value;
		}
	};
	template<::std::size_t index = 0u>
	constexpr access_has_write_<index> access_has_write{};

	template<::std::size_t index = 0>
	struct to_layout_ {
		template<typename T>
		constexpr auto operator()(T info) const noexcept requires(index == 0u) {
			if constexpr (::std::same_as<view_usage::type, T>) {
				using namespace view_usage;

				auto layout = VK_ VkImageLayout(0u);

				if ((info & color) != 0) {
					layout = VK_ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
				else if ((info & (depth | stencil)) != 0) {
					if ((info & gpu_read) != 0 && (info & gpu_write) == 0) {
						layout = VK_ VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
					}
					else {
						layout = VK_ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					}
				}
				else if ((info & gpu_write) != 0) {
					layout = VK_ VK_IMAGE_LAYOUT_GENERAL;
				}
				else if ((info & gpu_read) != 0) {
					layout = VK_ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}
				else if ((info & input) != 0) {
					layout = VK_ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}
				else if ((info & copy_dst) != 0) {
					layout = VK_ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				}
				else if ((info & copy_src) != 0) {
					layout = VK_ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				}

				if constexpr (::std::invocable<to_layout_<index + 1u>, T>) {
					return layout != VK_ VK_IMAGE_LAYOUT_UNDEFINED ? layout : to_layout_<index + 1u>()(info);
				}
				else {
					return layout;
				}
			}
			else if constexpr (::std::same_as<T, attachment>) {
				switch (info.attribute & 0x1f) {
				case (1 << 0): // color (0x01)
					return VK_ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				case (1 << 1): // depth (0x02)
				case (1 << 2): // stencil (0x04)
				case ((1 << 1) | (1 << 2)): // depth + stencil (0x06)
					return VK_ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				case (1 << 3): // resolve (0x08)
					return VK_ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				case (1 << 4): // input (0x10)
					return VK_ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				default:assert(false);
				}
			}
			else if constexpr (::std::invocable<to_layout_<index + 1u>, T>) {
				return to_layout_<index + 1u>()(info);
			}
		}
	};
	template<::std::size_t index = 0u>
	constexpr to_layout_<index> to_layout{};


	template<::std::size_t index>
	struct revise_layout_ {
		template<typename T>
		constexpr auto operator()(T& layout, T other) const noexcept requires(index == 0u) {
			if constexpr (::std::invocable<revise_layout_<index + 1u>, T&, T>) {
				return revise_layout_<index + 1u>()(layout, other);
			}
			else {
				if (layout == other) {
					return;
				}

				if (layout == VK_ VK_IMAGE_LAYOUT_UNDEFINED) {
					layout = other;
				}
				else if (other == VK_ VK_IMAGE_LAYOUT_UNDEFINED) {
					return;
				}
				else {
					layout = VK_ VK_IMAGE_LAYOUT_GENERAL;
				}
			}
		}
	};
	template<::std::size_t index = 0u>
	constexpr revise_layout_<index> revise_layout{};


	// BEGIN PASS.


	// BEGIN RESOURCE VIEW



	template<typename T>
	struct basic_view : T {
		using view_type = void;

		constexpr basic_view(auto&&...infos)
			: T{ forward_(infos)... } {
		}

		constexpr void connect(infomation_of<bind_points> auto& bind_points) noexcept {
			if (bind_point_index == invalid) {
				view_index = bind_points.receive(T::view());
				bind_point_index = bind_points.bind_point_index;
			}
		}

		constexpr void setup_backward(auto& pipe_stage)
			noexcept requires(requires{ pipe_stage.pipe_stage(); }) {
			if (stages == invalid) {
				auto access = T::access();
				access.dependency = to_dependency<>(T::view().usages);

				stages = access.stages = pipe_stage.pipe_stage();
				pipe_stage.receive(::std::move(access));
			}
		}

		constexpr void set_connectable() noexcept {
			bind_point_index = invalid;
			stages = invalid;
		}

		::std::uint16_t bind_point_index = invalid;
		transformed<VK_ VkPipelineStageFlags, T> stages = invalid; // stages only allow transfer to VkPipelineStageFlags2.
		::std::uint16_t pipe_index = invalid;
	};

	using namespace resource_view_extensions;

	using namespace buffer_view_extensions;

	template<>
	struct meta_of<buffer_view> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x1000);
		static constexpr auto name = fixed_string{ "buffer_view" };
		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct detail : T {
			using view_type = buffer_view;

			constexpr detail(buffer_view const& view, auto const infos)
				: T{ infos }
				, buffer_view{} {
				buffer_view.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
				buffer_view.format = VK_ VK_FORMAT_UNDEFINED;
				buffer_view.range = VK_WHOLE_SIZE;
				buffer_view.usages = view.usage;
			}
			constexpr detail(auto const infos) : detail{ get_by<view_type>(infos), infos } {}

			constexpr auto& view() noexcept { return buffer_view; }

			// defualt access.
			constexpr auto access() noexcept {
				default_buffer_access access{};
				access.offset = buffer_view.offset;
				access.size = buffer_view.size;
				access.index = buffer_view.index;
				access.access = to_image_access(buffer_view.usages);
				return access;
			}

			transformed<unattached<unused<VK_ VkBufferViewCreateInfo>>, T> buffer_view;
		};

		template<typename T>
		using info = basic_view<detail<T>>;

		template<typename T>
		using make = skipped_make<T>;
	};

	using namespace image_view_extensions;
	template<>
	struct meta_of<image_view> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x2000);
		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct detail : T {
			using view_type = texture_view;

			constexpr detail(image_view const& view, auto const infos)
				: T{ infos }
				, image_view{} {
				image_view.sType = VK_ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				image_view.viewType = VK_ VK_IMAGE_VIEW_TYPE_2D;
				image_view.components = {
					.r = VK_ VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_ VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_ VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_ VK_COMPONENT_SWIZZLE_IDENTITY
				};
				image_view.subresourceRange = {
					.aspectMask = to_aspect_mask<>(view.usage),
					.baseMipLevel = 0,
					.levelCount = VK_REMAINING_MIP_LEVELS,
					.baseArrayLayer = 0,
					.layerCount = VK_REMAINING_ARRAY_LAYERS,
				};
				image_view.index = view.index;
				image_view.usages = view.usage;
			}
			constexpr detail(auto const infos) : detail{ get_by<T>(infos), infos } {}

			constexpr auto& view() noexcept { return image_view; }

			// defualt access.
			constexpr auto access() noexcept {
				default_image_access access{};
				static_cast<VK_ VkImageSubresourceRange&>(access) = image_view.subresouceRange;
				access.access = to_image_access(image_view.usages);
				access.index = image_view.index;
				access.layout = to_layout<>(image_view.usages);
				return access;
			}

			transformed<unattached<unused<VK_ VkImageViewCreateInfo>>, T> image_view;
		};

		template<typename T>
		using info = basic_view<detail<T>>;

		template<typename T>
		using make = skipped_make<T>;
	};

	template<>
	struct meta_of<format_color> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x2001);
		static constexpr auto name = fixed_string{ "format_color" };

		using order = order::at_middle;
		using extend = extend_one_of<image_view, buffer_view>;

		template<typename T>
		struct info : T {
			constexpr info(auto&& infos)
				: T{ forward_(infos) } {
				T::view().format = to_format<>(get_by<T>(infos));
			}
		};
	};

	template<>
	struct meta_of<format_depth> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x2002);
		static constexpr auto name = fixed_string{ "format_depth" };

		using order = order::at_middle;
		using extend = extend_one_of<image_view, buffer_view>;

		template<typename T>
		struct info : T {
			constexpr info(auto&& infos)
				: T{ forward_(infos) } {
				T::view().format = to_format<>(get_by<T>(infos));
			}
		};
	};
	template<>
	struct meta_of<binding> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x2005);
		static constexpr auto name = fixed_string{ "binding" };
		using order = order::at_middle;
		using extend = extend_any;

		template<typename T>
		struct transform : ::std::type_identity<T> {};
		template<typename T>
			requires(::std::derived_from<T, VK_ VkBufferViewCreateInfo> || ::std::derived_from<T, VK_ VkImageViewCreateInfo>)
		struct transform : ::std::type_identity<unbinded<T>> {};

		template<typename T>
		struct info : T {
			constexpr info(auto&& info)
				: T{ forward_(info) }
			{
			}

			constexpr void connect(infomation_of<affine_set> auto& affine_set) noexcept {
				if (offset == invalid && affine_set.receive(T::view().set, offset)) {
					T::view().set = offset;
				}
			}

			constexpr void connect(infomation_of<pass> auto& p) {
				if () {

				}
			}

			::std::uint16_t offset = invalid;
		};
	};

	template<typename T>
		requires(requires { typename T::view_type; })
	struct meta_of<is_static>::info<T> : T {
		constexpr info(auto&& info) : T{ forward_(info) } {
			this->view().usages |= resource_attrs::is_static;
		}
	};

	template<typename T, typename Access = default_image_access>
	struct image_access_point : T {
		using image_access_info = Access; // must copy constructible.

		constexpr image_access_point(auto&&...others)
			: T{ forward_(others)... } {
		}

		constexpr void receive(image_access_info info) {
			insert_image_(image_access_policy(),
				image_accesses, image_accesses.begin(), ::std::move(info));
		}

		::std::vector<image_access_info> image_accesses;

	protected:
		template<typename U>
		constexpr void init_from_others(U&& others) {
			image_accesses = forward_like<U>(others.image_accesses);
			if constexpr (requires { T::init_from_others(static_cast<U&&>(others)); }) {
				T::init_from_others(static_cast<U&&>(others));
			}
		}
	};

	template<typename T, typename Access = default_buffer_access>
	struct buffer_access_point : T {
		using access_info = Access; // must copy constructible.

		constexpr buffer_access_point(auto&&...others)
			: T{ forward_(others)... } {
		}

		constexpr void receive(access_info info) {
			insert_buffer_(buffer_access_policy(),
				buffer_accesses, buffer_accesses.begin(), ::std::move(info));
		}

		::std::vector<access_info> buffer_accesses;

	protected:
		template<typename U>
		constexpr void init_from_others(U&& others) {
			buffer_accesses = forward_like<U>(others.buffer_accesses);
			if constexpr (requires { T::init_from_others(static_cast<U&&>(others)); }) {
				T::init_from_others(static_cast<U&&>(others));
			}
		}
	};

	//template<typename T>
	//struct image_barrier_point : image_access_point<T> {
	//    using base = image_access_point<T>;
	//    using image_access_info = typename base::access_info;
	//    using barrier_points_info = unattached<barrier_points>;

	//    constexpr image_barrier_point(auto&&...others)
	//        : base{ forward_(others)... } {
	//    }

	//    ::std::vector<barrier_points_info> image_barrier_points;
	//    ::std::vector<VK_ VkImageMemoryBarrier> image_barriers;

	//protected:
	//    constexpr void insert_image_barrier(::std::span<image_access_info const> fronters) {
	//        for (auto const& fronter : fronters) {
	//            for (auto const& current : this->image_accesses) {
	//                if (current.index == fronter.index) {
	//                    barrier_points_info info{};
	//                    info.front_stages = fronter.stages;
	//                    info.trigger_stages = current.stages;
	//                    info.dependencies = current.dependencies;

	//                    auto intersected = subres.get_intersect(fronter, current);
	//                    VK_ VkImageMemoryBarrier barrier{
	//                        .sType = VK_ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
	//                        .srcAccessMask = fronter.accesss,
	//                        .dstAccessMask = current.access,
	//                        .oldLayout = fronter.layout,
	//                        .newLayout = current.layout,
	//                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	//                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	//                        .offset = intersected.offset,
	//                        .size = intersected.size,
	//                    };
	//                    insert_image_(image_barrier_policy(),
	//                        image_barrier_points, image_barriers,
	//                        image_barrier_points.begin(), image_barriers.begin(),
	//                        ::std::move(info), ::std::move(barrier));
	//                }
	//            }
	//        }
	//    }

	//    constexpr void tune_layout_in_pass(::std::span<image_access_info const> fronters) {
	//        for (auto const& fronter : fronters) {
	//            for (auto& current : this->image_accesses) {
	//                if (fronter.index == current.index) {
	//                    revise_layout<>(current.layout, fronter.layout);
	//                    auto layout = current.layout;
	//                    auto itb = this->image_barriers.begin();
	//                    auto itm = this->image_barrier_points.begin();
	//                    for (; itb != image_barriers.end(); itb++, itm++) {
	//                        if (itm->index == current.index) {
	//                            itb->newLayout = itb->oldLayout = layout;
	//                        }
	//                    }
	//                }
	//            }
	//        }
	//    }

	//    template<typename U>
	//    constexpr void init_from_others(U&& others) {
	//        image_barrier_points = forward_like<U>(others.image_barrier_points);
	//        image_barriers = forward_like<U>(others.image_barriers);
	//        base::init_from_others(static_cast<U&&>(others));
	//    }
	//};

	//template<typename T>
	//struct buffer_barrier_point : buffer_access_point<T> {
	//    using base = buffer_access_point<T>;
	//    using buffer_access_info = typename base::buffer_access_info;
	//    using barrier_points_info = unattached<barrier_points>;

	//    constexpr buffer_barrier_point(auto&&...others)
	//        : base{ forward_(others)... } {
	//    }

	//    void receive() {

	//    }

	//    ::std::vector<barrier_points_info> buffer_barrier_points;
	//    ::std::vector<VK_ VkBufferMemoryBarrier> buffer_barriers;

	//protected:
	//    constexpr void insert_buffer_barrier(::std::span<buffer_access_info const> fronters) {
	//        for (auto const& fronter : fronters) {
	//            for (const auto& current : this->buffer_accesses) {
	//                if (fronter.index == current.index
	//                    && subres.intersected(fronter.offset, fronter.size, current.offset, current.size)) {
	//                    barrier_points_info info{};
	//                    info.front_stages = fronter.stages;
	//                    info.trigger_stages = current.stages;
	//                    info.dependencies = current.dependencies;

	//                    auto intersected = subres.get_intersect(fronter.offset, fronter.size, current.offset, current.size);
	//                    VK_ VkBufferMemoryBarrier barrier{
	//                        .sType = VK_ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
	//                        .srcAccessMask = fronter.accesss,
	//                        .dstAccessMask = current.access,
	//                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	//                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	//                        .offset = intersected.offset,
	//                        .size = intersected.size,
	//                    };
	//                    insert_buffer_(buffer_barrier_policy(),
	//                        buffer_barrier_points, buffer_barriers,
	//                        buffer_barrier_points.begin(), buffer_barriers.begin(),
	//                        ::std::move(info), ::std::move(barrier));
	//                }
	//            }
	//        }
	//    }

	//    template<typename U>
	//    constexpr void init_from_others(U&& others) {
	//        buffer_barrier_points = forward_like<U>(others.buffer_barrier_points);
	//        buffer_barriers = forward_like<U>(others.buffer_barriers);
	//        base::init_from_others(static_cast<U&&>(others));
	//    }
	//};

	//template<typename T>
	//struct memory_barrier_point : T {
	//    ::std::vector<barrier_points> memory_barrier_points;
	//    ::std::vector<VK_ VkMemoryBarrier> memory_barriers;

	//    constexpr memory_barrier_point(auto&&... args) : T{ forward_(args)... } {}

	//    template<typename U>
	//    constexpr void setup_forward(memory_barrier_point<U>& other) {
	//        auto itop = other.memory_barrier_points.begin();
	//        auto itob = other.memory_barriers.begin();
	//        for (; itob != other.memory_barriers.end(); itob++, itop++) {
	//            auto itcp = memory_barrier_points.begin();
	//            auto itcb = memory_barriers.begin();
	//            for (; itcb != memory_barriers.end();) {
	//                if (itop->front_stages == itcp->front_stages
	//                    && itop->trigger_stages == itcp->trigger_stages
	//                    && itop->dependencies == itcp->dependencies) {
	//                    itob->dstAccessMask |= itcb->dstAccessMask;
	//                    itcb->srcAccessMask |= itcb->srcAccessMask;
	//                    itcp = memory_barrier_points.erase(itcp);
	//                    itcb = memory_barriers.erase(itcb);
	//                }
	//                else {
	//                    itcp++;
	//                    itcb++;
	//                }
	//            }
	//        }
	//    }

	//protected:
	//    template<typename U>
	//    constexpr void init_from_other(U&& other) {
	//        memory_barriers = forward_like<U>(other.memory_barriers);
	//        memory_barrier_points = forward_like<U>(other.memory_barrier_points);
	//        if constexpr (requires { T::init_from_other(forward_(other)); }) {
	//            T::init_from_other(forward_(other));
	//        }
	//    }
	//};

	struct insert_push_constants {
		VK_ VkPushConstantRange temp;

		static constexpr bool compare_less(auto const& lhs, auto const& rhs) noexcept {
			return lhs.offset < rhs.offset;
		}
		static constexpr bool has_intersection(auto const& lhs, auto const& rhs) noexcept {
			return subres.intersected(lhs.offset, lhs.size, rhs.offset, rhs.size);
		}
		static constexpr bool can_merge(auto const& lhs, auto const& rhs) noexcept {
			return lhs.stageFlags == rhs.stageFlags;
		}
		static constexpr void merge_attributes(auto& slice, auto const& buf) noexcept {
			slice.stageFlags |= buf.stageFlags;
		}
	};
	using push_constants_policy = buffer_policy<insert_push_constants>;

	template<typename T>
	struct access_map {
		using range_type = ::std::conditional_t<(::std::derived_from<T, VK_ VkImageSubresourceRange>),
			VK_ VkImageSubresourceRange, range<>>;

		access_map() = default;

		void insert(T access) {
			auto it = accesses_.find(access.index);
			if (it != accesses_.end()) {
				auto& vec = it->second;

				auto itb = vec.end() - 1u;
				while (itb != vec.begin() - 1u && itb->stages != access.stages) { itb--; }
				if (itb == vec.begin() - 1u) {
					vec.emplace_back(::std::move(access));
					return;
				}

				auto ite = itb - 1u;
				while (ite != vec.begin() - 1u && ite->stages == access.stages) { ite--; }

				list<T> stack;
				stack.emplace_front(::std::move(access));
				for (; itb != ite; ) {
					auto c = ::std::move(stack.front());
					stack.erase(stack.begin());
					if (c.stages == itb->stages && itb->dependency == c.dependency) {
						if (itb->access == c.access && subres.adjacent_intersect(*itb, c)) {
							static_cast<range_type&>(c) = subres.merge(c, *itb);
							auto layer_diff = ::std::abs(int(itb->baseArrayLayer) - int(c.baseArrayLayer));
							auto mip_diff = ::std::abs(int(itb->baseMipLevel) - int(c.baseMipLevel));
							c.baseArrayLayer = (::std::min)(itb->baseArrayLayer, c.baseArrayLayer);
							c.baseMipLevel = (::std::min)(itb->baseMipLevel, c.baseMipLevel);
							c.layerCount = (::std::max)(itb->layerCount, c.layerCount) + mip_diff;
							c.levelCount = (::std::max)(itb->levelCount, c.levelCount) + layer_diff;
							itb = vec.erase(itb);
						}
						else if (subres.intersected(*itb, c)) {
							auto intersected = subres.get_intersect(*itb, c);

							auto copy = *itb;
							itb = vec.erase(itb);

							auto a = copy;
							static_cast<range_type&>(a) = intersected;
							a.access |= c.access;
							itb = vec.insert(itb, a);

							bool can_break = !stack.size();
							for (auto non_intersected : subres.get_not_intersected(*itb, c)) {
								auto slice = non_intersected.is_left ? copy : c;
								static_cast<range_type&>(slice) = static_cast<range_type&&>(non_intersected);
								if (!non_intersected.is_left && !subres.empty(slice)) {
									accesses_.emplace(slice);
									can_break = false;
								}
								else {
									itb = vec.insert(itb, slice);
								}
							}
							if (can_break) {
								break;
							}
						}
						itb--;
					}
				}
				for (auto&& v : ::std::move(stack)) {
					itb = vec.insert(itb, ::std::move(v));
				}
			}
			else {
				accesses_.emplace(access.index).first
					->second.emplace_back(::std::move(access));
			}
		}

	private:
		// access per handle.
		::std::unordered_map<::std::uint16_t, ::std::vector<T>> accesses_;
	};

	template<typename T>
	struct basic_barrier_point_info : T {
		auto receive(default_image_access access) {
			image_accesses.insert(access);
		}

		auto receive(defualt_buffer_access access) {
			buffer_accesses.insert(access);
		}

		access_map<default_image_access> image_accesses;
		access_map<default_buffer_access> buffer_accesses;
	};

	template<typename T>
	struct basic_barrier_point_make : T {
		template<typename F>
		basic_barrier_point_make(F&& first, auto&&...rest)
			: T{ forward_(rest)... }
			, image_accesses_{ forward_like<F&&>(first.image_accesses) }
			, buffer_accesses_{ forward_like<F&&>(first.buffer_accesses) }
		{}

		void receive(event::fill_barrier, auto& invoker) {
			invoker.append(buffer_access_, image_accesses_);
		}

	protected:
		access_map<default_image_access> image_accesses_;
		access_map<default_buffer_access> buffer_accesses_;
	};

	template<typename T>
	struct basic_pass_info : T {
		using base = T;

		constexpr basic_pass_info(auto&&...infos) : base{ forward_(infos)... } {}

		constexpr auto setup_backward(infomation_of<pass> auto& other) {
			if (pass <= other.pass) {
				pass = other.pass + 1u;
				return true;
			}
			else {
				return false;
			}
		}

		// barrier setup defer to construct time.
		// thus here have no barriers.

		constexpr void set_connectable() noexcept {
			pass = 0u;
			if constexpr (requires { base::set_connectable(); }) {
				base::set_connectable();
			}
		}

		::std::uint16_t pass = 0u;
	};


	using namespace pass_extensions;

	template<>
	struct meta_of<affine_set> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x5001);
		static constexpr auto name = fixed_string{ "affine_set" };

		using order = order::at_middle;
		using extend = pass;

		template<typename T>
		struct info : T {
			constexpr info(auto&& infos)
				: T{ forward_(infos) }
				, affine_set{ get_by<T>(forward_(infos)) }
			{
			}

			constexpr bool receive(::std::uint16_t set, ::std::uint16_t& actual) {
				if (set == affine_set.set) {
					actual = affine_set.index;
					return true;
				}
				else if constexpr (requires { T::receive(set, actual); }) {
					return T::receive(set, actual);
				}
				else {
					return false;
				}
			}

			affine_set affine_set;
		};
	};

	struct default_pipe_layout_info : VK_ VkPipelineLayoutCreateInfo {
		::std::vector<::std::uint16_t> set_indices;
		::std::vector<VK_ VkPushConstantRange> push_ranges;
	};
	template<typename T>
	struct basic_pipe_pass_info : basic_pass_info<T> {
		using base = basic_pass_info<T>;
		using pipe_layout_info
			= transformed<default_pipe_layout_info, T>;

		static constexpr auto use_default
			= ::std::derived_from<pipe_layout_info, default_pipe_layout_info>;

		constexpr basic_pipe_pass_info(auto&&...infos)
			: base{ forward_(infos)... } {
		}

		constexpr auto receive(VK_ VkPushConstantRange const& range) requires(use_default) {
			auto& push_ranges = pipe_layout.push_ranges;
			auto itp = push_ranges.begin();
			auto copy{ range };
			insert_buffer_(push_constants_policy(), push_ranges, itp->begin(), copy);
		}

		constexpr auto append_binding_set(::std::uint16_t set) requires(use_default) {
			insert_set_indices(pipe_layout.set_indices, set);
		}

		pipe_layout_info pipe_layout;

	private:
		void insert_set_indices(auto& vec, auto set) {
			auto itii = vec.begin();
			while (itii != vec.end() && *itii < set) { itii++; }
			if (itii == vec.end() || *itii != set) {
				vec.insert(itii, set);
			}
		}
	};


	template<typename T>
	struct basic_pass_make : T {
		template<typename>
		friend struct basic_pass_make;

		static constexpr auto is_child_pass = have_parent_v<T, pass>;
		static constexpr auto pass_index = []() constexpr {
			if constexpr (is_child_pass) {
				return object_parent_t<T, pass>::pass_index + 1u;
			}
			else {
				return 0u;
			}
		}();
		static constexpr auto local_pass_index = []() constexpr {
			if constexpr (requires{ T::local_pass_index; }) {
				return T::local_pass_index + 1u;
			}
			else {
				return 0u;
			}
		}();

		using image_access_point = image_access_point<T>;
		using buffer_access_point = buffer_access_point<image_access_point>;
		using base = buffer_access_point;

		template<typename F>
		constexpr basic_pass_make(F&& info, auto&&...others)
			: base{ forward_(others)... } {
			base::init_from_other(info);
			buffer_accesses = forward_like<F>(info.buffer_accesses);
			image_accesses = forward_like<F>(info.image_accesses);
			if constexpr (is_child_pass) {
				this->connect(*T::template parent<pass>());
			}
		}

		constexpr auto connect(object_of<pass> auto& fronter) {
			if constexpr (is_child_pass) {
				dispatch = T::template parent<pass>()->connect(fronter);
			}
			assert(false); // pass barrier is not finished.
			for (auto const& fronter : fronter.buffer_accesses) {
				for (const auto& current : buffer_accesses) {
					if (fronter.index == current.index
						&& subres.intersected(fronter.offset, fronter.size, current.offset, current.size)) {
						auto intersected = subres.get_intersect(fronter.offset, fronter.size, current.offset, current.size);
						barrier_points<VK_ VkBufferMemoryBarrier> info{};
						info.sType = VK_ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
						info.srcAccessMask = fronter.accesss;
						info.dstAccessMask = current.access;
						info.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						info.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						info.offset = intersected.offset;
						info.size = intersected.size;
						info.front_stages = fronter.stages;
						info.trigger_stages = current.stages;
						info.dependencies = current.dependencies;
						insert_buffer_(buffer_barrier_policy(), barriers_, barriers_.begin(), ::std::move(info));
					}
				}
			}
			for (auto const& fronter : fronter.image_accesses) {
				for (const auto& current : image_accesses) {
					if (fronter.index == current.index
						&& subres.intersected(current, fronter)) {
						auto intersected = subres.get_intersect(current, fronter);
						barrier_points<VK_ VkImageMemoryBarrier> info{};
						info.sType = VK_ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						info.srcAccessMask = fronter.accesss;
						info.dstAccessMask = current.access;
						info.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						info.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						info.subresourceRange = intersected;
						info.front_stages = fronter.stages;
						info.trigger_stages = current.stages;
						info.dependencies = current.dependencies;
						insert_image_(image_barrier_policy(), barriers_, barriers_.begin(), ::std::move(info));
					}
				}
			}
		}

	protected:
		list<barrier_dispatch> barriers_;

		using image_access_point::image_accesses;
		using image_access_point::append;
		using buffer_access_point::buffer_accesses;
		using buffer_access_point::append;
	};

	template<typename T>
	struct basic_pipe_pass_make : basic_pass_make<T> {
		using base = basic_pass_make<T>;
		using image_access_point = typename base::image_access_point;
		using buffer_access_point = typename base::buffer_access_point;

		template<typename F>
		constexpr basic_pipe_pass_make(F&& info, auto&&...infos)
			: base{ forward_(infos)... }
			, pipe_to_layout_indices_{ forward_like<F>(info.pipe_to_layout_indices) } {
			auto hdv = T::template parent<device>()->device_handle();
			auto bp = T::template parent<bind_points>();

			image_access_point::init_from_others(info);
			buffer_access_point::init_from_others(info);

			pipe_layouts_.resize(info.pipe_layouts.size());
			auto itdp = pipe_layouts_.begin();
			auto itp = info.pipe_layouts.begin();
			auto itc = info.pipe_constants.begin();
			auto iti = info.pipe_set_indices.begin();
			for (; itp != info.pipe_layouts.end(); itp++, itc++, iti++) {
				::std::vector<VK_ VkDescriptorSetLayout> layouts;
				layouts.reserve(iti->size());
				for (auto idx : *iti) {
					layouts.push_back(bp->set_layout(idx));
				}

				VK_ VkPipelineLayoutCreateInfo info = *itp;
				info.pushConstantRangeCount = ::std::uint32_t(itc->size());
				info.pPushConstantRanges = itc->data();
				info.setLayoutCount = ::std::uint32_t(layouts.size());
				info.pSetLayouts = layouts.data();

				VK_ vkCreatePipelineLayout(hdv, &info, T::allocator(), &*itdp++);
			}
		}

		template<event_of<begin_data> E>
		constexpr void receive(E& e, auto& invoker) {
			for (auto const& dispatch : barrier_point_) {
				VK_ vkCmdPipelineBarrier(e.current(),
					dispatch.front_stages, dispatch.trigger_stages, dispatch.dependencies,
					::std::uint32_t(dispatch.memories.size()), dispatch.memories.data(),
					::std::uint32_t(dispatch.buffers.size()), dispatch.buffers.data(),
					::std::uint32_t(dispatch.images.size()), dispatch.images.data());
			}
		}

		constexpr auto pipe_layout(::std::size_t index) const noexcept {
			return pipe_layouts_[index];
		}

	protected:
		// ::std::vector<barrier_dispatch> static_barriers;
		::std::vector<move_only<VK_ VkPipelineLayout>> pipe_layouts_;
		::std::vector<::std::uint16_t> pipe_to_layout_indices_;
		::std::vector<::std::uint16_t> unresolved_barriers_;

	private:
		constexpr void append_barrier(auto fn, auto itb, auto itbe, auto itm) {
			for (; itb != itbe; itb++, itm++) {
				auto it = barrier_point_.begin();
				while (it != barrier_point_.end()) {
					if (it->trigger_stages == itm->trigger_stages
						&& (it->front_stages == itm->trigger_stages)
						&& (it->dependencies == itm->dependencies)) {
						fn(it).emplace_back(*itb);
						break;
					}
					it++;
				}
				if (it == barrier_point_.end()) {
					it = barrier_point_.insert(it, barrier_dispatch{});
					it->front_stages = itm->front_stages;
					it->trigger_stages = itm->trigger_stages;
					it->dependencies = itm->dependencies;
					fn(it).emplace_back(*itb);
				}
			}
		}

	private:
		::std::vector<barrier_dispatch> barrier_point_;
		using image_access_point::image_accesses;
		using buffer_access_point::buffer_accesses;
	};
	// pass event.

	using namespace pass_extensions;
	using namespace pipe_extensions;

	// template<typename T, ::std::uint16_t index>
	// struct inside_pass : T {
	//     static constexpr auto pass_index = index;
	// };
	// template<typename T>
	// struct pass_data : T {
	//     constexpr void next_subpass() {
	//         subpass++;
	//     }
	// 
	//     ::std::uint16_t subpass;
	// };

	// template<typename T, template<typename>typename EventType, typename Next>
	// concept event_in_pass = event_of<T, EventType>&& event_in_task<T, EventType, Next>&& Next::pass_index == T::pass_index;

	template<>
	struct meta_of<pass> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x5000);
		static constexpr auto name = fixed_string{ "pass" };
		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct info {
			static_assert(always_false<T>,
				"pass instance must contain one of "
				"`render_pass`, `compute`, `transfer` or `graphics` can mark pass's type.");
		};

		static constexpr auto as_attachment(attachment att) noexcept {
			// const bool is_ds = att.depth || att.stencil;
			// const bool is_color = att.color || att.resolve;
			return VK_ VkAttachmentDescription{
				.flags = 0,
				.loadOp = att.clear ? VK_ VK_ATTACHMENT_LOAD_OP_CLEAR :
						  att.load ? VK_ VK_ATTACHMENT_LOAD_OP_LOAD : VK_ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp = VK_ VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = att.clear_stencil ? VK_ VK_ATTACHMENT_LOAD_OP_CLEAR :
								 att.load ? VK_ VK_ATTACHMENT_LOAD_OP_LOAD : VK_ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = att.stencil ? VK_ VK_ATTACHMENT_STORE_OP_STORE : VK_ VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_ VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_ VK_IMAGE_LAYOUT_UNDEFINED,
			};
		}

		static constexpr auto to_stage(VK_ VkAccessFlags accessFlags) noexcept {
			VK_ VkPipelineStageFlags stages = 0;
			if (accessFlags & VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) {
				stages |= VK_ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			}
			if (accessFlags & VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) {
				stages |= VK_ VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_ VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			}
			if (accessFlags & VK_ VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) {
				stages |= VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			return stages;
		};

		static constexpr auto to_access(attachment att) noexcept {
			switch (att.attribute & 0x1f) {
			case (1 << 0): // color (0x01)
			case (1 << 3): // resolve (0x08)
				return VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			case (1 << 1): // depth (0x02)
			case (1 << 2): // stencil (0x04)
			case ((1 << 1) | (1 << 2)): // depth + stencil (0x06)
				return VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			case (1 << 4): // input (0x10)
				return VK_ VK_ACCESS_SHADER_READ_BIT;

			default:assert(false);
			}
		}

		using subpass_attachment_access = unhandled<unattached<>>;

		template<contains<render_pass> T>
		struct info<T> : basic_pipe_pass_info<T> {
			using base = basic_pipe_pass_info<T>;

			constexpr info(pass const& info, auto&& infos) : base{ forward_(infos) } {
				framebuffer.sType = VK_ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				renderpass.sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			}

			constexpr info(auto&& infos) : info{ get_by<T>(infos), forward_(infos) } {
			}


			constexpr void relocate() noexcept {
				auto it_subpass = subpasses.begin();
				auto it_color = colors.begin();
				auto it_depth = depths.begin();
				auto it_resolve = resolves.begin();
				auto it_input = inputs.begin();
				for (; it_subpass != subpasses.end();
					++it_subpass, ++it_color, ++it_depth, ++it_resolve, ++it_input) {
					auto& subpass = *it_subpass;

					[[maybe_unused]] auto size
						= subpass.colorAttachmentCount
						= ::std::uint32_t((::std::max)({ it_color->size(), it_depth->size(), it_resolve->size() }));
					subpass.pColorAttachments = it_color->data();
					subpass.pDepthStencilAttachment = it_depth->data();
					subpass.pResolveAttachments = it_resolve->data();

					assert(size != 0u ||
						((it_color->size() == 0u || it_color->size() == size)
							&& (it_depth->size() == 0u || it_depth->size() == size)
							&& (it_resolve->size() == 0u || it_resolve->size() == size)));

					subpass.inputAttachmentCount = ::std::uint32_t(it_input->size());
					subpass.pInputAttachments = it_input->data();
				}

				renderpass = {
					.sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
					.attachmentCount = ::std::uint32_t(attachments.size()),
					.pAttachments = attachments.data(),
					.subpassCount = ::std::uint32_t(subpasses.size()),
					.pSubpasses = subpasses.data(),
					.dependencyCount = ::std::uint32_t(dependencies.size()),
					.pDependencies = dependencies.data(),
				};

				base::relocate();
			}

			constexpr auto receive(::std::uint16_t pipe,
				VK_ VkPipelineShaderStageCreateInfo const& shader_stage,
				VK_ VkShaderModuleCreateInfo const& shader) {
				shader_stages[pipe].emplace_back(shader_stage);
				shaders[pipe].emplace_back(shader);
			}

			// attachment dependency usually is frame-local, if some operation like blur will use frame-global.
			constexpr auto receive(::std::uint16_t subpass, attachment attachment) {
				if (attachment.index >= attachments.size()) {
					auto new_size = attachment.index + 1u;
					attachments.resize(new_size);
					attachment_image_indices.resize(new_size);
					attachment_view_indices.resize(new_size);
					image_which_subpass_access.resize(new_size);
				}
				attachments[attachment.index] = as_attachment(attachment); // overwrite it.
				attachment_view_indices[attachment.index] = attachment.view_index;

				// assume already resize. thus no resize.

				switch (attachment.attribute & 0x1f) {
				case (1 << 0): // color (0x01)
					append_subpass(colors[subpass], subpass_accesses[subpass], attachment);
					break;

				case (1 << 1): // depth (0x02)
				case (1 << 2): // stencil (0x04)
				case ((1 << 1) | (1 << 2)): // depth + stencil (0x06)
					append_subpass(depths[subpass], subpass_accesses[subpass], attachment);
					break;

				case (1 << 3): // resolve (0x08)
					append_subpass(resolves[subpass], subpass_accesses[subpass], attachment);
					break;

				case (1 << 4): // input (0x10)
					append_subpass(inputs[subpass], subpass_accesses[subpass], attachment);
					break;

				default:assert(false); // unknown type.
				}

				image_which_subpass_access[attachment.index].emplace_back(subpass);
			}

			constexpr auto receive(VK_ VkGraphicsPipelineCreateInfo const& info,
				VK_ VkSubpassDescription const& subpass_desc) {
				const auto subpass = info.subpass;
				if (subpass >= subpasses.size()) {
					const auto new_size = info.subpass + 1u;
					subpasses.resize(new_size);
					colors.resize(new_size);
					inputs.resize(new_size);
					depths.resize(new_size);
					resolves.resize(new_size);
					subpass_accesses.resize(new_size);
				}

				pipelines.push_back(info);
				shaders.emplace_back();
				subpasses.push_back(::std::move(subpass_desc));
			}

			constexpr void connect_backward(infomation_of<pass, render_pass> auto& fronter) {
				auto fronter_attachment_index = 0u;
				for (auto front_image : fronter.attachment_image_indices) {
					auto current_attachment_index = 0u;
					for (auto current_image : attachment_image_indices) {
						if (front_image == current_image) {
							auto fronter_stage = VK_ VkAccessFlags(0u);
							auto fronter_access = VK_ VkAccessFlags(0u);
							for (auto fronter_subpass : fronter.image_which_subpass_access[fronter]) {

							}
							auto current_access = VK_ VkAccessFlags(0u);
							for (auto current_subpass : image_which_subpass_access[current]) {

							}
						}
						current_attachment_index++;
					}

					fronter_attachment_index++;
				}
			}

			constexpr void setup_forward(infomation_of<bind_points> auto& bp) {
				if (begin_external_dependencies != invalid) {
					assert(false); // TODO: maybe multiple call.
				}

				::std::uint32_t
					min_width = maximum,
					min_height = maximum;
				::std::uint16_t
					min_layers = maximum;
				auto attidx = 0u;
				auto ita = attachments.begin();
				auto iti = attachment_image_indices.begin();
				auto itv = attachment_view_indices.begin();
				for (; ita != attachments.end(); ita++, itv++) {
					auto& view = bp.image_views[*itv];
					*iti = view.index;
					auto index = ::std::distance(attachments.begin(), ita);
					auto itait = ita + 1u;
					auto itvit = itv + 1u;
					for (; itait != attachments.end(); itait++, itvit++) {
						if (*itv == *itvit || view.index == bp.image_views[*itvit].index) {
							ita->flags |= VK_ VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
							itait->flags |= VK_ VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
						}
					}
				}
				framebuffer.width = min_width;
				framebuffer.height = min_height;
				framebuffer.layers = min_layers;

				auto its = subpass_accesses.begin();
				for (; its != subpass_accesses.end(); its++) {
					auto front_subpass = ::std::uint16_t(::std::distance(subpass_accesses.begin(), its));
					auto itsit = its + 1u;
					for (; itsit != subpass_accesses.end(); itsit++) {
						auto current_subpass = ::std::uint16_t(::std::distance(subpass_accesses.begin(), itsit);
						for (auto fronter : *its) {
							for (auto current : *itsit) {
								bool front_have_write = false;
								bool current_have_write = false;
								if (fronter.index == current.index
									&& (front_have_write = access_has_write<>(fronter.access)
										|| current_have_write = access_has_write<>(current.access))) {
									erase_internal(front_have_write ? no_internal_write : no_internal_read, front_subpass);
									erase_internal(current_have_write ? no_internal_write : no_internal_read, current_subpass);
									append_dependencies(subpass_accesses.end(),
										front_subpass, current_subpass),
										fronter, current);
								}
							}
						}
					}
				}

				if (bp.bind_point_index == 0u && begin_external_dependencies == invalid) {
					begin_external_dependencies = ::std::uint16_t(dependencies.size());
					append_external_dependencies<false>(
						dependencies.end(),
						no_internal_read,
						subpass_accesses,
						VK_ VkAccessFlags(VK_ VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
							| VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
							| VK_ VK_ACCESS_SHADER_WRITE_BIT));
					append_external_dependencies<true>(
						dependencies.end(),
						no_internal_write,
						subpass_accesses,
						VK_ VkAccessFlags(VK_ VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
							| VK_ VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
							| VK_ VK_ACCESS_SHADER_READ_BIT
							| VK_ VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT));
				}
			}

			constexpr void set_connectable() {
				for (auto& no : no_internal_write) { no = true; }
				for (auto& no : no_internal_read) { no = true; }
				dependencies.erase(dependencies.begin() + begin_external_dependencies, dependencies.end());
				begin_external_dependencies = invalid;
				base::set_connectable();
			}

			::std::vector<VK_ VkGraphicsPipelineCreateInfo> pipelines;
			::std::vector<::std::vector<VK_ VkPipelineShaderStageCreateInfo>> shader_stages;
			::std::vector<::std::vector<VK_ VkShaderModuleCreateInfo>> shaders;

			::std::vector<VK_ VkAttachmentDescription> attachments;

			::std::vector<VK_ VkSubpassDescription> subpasses;
			::std::vector<::std::vector<VK_ VkAttachmentReference>> colors, depths, resolves, inputs;
			::std::vector<::std::vector<subpass_attachment_access>> subpass_accesses;

			::std::uint16_t begin_external_dependencies = invalid;
			::std::vector<VK_ VkSubpassDependency> dependencies;
			VK_ VkRenderPassCreateInfo renderpass;

			// subpass no internal dependency.
			::std::vector<bool> no_internal_write;
			::std::vector<bool> no_internal_read;
			::std::vector<::std::uint16_t> attachment_view_indices;
			::std::vector<::std::uint16_t> attachment_image_indices;
			::std::vector<::std::vector<::std::uint16_t>> image_which_subpass_access;
			VK_ VkFramebufferCreateInfo framebuffer;

		private:
			template<bool to_external>
			constexpr void append_external_dependencies(auto out, auto& no_internal_flags, auto& subpass_accesses, VkAccessFlags mask) {
				auto index = 0u;
				for (bool no_internal : no_internal_flags) {
					if (no_internal) {
						subpass_attachment_access ss{};
						for (const auto& access : accesses) {
							ss.access |= access.access & mask;
							ss.dependency |= access.dependency;
						}
						if constexpr (to_external) {
							append_dependencies(out, index, VK_SUBPASS_EXTERNAL, ss, {});
						}
						else {
							append_dependencies(out, VK_SUBPASS_EXTERNAL, index, {}, ss);
						}
					}
					++index;
				}
			}

			constexpr void append_external_dependencies() {

			}

			constexpr void append_dependencies(auto it, auto front_subpass, auto current_subpass, auto front, auto current) {
				while (it != dependencies.end()
					&& (it->srcSubpass < front_subpass
						|| (it->srcSubpass == front_subpass && it->dstSubpass < current_subpass))) {
					it++;
				}
				if (it == dependencies.end() || it->srcSubpass != front_subpass || it->dstSubpass != current_subpass) {
					// detail::to_shader_stage;
					dependencies.insert(VK_ VkSubpassDependency{
						.srcSubpass = front_subpass,
						.dstSubpass = current_subpass,
						.srcStageMask = to_stage(front.access),
						.dstStageMask = to_stage(current.access),
						.srcAccessMask = front.access,
						.dstAccessMask = current.access,
						.dependency = current.dependency,
						});
				}
				else {
					it->srcStageMask |= to_stage(front.access);
					it->dstStageMask |= to_stage(current.access);
					it->srcAccessMask |= front.access;
					it->dstAccessMask |= current.access;
				}
			}

			constexpr void erase_internal(auto& values, auto subpass) {
				values[subpass] = false;
			}
			constexpr void append_subpass(auto& ref, auto& accesses,
				auto attachment, auto dependency) {
				auto itr = ref.begin();
				while (itr != ref.end() && itr->attachment < attachment.index) { itr++; }
				if (itr == ref.end() || itr->attachment != attachment.index) {
					ref.emplace_back(VK_ VkAttachmentReference{
						.attachment = attachment.index,
						.layout = to_layout(attachment),
						});
				}
				else {
					revise_layout<>(itr->layout, to_layout(attachment));
				}

				auto ita = accesses.begin();
				while (ita != accesses.end() && ita->index < attachment.index) { ita++; }
				if (ita == accesses.end() || ita->index != attachment.index) {
					ita = accesses.insert(ita, {});
					auto index = ::std::uint16_t(::std::distance(accesses.begin(), ita));
					if (access_has_read<>(ita->access)) {
						this->no_internal_write.emplace_back(index);
					}
					if (access_has_write<>(ita->access)) {
						this->no_internal_read.emplace_back(index);
					}
				}
				ita->access |= to_access(attachment);
				ita->dependency |= dependency;
			}
		};

		template<contains<compute> T>
		struct info<T> : basic_pipe_pass_info<T> {

			::std::vector<VK_ VkComputePipelineCreateInfo> pipelines;
		};

		template<contains<transfer> T>
		struct info<T> : basic_pass_info<T> {

		};

		template<typename T>
		struct make;

		template<local_contains<render_pass> T>
		struct make<T> : basic_pipe_pass_make<T> {
			using base = basic_pipe_pass_make<T>;

			template<infomation_of<pass, render_pass> F>
			make(F&& info, auto&&...other)
				: base{ static_cast<F&&>(info), forward_(other)... }
				, framebuffer_create_infos_{ forward_like<F&&>(info.framebuffer) }
				, attachment_view_indices_{ forward_like<F&&>(info.attachment_view_indices) } {
				auto hdv = T::template parent<device>()->device_handle();
				auto bp = T::template parent<bind_points>();

				::std::unordered_map<::std::uint32_t const*, VK_ VkShaderModule> shaders;
				::std::vector<VK_ VkGraphicsPipelineCreateInfo> pipes(forward_like<F&&>(info.pipelines));
				::std::vector<::std::vector<VK_ VkPipelineShaderStageCreateInfo>> stages;
				try {
					auto its = info.shaders.begin();
					auto itp = pipes.begin();
					for (; its != info.shaders.end(); its++, itp++) {
						// auto size = its->size();
						auto itss = its->begin();

						auto itpp = itp->pStages;
						auto& stages = stages.emplace_back(itp->pStages, itp->pStages + itp->stageCount);
						auto itps = stages.begin();
						for (; itss != its->end(); itss++, itpp++, itps++) {
							if (itss->pCode) {
								auto it = shaders.find(itss->pCode);
								VK_ VkShaderModule shader;
								if (it == shaders.end()) {
									VK_ vkCreateShaderModule(hdv, &*itss, T::allocator(), &shader)
										| popup{ "[RENDERPASS] Create shader modules failure." };
									shaders.emplace(itss->pCode, shader);
								}
								else {
									shader = it->second;
								}
								itps->module = shader;
							}
							else {
								itps->module = itpp->module;
							}
						}
					}
				}
				catch (...) {
					clean_shaders(hdv, shaders);
					throw;
				}

				try {
					VK_ vkCreateRenderPass(hdv, &info.renderpass, T::allocator(), &render_pass_)
						| popup("[RENDERPASS] Create renderpass failure.");

					// each pipe to layout index.
					auto itmap = this->pipe_to_layout_indices_.begin();
					for (auto& pipe : pipes) {
						pipe.renderPass = render_pass_;
						pipe.layout = this->pipe_layouts_[*itmap++];
					}

					const auto num = ::std::uint32_t(pipes.size());
					::std::vector<VK_ VkPipeline> pipelines(num);
					VK_ vkCreateGraphicsPipelines(T::template parent<device>()->device_handle(),
						nullptr, num, pipes.data(), T::allocator(), pipelines.data())
						| popup("[RENDERPASS] Create pipelines failure.");
					pipelines_.insert(pipelines_.end(), pipelines.begin(), pipelines.end());

					clean_shaders(hdv, shaders);

					auto fc = T::frame_count();
					framebuffers_.resize(fc);

					auto frame_buffer_info = forward_like<F>(info.framebuffer);
					frame_buffer_info.renderPass = render_pass_;

					auto dst = framebuffers_.begin();
					for (auto i = 0u; i < fc; i++) {
						::std::vector<VK_ VkImageView> views;
						views.reserve(info.view_indices.size());
						for (auto idx : info.view_indices) {
							views.emplace_back(bp->unbinded_image_view(i, idx));
						}
						frame_buffer_info.pAttachments = views.data();
						VK_ vkCreateFramebuffer(hdv, &info.framebuffer, T::allocator(), &*dst++)
							| popup("[RENDERPASS] Create framebuffer failure.");
					}
				}
				catch (...) {
					clear();
					throw;
				}
			}

			~make() { clear(); }

			template<event_in_task<begin_data, make> E>
			void receive(E& e, auto& invoker,
				void* next = nullptr, VK_ VkSubpassContents content = VK_ VK_SUBPASS_CONTENTS_INLINE) {
				T::receive(e, invoker);
				if constexpr (!base::pass_index) {
					auto span = T::template parent<bind_points>()->sets();
					for (auto layout : this->pipe_layouts_) {
						VK_ vkCmdBindDescriptorSets(e.graphics(),
							VK_ VK_PIPELINE_BIND_POINT_GRAPHICS,
							layout,
							0u,
							::std::uint32_t(span.size()), span.data(),
							0u, nullptr);
					}
				}

				VK_ VkRenderPassBeginInfo info{
					.sType = VK_ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.pNext = next,
					.renderPass = render_pass_,
					.framebuffer = framebuffers_[base::frame_index()],
					.renderArea = {.extent{ width_, height_ } },
				};
				VK_ vkCmdBeginRenderPass(base::graphics(), &info, content);

				pass_data<inside_pass<E, base::pass_index>> pass_event{ {{e}}, ::std::uint16_t(0u) };
				base::as_self().receive(pass_event, invoker);

				VK_ vkCmdEndRenderPass(base::graphics());
			}

			constexpr auto render_pass_handle() const noexcept { return render_pass_.value; }
			constexpr auto pipe(::std::uint32_t index) const noexcept { return pipelines_.at(index); }
			constexpr auto& attachments_view_indices() const noexcept { return attachment_view_indices_; }

		protected:
			::std::uint32_t width_, height_;
			move_only<VK_ VkRenderPass> render_pass_;
			::std::vector<VK_ VkPipeline> pipelines_;
			::std::vector<VK_ VkFramebuffer> framebuffers_;

		private:
			void clear() noexcept {
				auto hdv = T::template parent<device>()->device_handle();
				for (auto fmb : this->framebuffers_) {
					VK_ vkDestroyFramebuffer(hdv, fmb, T::allocator());
				}
				for (auto pipe : this->pipelines_) {
					VK_ vkDestroyPipeline(hdv, pipe, T::allocator());
				}
				VK_ vkDestroyRenderPass(hdv, render_pass_, T::allocator());
			}

			void clean_shaders(auto dv, auto& shaders) {
				for (auto [key, shader] : shaders) {
					VK_ vkDestroyShaderModule(dv, shader, T::allocator());
				}
			}
		};
	};

	template<>
	struct meta_of<render_pass> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x10001);
		static constexpr auto name = fixed_string{ "render_pass" };

		using order = order::at_middle;
		using extend = pass;

		template<typename T>
		using info = T;

		template<typename T>
		using make = T;
	};

	// using shader_mod_cinfo = VK_ VkShaderModuleCreateInfo;
	// using gpipe_cinfo = VK_ VkGraphicsPipelineCreateInfo;
#pragma region NOBODY LIKE GRAPHICS PIPELINE.
	[[maybe_unused]]
	inline constexpr VK_ VkPipelineVertexInputStateCreateInfo defaultVertexInputState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr
	};
	[[maybe_unused]]
	inline constexpr VK_ VkPipelineInputAssemblyStateCreateInfo defaultInputAssemblyState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.topology = VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE
	};
	[[maybe_unused]]
	inline constexpr VK_ VkPipelineTessellationStateCreateInfo defaultTessellationState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.patchControlPoints = 3
	};
	[[maybe_unused]]
	inline constexpr VK_ VkPipelineViewportStateCreateInfo defaultViewportState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr
	};
	[[maybe_unused]]
	inline constexpr VK_ VkPipelineRasterizationStateCreateInfo defaultRasterizationState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_ VK_POLYGON_MODE_FILL,
		.cullMode = VK_ VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_ VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f
	};

	[[maybe_unused]]
	inline constexpr VK_ VkPipelineMultisampleStateCreateInfo defaultMultisampleState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = VK_ VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE
	};

	[[maybe_unused]]
	inline constexpr VK_ VkPipelineDepthStencilStateCreateInfo defaultDepthStencilState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthTestEnable = VK_FALSE,
		.depthWriteEnable = VK_FALSE,
		.depthCompareOp = VK_ VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.front = {
			.failOp = VK_ VK_STENCIL_OP_KEEP,
			.passOp = VK_ VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_ VK_STENCIL_OP_KEEP,
			.compareOp = VK_ VK_COMPARE_OP_NEVER,
			.compareMask = 0,
			.writeMask = 0,
			.reference = 0
		},
		.back = {
			.failOp = VK_ VK_STENCIL_OP_KEEP,
			.passOp = VK_ VK_STENCIL_OP_KEEP,
			.depthFailOp = VK_ VK_STENCIL_OP_KEEP,
			.compareOp = VK_ VK_COMPARE_OP_NEVER,
			.compareMask = 0,
			.writeMask = 0,
			.reference = 0
		},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};

	[[maybe_unused]]
	inline constexpr VK_ VkPipelineColorBlendAttachmentState defaultColorBlendAttachment{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_ VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_ VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_ VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_ VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_ VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_ VK_BLEND_OP_ADD,
		.colorWriteMask = VK_ VK_COLOR_COMPONENT_R_BIT | VK_ VK_COLOR_COMPONENT_G_BIT |
						  VK_ VK_COLOR_COMPONENT_B_BIT | VK_ VK_COLOR_COMPONENT_A_BIT
	};

	[[maybe_unused]]
	inline constexpr VK_ VkPipelineColorBlendStateCreateInfo defaultColorBlendState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_ VK_LOGIC_OP_COPY,
		.attachmentCount = 0,
		.pAttachments = nullptr,
		.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
	};
	[[maybe_unused]]
	inline constexpr VK_ VkPipelineDynamicStateCreateInfo defaultDynamicState{
		.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.dynamicStateCount = 0,
		.pDynamicStates = nullptr
	};

#pragma endregion

	template<typename N, VK_ VkPipelineStageFlags Stage>
	struct basic_stage_info : N {
		static constexpr auto is_top_of_pipe = (Stage == VK_ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

		using base = N;
		using base::base;

		template<typename, VK_ VkPipelineStageFlags>
		friend struct basic_stage_info;

		static consteval auto pipe_stage() noexcept {
			return Stage;
		}

		constexpr bool setup_backward(infomation_of<pipe> auto& p) {
			if (pipe == invalid) {
				if constexpr (!is_top_of_pipe) {
					subpass = p.subpass;
				}
				pipe = p.pipe;
				return true;
			}
			else {
				return false;
			}
		}

		constexpr bool setup_backward(infomation_of<pass> auto& p) {
			if (this->pass == invalid) {
				this->pass = p.pass;
				return true;
			}
			else {
				return false;
			}
		}

		constexpr void relocate() noexcept {
			base::relocate();
		}

		constexpr void set_connectable() {
			pipe = invalid;
			subpass = invalid;
			pass = invalid;
			base::set_connectable();
		}

		::std::uint16_t pass = invalid;
		::std::uint16_t subpass = invalid;
		::std::uint16_t pipe = invalid;

	private:
		static constexpr bool top_of_pipe(VK_ VkPipelineStageFlags v = Stage) noexcept {
			return v == VK_ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}
	};


	// template<typename T, ::std::size_t index>
	// struct inside_pipe {
	//     static constexpr auto pipe_index = index;
	// };
	// template<typename T, template<typename>typename EventType, typename Next>
	// concept event_in_pipe = event_in_pass<T, EventType, Next>&& Next::pipe_index == T::pipe_index;

	template<typename T, VK_ VkPipelineStageFlagBits Current>
	struct basic_stage_make : T {
		constexpr basic_stage_make(auto&&...others)
			: T{ forward_(others)... } {
		}
	};

	using namespace pipe_extensions;
	template<typename T>
	struct basic_pipe_info : basic_barrier_point_info<basic_stage_info<T, VK_ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT>> {
		using base = basic_stage_info<T, VK_ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT>;

		constexpr basic_pipe_info(pipe const& pipe, auto&& infos)
			: base{ forward_(infos) }
			, subpass{ pipe.subpass }
		{
		}

		template<typename U>
		constexpr auto receive(unbinded<U> view) {
			auto it = set_indices.begin();
			while (it != set_indices.end() && *it < view.set) { it++; }
			if (it == set_indices.end() || *it != view.set) {
				set_indices.insert(it, ::std::uint16_t(view.set));
			}
		}

		constexpr bool setup_backward(infomation_of<pass> auto& other) {
			if (pipe_layout_index == invalid) {
				pipe_layout_index = other.receive(pipe_layout, push_constants, set_indices);
				return true;
			}
			else {
				return false;
			}
		}

		constexpr void relocate() noexcept {
			base::relocate();
			pipe_layout.pushConstantRangeCount = ::std::uint32_t(push_constants.size());
			pipe_layout.pPushConstantRanges = push_constants.data();
			pipe_layout.setLayoutCount = ::std::uint32_t(set_indices.size());
		}

		constexpr void set_connectable() {
			pipe_layout_index = invalid;
			base::set_connectable();
		}

		::std::uint16_t pipe_layout_index = invalid;
		VK_ VkPipelineLayoutCreateInfo pipe_layout{
			.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		};
		::std::vector<::std::uint16_t> set_indices;
		::std::vector<VK_ VkPushConstantRange> push_constants;
	};

	template<typename T, VK_ VkPipelineBindPoint Point>
	struct basic_pipe_make : basic_stage_make<image_barrier_point<T>, VK_ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT> {
		static constexpr auto pipe_pass = T::pass_index;
		static constexpr auto pipe_index = []() constexpr {
			if constexpr (requires{ T::pipe_pass; }) {
				if constexpr (T::pipe_pass == pipe_pass) {
					return T::pipe_index + 1u;
				}
				else {
					return 0u;
				}
			}
			else {
				return 0u;
			}
			}();

		using base = basic_stage_make<image_barrier_point<T>, VK_ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT>;

		constexpr basic_pipe_make(auto&& info, auto&&...infos)
			: base{ forward_(infos)... }
			, subpass_{ info.subpass } {
		}

		template<event_in_pass<begin_data, basic_pipe_make> E>
		void receive(E& e, auto& invoker) {
			T::receive(e, invoker);

			if (subpass_ != e.subpass) {
				T::as_local().on_next_subpass();
				e.next_subpass();
			}

			for (auto const& point : this->points_) {
				auto span = ::std::span{ this->image_barriers_ }.subspan(point.offset, point.count);
				VK_ vkCmdPipelineBarrier(e.current(),
					point.front_stages, point.trigger_stages, point.dependencies,
					0u, nullptr,
					0u, nullptr,
					::std::uint32_t(span.size()), span.data());
			}

			VK_ vkCmdBindPipeline(e.current(), Point, T::pipeline(pipe_index));
		}

	private:
		::std::uint16_t subpass_;
	};

	template<> struct meta_of<pipe> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x0u);
		using order = order::at_middle;
		using extend = extend_any;

		template<typename T>
		struct info;

		// traditional graphics pipeline.
		template<typename T>
		struct tgi : basic_pipe_info<T> {
			using base = basic_pipe_info<T>;
			constexpr tgi(pipe const& pipe, auto&& infos)
				: base{ pipe, forward_(infos) } {
				this->subpass = pipeline.subpass = pipe.subpass;
				pipeline.sType = VK_ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
				pipeline.pVertexInputState = &defaultVertexInputState;
				pipeline.pInputAssemblyState = &defaultInputAssemblyState;
				pipeline.pTessellationState = &defaultTessellationState;
				pipeline.pViewportState = &defaultViewportState;
				pipeline.pRasterizationState = &defaultRasterizationState;
				pipeline.pMultisampleState = &defaultMultisampleState;
				pipeline.pDepthStencilState = &defaultDepthStencilState;
				pipeline.pColorBlendState = &defaultColorBlendState;
				pipeline.pDynamicState = &defaultDynamicState;
				pipeline.subpass = pipe.subpass;

				subpass.pipelineBindPoint = VK_ VK_PIPELINE_BIND_POINT_GRAPHICS;
			}
			constexpr tgi(auto&& infos) : tgi{ get_by<T>(infos), forward_(infos) } {}

			constexpr auto receive(VK_ VkPipelineVertexInputStateCreateInfo const* input) {
				if (input) {
					pipeline.pVertexInputState = input;
				}
				else {
					pipeline.pVertexInputState = &defaultVertexInputState;
				}
			}

			constexpr auto receive(VK_ VkPipelineInputAssemblyStateCreateInfo const* assembly = nullptr) {
				if (assembly) {
					pipeline.pInputAssemblyState = assembly;
				}
				else {
					pipeline.pInputAssemblyState = &defaultInputAssemblyState;
				}
			}

			constexpr auto setup_backward(infomation_of<pass> auto& rp) {
				if (base::setup_backward(rp)) {
					if constexpr (requires { rp.receive(pipeline, subpass); }) {
						rp.receive(pipeline, subpass);
					}
					else {
						rp.receive(pipeline);
					}
					VkSubpassDescription;
					return true;
				}
				else {
					return false;
				}
			}

			VK_ VkGraphicsPipelineCreateInfo pipeline;
			transformed<VK_ VkSubpassDescription, T> subpass;

		};

		template<typename T>
		struct tci : basic_pipe_info<T> {
			using base = basic_pipe_info<T>;
			constexpr tci(auto&& infos)
				: base{ get_by<T>(infos), forward_(infos) } {
				pipeline.sType = VK_ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			}

			void relocate() noexcept {
				pipeline.stages.module = this->shader_;
				T::relocate();
			}

			constexpr auto receive(VK_ VkShaderModuleCreateInfo shader, VK_ VkPipelineShaderStageCreateInfo stages) {
				assert(stages.stages & VK_ VK_SHADER_STAGE_COMPUTE_BIT);
				pipeline.stages = ::std::move(stages);
				return 0u;
			}

			constexpr bool setup_backward(infomation_of<pass> auto& pass) {
				if (base::setup_backward(pass)) {
					this->pipe_index = pass->receive(pipeline);
					return true;
				}
				else {
					return true;
				}
			}

			VK_ VkComputePipelineCreateInfo pipeline;
		};

		template<typename T>
			requires(contains<graphics> || contains<attachment>)
		struct info<T> : tgi<T> { using tgi<T>::tgi; };
		template<contains<compute> T>
		struct info<T> : tci<T> { using tci<T>::tci; };

		template<typename T>
		struct make;

		template<local_contains<compute> T>
		struct make<T> : basic_pipe_make<T, VK_ VK_PIPELINE_BIND_POINT_COMPUTE> {
			using base = basic_pipe_make<T, VK_ VK_PIPELINE_BIND_POINT_COMPUTE>;

			using base::base;
		};

		template<local_contains<graphics> T>
		struct make<T> : basic_pipe_make<T, VK_ VK_PIPELINE_BIND_POINT_GRAPHICS> {
			using base = basic_pipe_make<T, VK_ VK_PIPELINE_BIND_POINT_GRAPHICS>;
			using base::base;

			template<event_in_pass<pass_data, make> E>
			void on_next_subpass(E& e, auto& invoker, VK_ VkSubpassContents content = VK_ VK_SUBPASS_CONTENTS_INLINE) {
				if (e.subpass != this->subpass) {
					VK_ vkCmdNextSubpass(e.current(), content);
				}
			}
		};
	};

	template<>
	struct meta_of<attachment> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x20001);
		static constexpr auto name = fixed_string{ "attachment" };

		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct info : T {
			using base = T;
			constexpr info(auto&& infos)
				: base{ forward_(infos) }
				, attinfo{ get_by<T>(forward_(infos)) } {
				view().usages
					= attinfo.color
					? view_usage::color
					: (attinfo.depth || attinfo.stencil)
					? (attinfo.depth ? view_usage::depth : 0u) | (view_usage::stencil ? view_usage::stencil : 0u)
					: attinfo.resolve
					? view_usage::resolve
					: attinfo.input
					? view_usage::input;
				assert(this->usages); // cannot no usage attachment.
			}

			constexpr auto setup_backward(infomation_of<pipe, graphics> auto& pipe) {
				if (subpass == invalid) {
					subpass = pipe.subpass;
					return true;
				}
				else {
					return false;
				}
			}

			constexpr auto setup_forward(infomation_of<pass> auto& rp) {
				if (pass == invalid) {
					assert(subpass != invalid); // require pass at top of pipe and pipe is graphics pipe.
					assert(view_index != invalid); // require already bind to bind points.
					rp.receive(subpass, view_index, attinfo);
					pass = rp.pass;
					return true;
				}
				else {
					return false;
				}
			}

			constexpr void set_connectable() {
				pass = invalid;
				subpass = invalid;
				base::set_connectable();
			}

			::std::uint16_t view_index = invalid;
			::std::uint16_t pass = invalid;
			::std::uint16_t subpass = invalid;
			attachment attinfo;
		};

		template<typename T>
		using make = T;
	};

	template<VK_ VkShaderStageFlagBits STAGE>
	struct map_shader_to_pipeline_stage {};
	// graphics shader basic info.
	template<typename T, VK_ VkShaderStageFlagBits STAGE>
	struct basic_shader_info : basic_stage_info<T, map_shader_to_pipeline_stage<STAGE>::value> {
		using base = basic_stage_info<T, map_shader_to_pipeline_stage<STAGE>::value>;

		constexpr basic_shader_info(byte_view bytes, auto&& info)
			: base{ forward_(info) }
			, bytes{ bytes.begin(), bytes.end() }
			, shader_module{
				.sType = VK_ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = ::std::uint32_t(bytes.size()),
				.pCode = reinterpret_cast<::std::uint32_t const*>(bytes.data()),
			}
			, shader_stage{
				.sType = VK_ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stages = STAGE,
				.pName = "main",
			} {
		}

		constexpr bool setup_backward(infomation_of<pipe> auto& p) {
			if (base::setup_backward(p)) {
				pipe = p.pipe;
				return true;
			}
			else {
				return false;
			}
		}

		constexpr void relocate() noexcept {
			shader_module.codeSize = ::std::uint32_t(bytes.size());
			shader_module.pCode = reinterpret_cast<::std::uint32_t const*>(bytes.data());
		}

		::std::uint16_t pass = invalid;
		::std::uint16_t pipe = invalid;
		::std::vector<::std::byte> bytes;
		VK_ VkShaderModuleCreateInfo shader_module;
		VK_ VkPipelineShaderStageCreateInfo shader_stage;
	};

	template<::std::size_t index = 0>
	struct format_to_size_t {};
	template<>
	struct format_to_size_t<0> {
		template<typename T>
		constexpr ::std::size_t operator()(T format) const noexcept {
			::std::size_t size = 0;
			switch (format) {
				// intellsense automatically relayout it as this.
				// I hate it, but i got no time to fix it.
				case VK_ VK_FORMAT_R16_UINT : size = 2; break;
					case VK_ VK_FORMAT_R32_UINT : size = 4; break;

						case VK_ VK_FORMAT_R32_SFLOAT : size = 4; break;
							case VK_ VK_FORMAT_R32G32_SFLOAT : size = 8; break;
								case VK_ VK_FORMAT_R32G32B32_SFLOAT : size = 12; break;
									case VK_ VK_FORMAT_R32G32B32A32_SFLOAT : size = 16; break;

										case VK_ VK_FORMAT_R8G8B8A8_UNORM :
											case VK_ VK_FORMAT_R8G8B8A8_SNORM :
												case VK_ VK_FORMAT_R8G8B8A8_UINT : size = 4; break;

													case VK_ VK_FORMAT_A2B10G10R10_UNORM_PACK32 :
														size = 4;
														break;

													default: break;
			}

			if constexpr (::std::invocable<format_to_size_t<1>, T>)
				return size != 0 ? size : format_to_size_t<1>()(format);
			else
				return size;
		}
	};
	template<::std::size_t index = 0u>
	constexpr format_to_size_t<index> format_to_size{};


	using namespace vertex_input_extensions;

	template<typename T>
	struct affine_vertex_format {};

	template<> struct affine_vertex_format<math::vec1f> { static constexpr auto format = VK_ VK_FORMAT_R32_SFLOAT; };
	template<> struct affine_vertex_format<math::vec2f> { static constexpr auto format = VK_ VK_FORMAT_R32G32_SFLOAT; };
	template<> struct affine_vertex_format<math::vec3f> { static constexpr auto format = VK_ VK_FORMAT_R32G32B32_SFLOAT; };
	template<> struct affine_vertex_format<math::vec4f> { static constexpr auto format = VK_ VK_FORMAT_R32G32B32A32_SFLOAT; };

	template<> struct affine_vertex_format<math::vec1d> { static constexpr auto format = VK_ VK_FORMAT_R64_SFLOAT; };
	template<> struct affine_vertex_format<math::vec2d> { static constexpr auto format = VK_ VK_FORMAT_R64G64_SFLOAT; };
	template<> struct affine_vertex_format<math::vec3d> { static constexpr auto format = VK_ VK_FORMAT_R64G64B64_SFLOAT; };
	template<> struct affine_vertex_format<math::vec4d> { static constexpr auto format = VK_ VK_FORMAT_R64G64B64A64_SFLOAT; };

	template<> struct affine_vertex_format<math::vec1i32> { static constexpr auto format = VK_ VK_FORMAT_R32_SINT; };
	template<> struct affine_vertex_format<math::vec2i32> { static constexpr auto format = VK_ VK_FORMAT_R32G32_SINT; };
	template<> struct affine_vertex_format<math::vec3i32> { static constexpr auto format = VK_ VK_FORMAT_R32G32B32_SINT; };
	template<> struct affine_vertex_format<math::vec4i32> { static constexpr auto format = VK_ VK_FORMAT_R32G32B32A32_SINT; };

	template<> struct affine_vertex_format<math::vec1u32> { static constexpr auto format = VK_ VK_FORMAT_R32_UINT; };
	template<> struct affine_vertex_format<math::vec2u32> { static constexpr auto format = VK_ VK_FORMAT_R32G32_UINT; };
	template<> struct affine_vertex_format<math::vec3u32> { static constexpr auto format = VK_ VK_FORMAT_R32G32B32_UINT; };
	template<> struct affine_vertex_format<math::vec4u32> { static constexpr auto format = VK_ VK_FORMAT_R32G32B32A32_UINT; };

	template<> struct affine_vertex_format<math::vec1i16> { static constexpr auto format = VK_ VK_FORMAT_R16_SINT; };
	template<> struct affine_vertex_format<math::vec2i16> { static constexpr auto format = VK_ VK_FORMAT_R16G16_SINT; };
	template<> struct affine_vertex_format<math::vec3i16> { static constexpr auto format = VK_ VK_FORMAT_R16G16B16_SINT; };
	template<> struct affine_vertex_format<math::vec4i16> { static constexpr auto format = VK_ VK_FORMAT_R16G16B16A16_SINT; };

	template<> struct affine_vertex_format<math::vec1u16> { static constexpr auto format = VK_ VK_FORMAT_R16_UINT; };
	template<> struct affine_vertex_format<math::vec2u16> { static constexpr auto format = VK_ VK_FORMAT_R16G16_UINT; };
	template<> struct affine_vertex_format<math::vec3u16> { static constexpr auto format = VK_ VK_FORMAT_R16G16B16_UINT; };
	template<> struct affine_vertex_format<math::vec4u16> { static constexpr auto format = VK_ VK_FORMAT_R16G16B16A16_UINT; };

	template<> struct affine_vertex_format<math::vec1i8> { static constexpr auto format = VK_ VK_FORMAT_R8_SINT; };
	template<> struct affine_vertex_format<math::vec2i8> { static constexpr auto format = VK_ VK_FORMAT_R8G8_SINT; };
	template<> struct affine_vertex_format<math::vec3i8> { static constexpr auto format = VK_ VK_FORMAT_R8G8B8_SINT; };
	template<> struct affine_vertex_format<math::vec4i8> { static constexpr auto format = VK_ VK_FORMAT_R8G8B8A8_SINT; };

	template<> struct affine_vertex_format<math::vec1u8> { static constexpr auto format = VK_ VK_FORMAT_R8_UINT; };
	template<> struct affine_vertex_format<math::vec2u8> { static constexpr auto format = VK_ VK_FORMAT_R8G8_UINT; };
	template<> struct affine_vertex_format<math::vec3u8> { static constexpr auto format = VK_ VK_FORMAT_R8G8B8_UINT; };
	template<> struct affine_vertex_format<math::vec4u8> { static constexpr auto format = VK_ VK_FORMAT_R8G8B8A8_UINT; };

	template<typename T>
	struct affine_index_type : const_<VK_ VkIndexType(0u)> {};

	template<> struct affine_index_type<::std::uint16_t> : const_<VK_ VK_INDEX_TYPE_UINT16> {};
	template<> struct affine_index_type<::std::uint32_t> : const_<VK_ VK_INDEX_TYPE_UINT32> {};
	// NOTICED that, in vulkan 1.0, VK_INDEX_TYPE_UINT8 is an extensions and not recommand to use.
	template<> struct affine_index_type<::std::uint8_t> : const_<VK_ VK_INDEX_TYPE_UINT8> {};

	template<typename T, typename...Ts>
	struct vertex_binded : T {
		::std::uint32_t binding;
		VK_ VkVertexInputRate rate = VK_ VK_VERTEX_INPUT_RATE_VERTEX;
	};

	template<typename T, typename Type>
	struct vertex_indexed : T {
		VK_ VkIndexType type = affine_index_type<Type>::value;
	};

	template<template<typename>typename C = cspan>
	struct vertex_buffer_info {
		C<VK_ VkBuffer> buffers;
		C<VK_ VkDeviceSize> offsets;
	};

	template<>
	struct meta_of<vertex_input> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x1000);
		static constexpr auto name = fixed_string{ "vertex_input" };
		using order = order::at_middle;
		using extend = void;

		struct index_typed {
			VK_ VkIndexType type;
		};

		template<typename T>
		struct info : control_connectable<basic_stage_info<T, VK_ VK_PIPELINE_STAGE_VERTEX_INPUT_BIT>> {
			using base = control_connectable<basic_stage_info<T, VK_ VK_PIPELINE_STAGE_VERTEX_INPUT_BIT>>;

			constexpr info(auto const& infos) : base{ infos } {}

			template<typename U>
			constexpr void receive(unattached<U> view)
				requires (::std::derived_from<U, VK_ VkBufferViewCreateInfo>) {
				assert((view.usages & view_usage::index) != 0u); // this function is only for index buffer's.
				receive(index_buffers, view);

				base::receive(::std::move(view));
			}
			template<typename U, typename...Dts>
			constexpr void receive(vertex_binded<U, Dts...> view)
				requires (::std::derived_from<U, VK_ VkBufferViewCreateInfo>) /*vertex input stages only allow buffer*/ {
				assert((view.usages & view_usage::vertex) != 0u); // only vertex buffer can append at vertex input stages with binding.
				if (view.binding == invalid) {
					view.binding = input_bindings.size();
				}
				receive(vertex_buffers, view);
				receive(VK_ VkVertexInputBindingDescription{ view.binding, ((sizeof(Dts)) + ...), view.rate },
					make_attr(view, type_list<Dts...>(), ::std::make_index_sequence<sizeof...(Dts)>()));

				base::receive(::std::move(view));
			}

			template<infomation_of<pipe> P>
			void connect(P& pipes) {
				if (base::connectable()) {
					if constexpr (contains<P, graphics>) {
						pipes.receive(&inputs);
						base::set_connectable(false);
					}
					else {
						assert(!"Not allow connect vertex input stages with non-graphics pipeline.");
					}
				}
			}

			void relocate() {
				base::relocate();

				inputs.vertexBindingDescriptionCount = ::std::uint32_t(input_bindings.size());
				inputs.pVertexBindingDescriptions = input_bindings.data();
				inputs.vertexAttributeDescriptionCount = ::std::uint32_t(input_attributes.size());
				inputs.pVertexAttributeDescriptions = input_attributes.data();
			}

			void set_connectable() {
				base::set_connectable(true);
			}

			VK_ VkPipelineVertexInputStateCreateInfo inputs = defaultVertexInputState;
			::std::vector<unattached<range<>>> vertex_buffers;
			::std::vector<unattached<range<index_typed>>> index_buffers;

			::std::vector<VK_ VkVertexInputBindingDescription>   input_bindings;
			::std::vector<VK_ VkVertexInputAttributeDescription> input_attributes;

		private:
			template<typename Type, ::std::size_t index>
			static constexpr auto make_attr_impl(auto const& view) {
				return VK_ VkVertexInputAttributeDescription{
					.location = index,
					.binding = view.binding,
					.format = affine_vertex_format<Type>::format,
					.offset = view.offset,
				};
			}
			template<typename...Ts, ::std::size_t...ids>
			static constexpr auto make_attr(auto const& view, type_list<Ts...>, ::std::index_sequence<ids...>) {
				return::std::array{ make_attr_impl<Ts, ids>(view)... };
			}

			static void receive(auto& ranges, auto const& view) {
				auto it = ranges.begin();
				while (it != ranges.end() && it->index < view.index) { it++; }
				if (it == ranges.end() || view.index != it->index) {
					auto& val = ranges.emplace(it);
					val.index = view.index;
					val.offset = view.offset;
					val.size = view.range;
				}
				else {
					it->index = view.index;
					it->offset = view.offset;
					it->size = view.range;
				}
			}

			void receive(VK_ VkVertexInputBindingDescription binding,
				::std::span<VK_ VkVertexInputAttributeDescription> inputs) { // just overwrite.
				auto itb = input_bindings.begin();
				while (input_bindings.end() != itb && itb->binding < binding.binding) { itb++; }
				if (input_bindings.end() == itb || itb->binding != binding.binding) {
					itb = input_bindings.insert(itb, ::std::move(binding));
				}
				else {
					*itb = ::std::move(binding);
				}

				auto ita = input_attributes.begin();
				auto itae = input_attributes.end();
				while (input_attributes.end() != ita && ita->binding < ita->binding) { ita++; }
				if (input_attributes.end() == ita || ita->binding != ita->binding) {
					ita = input_attributes.insert(ita, inputs.begin(), inputs.end());
					itae = ita + inputs.size();
				}
				else {
					itae = ita + 1u;
					while (input_attributes.end() != itae && itae->binding == ita->binding) { itae++; }
					itae = input_attributes.erase(ita, itae);
					ita = input_attributes.insert(itae, inputs.begin(), inputs.end());
					itae = ita + inputs.size();
				}

				auto accumulate = 0u;
				for (; ita != itae; ita++) {
					ita->binding = binding.binding;
					accumulate += format_to_size<>(ita->format);
				}
				itb->stride = accumulate;
			}
		};

		struct index_buffer_info {
			VK_ VkBuffer buffer;
			VK_ VkDeviceSize offset;
			VK_ VkIndexType type;
		};

		template<typename N>
		struct make : N {
			template<infomation_of<vertex_input> F>
			constexpr make(F&& info, auto&&...others)
				: N{ forward_(others)... } {
				auto bp = N::template parent<bind_points>();

				const auto fc = bp->frame_count();
				const auto size_vbo = info.vertex_buffers.size();
				const auto size_ibo = info.index_buffers.size();

				auto first = true;

				vertex_buffers_.reserve(size_vbo * fc);
				vertex_buffers_offsets_.reserve(size_vbo);
				index_buffers_.reserve(size_ibo * fc);
				for (auto i{ 0u }; i < bp->frame_count(); i++) {
					for (auto const& buf : info.vertex_buffers) {
						if (first) {
							vertex_buffers_offsets_.emplace_back(buf.offset);
						}
						vertex_buffers_.emplace_back(bp->buffer(i, buf.index));
					}
					for (auto const& buf : info.index_buffers) {
						index_buffers_.emplace_back(bp->buffer(i, buf.index), buf.offset, buf.type);
					}
					first = false;
				}
			}

			constexpr auto vertex_buffer(::std::size_t begin, ::std::size_t end = maximum) const noexcept {
				auto const size = vertex_buffers_offsets_.size();
				auto const count = end - begin;
				auto bufs = ::std::span{ vertex_buffers_.data() + N::frame_index() * size + begin, count };
				auto ids = ::std::span{ vertex_buffers_offsets_.data() + begin, count };
				return vertex_buffer_info<cspan>{ bufs, ids };
			}

			constexpr auto vertex_buffer(::std::span<::std::size_t> indices) {
				auto const size = vertex_buffers_offsets_.size();
				vertex_buffer_info<::std::vector> info{};
				info.buffers.reserve(indices.size());
				info.offsets.reserve(indices.size());
				for (auto idx : indices) {
					info.buffers.push_back(vertex_buffers_[idx + N::frame_index() * size]);
					info.offsets.push_back(vertex_buffers_offsets_[idx]);
				}
				return info;
			}

			constexpr auto index_buffer(::std::size_t index) const noexcept {
				auto size = index_buffers_.size() / N::frame_count();
				return index_buffers_[index + size * N::frame_index()];
			}

		protected:
			::std::vector<VK_ VkBuffer> vertex_buffers_;
			::std::vector<VK_ VkDeviceSize> vertex_buffers_offsets_;
			::std::vector<index_buffer_info> index_buffers_;
		};
	};

	template<typename...Dts>
	struct meta_of<vertex_binding<Dts...>> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x1001);
		static constexpr auto name = fixed_string{ "vertex_binding<>" };
		using order = order::at_middle;
		using extend = buffer_view;

		using type = vertex_binding<Dts...>;

		template<typename Desc>
		using apply = typename Desc::template apply<Dts...>;

		template<typename T>
		struct info : T {
			constexpr info(auto&& infos) : T{ forward_(infos) } {
				auto const& c = get_by<type>(infos);
				auto& view = T::view();
				view.usages &= (~view_usage::type(0u) ^ ~::std::uint32_t(0u));
				view.usages |= view_usage::vertex;
				view.binding = c.binding;
				view.range = (... + (sizeof(Dts)));
				view.rate = c.each_instance ? VK_ VK_VERTEX_INPUT_RATE_INSTANCE : VK_ VK_VERTEX_INPUT_RATE_VERTEX;
			}

			constexpr void setup_forward(infomation_of<vertex_input> auto& pipe) const {
				auto& view = this->view();
				if (view.stages != VK_ VK_PIPELINE_STAGE_NONE) {
					pipe.receive(view);
					view.stages = pipe.stages();
				}
			}
		private:
			using T::setup_forward;
		};
	};

	template<typename Type>
	struct meta_of<vertex_indices<Type>> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x1002);
		static constexpr auto name = fixed_string{ "vertex_indices<>" };
		using order = order::at_middle;
		using extend = buffer_view;

		using type = vertex_indices<Type>;

		template<typename Desc>
		using apply = typename Desc::template apply<Type>;

		template<typename O>
		using transform = vertex_indexed<O, Type>;

		template<typename N>
		struct info : N {
			constexpr info(auto&& infos) : N{ forward_(infos) } {
				auto const& c = get_by<type>(infos);
				auto& view = N::view();
				view.usages &= (~view_usage::type(0u) ^ ~::std::uint32_t(0u));
				view.usages |= view_usage::index;
			}
		};
	};


	using namespace input_assembly_extensions;
	template<>
	struct meta_of<input_assembly> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x1500);
		static constexpr auto name = fixed_string{ "input_assembly" };

		using order = order::at_middle;
		using extend = void;

		static constexpr auto to_topology(input_assembly ia) noexcept {
			if (ia.point) {
				return VK_ VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			}

			if (ia.line) {
				if (ia.strip) {
					return ia.adjacent ? VK_ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY
						: VK_ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
				}
				return ia.adjacent ? VK_ VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY
					: VK_ VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			}

			if (ia.triangle) {
				if (ia.strip) {
					return ia.adjacent ? VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY
						: VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				}
				if (ia.fan) {
					return VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
				}
				return ia.adjacent ? VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY
					: VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			}

			if (ia.patch) {
				return VK_ VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
			}

			return VK_ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		}

		template<typename N>
		struct info : control_connectable<N> {
			constexpr info(auto&& info)
				: N{ forward_(info) } {
				input_assembly.topology = to_topology(get_by<input_assembly>(info));
			}

			constexpr void connect(infomation_of<pipe, graphics> auto& info) {
				if (this->connectable()) {
					info.receive(&input_assembly);
					this->set_connectable(false);
				}
			}

			VK_ VkPipelineInputAssemblyStateCreateInfo
				input_assembly = defaultInputAssemblyState;
		};

		template<typename T>
		using make = T; // input assembly must have make for extensions::dynamic to set dynamic state.
	};

	template<>
	struct map_shader_to_pipeline_stage<VK_ VK_SHADER_STAGE_VERTEX_BIT>
		: const_<VK_ VK_PIPELINE_STAGE_VERTEX_SHADER_BIT> {
	};
	template<> struct meta_of<vertex_shader> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x1000);
		static constexpr auto name = fixed_string{ "vertex_shader" };
		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct info : basic_shader_info<T, VK_ VK_SHADER_STAGE_VERTEX_BIT> {
			using base = basic_shader_info<T, VK_ VK_SHADER_STAGE_VERTEX_BIT>;
			constexpr info(vertex_shader const& vsd, auto const& infos)
				: base{ vsd.bytes, infos } {
			}
			constexpr info(auto const& infos)
				: info{ get_by<vertex_shader>(infos), infos } {
			}
		};

		template<typename T>
		using make = skipped_make<T>;
	};

	template<>
	struct map_shader_to_pipeline_stage<VK_ VK_SHADER_STAGE_FRAGMENT_BIT>
		: const_<VK_ VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT> {
	};
	template<> struct meta_of<pixel_shader> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x1500);
		static constexpr auto name = fixed_string{ "pixel_shader" };

		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct info : basic_shader_info<T, VK_ VK_SHADER_STAGE_FRAGMENT_BIT> {
			using base = basic_shader_info<T, VK_ VK_SHADER_STAGE_FRAGMENT_BIT>;
			constexpr info(pixel_shader const& vsd, auto const& infos)
				: base{ vsd.bytes, infos } {
			}
			constexpr info(auto const& infos)
				: info{ get_by<pixel_shader>(infos), infos } {
			}
		};

		template<typename T>
		using make = skipped_make<T>;
	};

	template<>
	struct map_shader_to_pipeline_stage<VK_ VK_SHADER_STAGE_COMPUTE_BIT>
		: const_<VK_ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT> {
	};
	template<> struct meta_of<compute_shader> {
		static constexpr auto type_id = make_type_id(PASS_SCOPE, 0x1500);
		static constexpr auto name = fixed_string{ "comput_shader" };

		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct info : basic_shader_info<T, VK_ VK_SHADER_STAGE_COMPUTE_BIT> {
			using base = basic_shader_info<T, VK_ VK_SHADER_STAGE_COMPUTE_BIT>;
			constexpr info(compute_shader const& vsd, auto const& infos)
				: base{ vsd.bytes, infos } {
			}
			constexpr info(auto const& infos)
				: info{ get_by<compute_shader>(infos), infos } {
			}
		};

		template<typename T>
		using make = skipped_make<T>;
	};

	using namespace draw_extensions;
	template<>
	struct meta_of<draw> {
		static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x1500);
		static constexpr auto name = fixed_string{ "draw" };

		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct info : T {
			constexpr info(auto&& infos) : T{ forward_(infos) } {
			}
		};

		template<typename T>
		struct make : T {
			template<infomation_of<draw> F>
			constexpr make(F&& info, auto&&...others)
				: T{ forward_(others)... } {
			}

			template<typename E>
			void receive(E& data, auto& invoker) {
				T::receive(data, invoker);
			}
		};
	};

	template<> struct meta_of<indexed> {
		static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x1501);
		static constexpr auto name = fixed_string{ "indexed" };

		using order = order::at_middle;
		using extend = draw;
		using type = indexed;

		template<typename T>
		struct info : T {
			constexpr info(auto&& infos)
				: T{ forward_(infos) }
				, command{ get_by<T>(infos) }
			{
			}

			type command;
		};

		template<typename T>
		struct make : T {
			constexpr make(auto&& info, auto&&...infos)
				: T{ forward_(info), forward_(infos)... }
				, cmd_{ get_by<T>(forward_(info)).command } {
			}

			template<typename E>
			void receive(E& data, auto& invoker) {
				T::receive(data, invoker);
				VK_ vkCmdDrawIndexed(data.graphics(),
					cmd_.index_count, cmd_.instance_count,
					cmd_.first_index, cmd_.vertex_offset, cmd_.first_instance);
			}

		private:
			type cmd_;
		};
	};

	template<> struct meta_of<instanced> {
		static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x1502);
		static constexpr auto name = fixed_string{ "instanced" };

		using order = order::at_middle;
		using extend = draw;
		using type = instanced;

		template<typename T>
		struct info : T {
			constexpr info(auto&& infos)
				: T{ forward_(infos) }
				, command{ get_by<T>(infos) } {
			}

			type command;
		};

		template<typename T>
		struct make : T {
			constexpr make(auto&& info, auto&&...infos)
				: T{ forward_(info), forward_(infos)... }
				, cmd_{ get_by<T>(forward_(info)).command } {
			}

			template<typename E>
			void receive(E& data, auto& invoker) {
				T::receive(data, invoker);
				VK_ vkCmdDraw(data.graphics()
					, cmd_.vertex_count, cmd_.instance_count
					, cmd_.first_vertex, cmd_.first_instance);
			}

		private:
			type cmd_;
		};
	};

	template<>
	struct meta_of<draw_vertex_by_range> {
		static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x1503);
		static constexpr auto name = fixed_string{ "draw_vertex_by_range" };

		using order = order::at_middle;
		using extend = draw;
		using type = draw_vertex_by_range;

		template<typename T>
		struct info : T {
			constexpr info(auto&& infos)
				: T{ forward_(infos) }
				, command{ get_by<T>(infos) } {
			}

			type command;
		};

		template<typename T>
		struct make : T {
			constexpr make(auto&& info, auto&&...infos)
				: T{ forward_(info), forward_(infos)... }
				, cmd_{ get_by<T>(forward_(info)).command } {
			}

			template<typename E>
			void receive(E& data, auto& invoker) {
				T::receive(data, invoker);
				auto vip = T::template parent<vertex_input>();
				auto c = vip->vertex_buffer(cmd_.begin, cmd_.end); // draw_vertex_by_range depend on vertex_input, remeber to add vertex input in pipe.
				VK_ vkCmdBindVertexBuffers(data.graphics(), cmd_.binding,
					::std::uint32_t(c.offsets.size()), c.buffers.data(), c.offsets.data());
			}

		private:
			type cmd_;
		};
	};

	template<>
	struct meta_of<draw_vertex_by_indices> {
		static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x1503);
		static constexpr auto name = fixed_string{ "draw_vertex_by_indices" };

		using order = order::at_middle;
		using extend = draw;
		using type = draw_vertex_by_range;

		template<typename T>
		struct info : T {
			constexpr info(auto&& infos)
				: T{ forward_(infos) } {
				auto c = get_by<T>(infos);
				indices.insert(c.indices.begin(), c.indices.end());
			}

			::std::uint16_t binding;
			::std::vector<::std::uint16_t> indices;
		};

		template<typename T>
		struct make : T {
			template<infomation_of<draw, draw_vertex_by_indices> F>
			constexpr make(F&& info, auto&&...infos)
				: T{ forward_(info), forward_(infos)... }
				, binding_{ get_by<T>(info).binding }
				, indices_{ forward_like<F>(get_by<T>(info).indices) } {
			}

			template<typename E>
			void receive(E& data, auto& invoker) {
				T::receive(data, invoker);
				auto vip = T::template parent<vertex_input>();
				auto c = vip->vertex_buffer(indices_); // draw_vertex_by_indices depend on vertex_input, remeber to add vertex input in pipe.
				VK_ vkCmdBindVertexBuffers(data.graphics(), binding_,
					::std::uint32_t(c.offsets.size()), c.buffers.data(), c.offsets.data());
			}

		private:
			::std::uint16_t binding_;
			::std::vector<::std::uint16_t> indices_;
		};
	};

	template<>
	struct meta_of<draw_on_index> {
		static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x1504);
		static constexpr auto name = fixed_string{ "draw_on_index" };

		using order = order::at_middle;
		using extend = draw;
		using type = draw_on_index;


		template<typename T>
		struct info : T {
			constexpr info(auto&& infos)
				: T{ forward_(infos) }
				, command{ get_by<T>(infos) } {
			}

			type command;
		};

		template<typename T>
		struct make : T {
			constexpr make(auto&& info, auto&&...infos)
				: T{ forward_(info), forward_(infos)... }
				, cmd_{ get_by<T>(forward_(info)).command } {
			}

			template<typename E>
			void receive(E& data, auto& invoker) {
				T::receive(data, invoker);
				auto vip = T::template parent<vertex_input>();
				auto c = vip->index_buffer(cmd_.index); // switch index depend on vertex input, remeber to add vertex input in pipe.
				VK_ vkCmdBindIndexBuffer(data.graphics(), c.buffer, c.offset, c.type);
			}

		private:
			type cmd_;
		};
	};

	template <>
	struct meta_of<present> {
		static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x5000);
		static constexpr auto name = fixed_string{ "present" };

		using extend = void;
		using order = order::at_middle;

		template<typename T>
		struct info : T {
			constexpr info(auto&& info)
				: T{ forward_(info) } {
				auto smps = get_by<T>(info).semaphores;
				semaphores = ::std::vector(smps.begin(), smps.end());
			}

			::std::vector<::std::uint16_t> semaphores;
		};

		template<typename T>
		struct make : basic_time_point<T> {
			using base = basic_time_point<T>;
			constexpr make(auto&& info, auto&&...infos)
				: basic_time_point<T>{ forward_(infos)... } {
				base::init_from_other(forward_(info));
			}

			template<event_in_task<submit_data, T> E>
			void receive(E& event, auto& invoker) {
				auto exec = T::template parent<execution>();
				auto sc = T::template parent<swapchain>();

				VK_ VkResult results[]{ VK_ VK_SUCCESS };
				VK_ VkSwapchainKHR handle[]{ sc->swapchain_handle() };
				::std::uint32_t indices[]{ T::frame_index() };

				VK_ VkPresentInfoKHR present{
					.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					.swapchainCount = 1u,
					.waitSemaphoreCount = ::std::uint32_t(semaphores_.size()),
					.pWaitSemaphores = semaphores_.data(),
					.pSwapchains = handle,
					.pImageIndices = indices,
					.pResults = results,
				};
				VK_ vkQueuePresentKHR(event, &present)
					| popup{ "[PRESENT] Submit present failure." };
				for (auto res : results) {
					res | popup{ "[PRESENT] Present failure." };
				}
			}

		private:
			::std::vector<VK_ VkSemaphore> semaphores_;
		};
	};

	template<>
	struct meta_of<copy_flow> {
		static constexpr auto type_id = make_type_id(EXECUTION_SCOPE, 0x6000u);
		static constexpr auto name = fixed_string{ "copy_flow" };

		using order = order::at_middle;
		using extend = void;

		template<typename T>
		struct info : basic_stage_info<T, VK_ VK_PIPELINE_STAGE_HOST_BIT> {
			using base = basic_stage_info<T, VK_ VK_PIPELINE_STAGE_HOST_BIT>;

			constexpr info(auto&& infos)
				: base{ forward_(infos) }
				, flow{ get_by<T>(forward_(infos)) }
			{
			}

			copy_flow flow;
		};

		template<typename T>
		struct make : T {
			constexpr make(infomation_of<copy_flow> auto&& info, auto&&...others)
				: T{ forward_(others)... } {
				// VK_ vkCmdCopyImageToBuffer;
			}

			template<typename E>
			void receive(E event, auto& invoker) {

			}

		};
	};

}


