#pragma once

VKTL_EXPORT_ namespace vktl::detail {

}

VKTL_EXPORT_ namespace vktl::detail {

	struct defualt_memory : copyable_if_null<VK_ VkDeviceMemory> {
		VK_ VkMemoryPropertyFlags property;
	};

	template<typename N>
	struct m<memory_allocator_, N> : N {


	private:
		defualt_memory memory;
	};

}