#pragma once

#include "vktl_detail/public.hpp"

#if !defined(VKTL_PUBLIC_ONLY)
#	include "vktl_detail/private.hpp"
#endif

#undef ins_fn_
#undef dv_fn_
#undef forward_
#undef VK_
#undef VKTL_EXPORT_ 
#undef VKTL_NO_UNIQUE_ADDRESS