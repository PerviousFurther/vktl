#pragma once

VKTL_EXPORT_ namespace vktl::detail {

	template<typename Fn, typename N>
	struct c<task<Fn>, N> : N {
		using base = N;
		template<similiar_to<task<Fn>> F>
		constexpr c(F&&, auto&&...infos)
			: base{ forward_(infos)... } {

		}

	private:
		Fn fn_;
	};

}