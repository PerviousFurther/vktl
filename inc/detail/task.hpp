#pragma once

// Interface style: a task composes a user callable onto an execution object
// and exposes refresh/record behavior through that callable.
// Implementation: the callable is stored by value and receives a lightweight
// state facade when work is refreshed or submitted.

VKTL_EXPORT_ namespace vktl::detail {

	template<typename Fn, typename N>
	struct m<task<Fn>, N> : N {
		using base = N;
		template<similiar_to<task<Fn>> F>
		constexpr m(F&& task_info, auto&&...infos)
			: base{ forward_(infos)... }
			, fn_{ forward_(task_info).func } {
		}

		void refresh() { base::init(); }

	private:
		Fn fn_;
	};

}
