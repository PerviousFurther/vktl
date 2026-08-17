#pragma once

// Interface style: execution objects compose queue declarations and submit
// work collected from child tasks.
// Implementation: queue handles and task state are retained by the execution
// layer and initialized only after its device parent is ready.

VKTL_EXPORT_ namespace vktl::detail {

	template<typename N>
	struct m<execution, N> : N {
		using base = N;

		constexpr m(execution const& info, auto&&...others)
			: base{ forward_(others)... } {
		}

		void submit() noexcept {}

	protected:
		vector<VK_ VkQueue> queues_;
	};

}
