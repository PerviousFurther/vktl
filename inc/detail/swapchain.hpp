#pragma once

#if !defined(VKTL_NO_WINDOW)

VKTL_EXPORT_ namespace vktl::detail {

	template<typename N>
	struct m<swapchain, N> : N {
		constexpr m(swapchain const& swapchain, auto&&...others)
			: N{ forward_(others)... }
		{

		}


	};

}

#endif
