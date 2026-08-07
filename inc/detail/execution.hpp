#pragma once

VKTL_EXPORT_ namespace vktl::detail {

	template<typename N>
	struct c<execution, N> : N {
		using base = N;

		constexpr c(execution const& info, auto&&...others)
			: base{ forward_(others)... } {
		}

	protected:
		vector<VK_ VkQueue> queues_;
	};

}