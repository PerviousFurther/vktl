#pragma once



#if defined(VKTL_EXPORT_MODULE)
#	define VKTL_EXPORT_ export 
#else
#	define VKTL_EXPORT_
#endif

#if !defined(VKTL_NO_STD)
#   define VKTL_HAVE_STD_ 1
#else
#   define VKTL_HAVE_STD_ 0
#endif

#define VKTL_VERSION uint32_t('pre0')

#if defined(__has_cpp_attribute)
# if __has_cpp_attribute(no_unique_address)
#  define VKTL_NO_UNIQUE_ADDRESS [[no_unique_address, msvc::no_unique_address]]
# endif
# if __has_cpp_attribute(nodiscard)
#  define VKTL_NODISCARD [[nodiscard]]
# endif
# if __has_cpp_attribute(likely)
#  define VKTL_LIKELY [[likely]]
# endif
# if __has_cpp_attribute(unlikely)
#  define VKTL_UNLIKELY [[unlikely]]
# endif
# if __has_cpp_attribute(maybe_unused)
#  define VKTL_MAYBE_UNUSED [[maybe_unused]]
# endif
#endif

#if !defined(VKTL_NO_UNIQUE_ADDRESS)
# define VKTL_NO_UNIQUE_ADDRESS
#endif
#if !defined(VKTL_NODISCARD)
# define VKTL_NODISCARD
#endif
#if !defined(VKTL_UNLIKELY)
# define VKTL_UNLIKELY
#endif
#if !defined(VKTL_LIKELY)
# define VKTL_LIKELY
#endif
#if !defined(VKTL_MAYBE_UNUSED)
# define VKTL_MAYBE_UNUSED
#endif

#if VKTL_HAVE_STD_
# include <utility>
# include <span>
# include <string_view>
# include <tuple>
# include <type_traits>
# include <concepts>
#endif

#if !defined(assert)
# if VKTL_HAVE_STD_ && !defined(VKTL_NO_ASSERT)
#  include <assert.h>
# else
#  if defined(_MSC_VER)
#   define assert(x) __assume(x)
#  elif defined(__clang__)
#   define assert(x) __builtin_assume(x)
#  elif defined(__GNU__)
#   define assert(x) __attribute__((assume(x)))
#  else
#   define assert(x)
#  endif
# endif
#endif

#include "detail/public.hpp"
#include "detail/math.hpp"

//--------------------------DETAIL SCOPE---------------------------

#if !defined(VKTL_NO_DETAIL)

#if VKTL_HAVE_STD_
# include <string>
# include <vector>
# include <set>
# include <map>
# include <unordered_map>
# include <algorithm>
# include <atomic>
# include <mutex>
# include <array>
# include <ranges>
# include <bit>
# include <initializer_list>
#endif

#if !defined(VK_NAMESPACE)
#define VK_NAMESPACE 
#endif

#define VK_ VK_NAMESPACE::


namespace VK_NAMESPACE {
#include <vulkan/vulkan.h>
}

#define forward_(x) ::std::forward<decltype(x)>(x)

#include "detail/meta.hpp"
#include "detail/utils.hpp"
#include "detail/container.hpp"
#include "detail/objects.hpp"


#include "detail/common.hpp"

VKTL_EXPORT_ namespace vktl {
	using detail::object;
	using detail::shared;
	using detail::cross_thread_shared;
	using detail::lockable;
	using detail::operator|;
}

#include "detail/instance.hpp"

#include "detail/window.hpp"
#include "detail/device.hpp"
#include "detail/compiler.hpp"
#include "detail/pass.hpp"
#include "detail/pipe.hpp"
#include "detail/fence.hpp"
#include "detail/event.hpp"
#include "detail/semaphore.hpp"

#include "detail/swapchain.hpp"
#include "detail/resource.hpp"
#include "detail/buffer.hpp"
#include "detail/image.hpp"
#include "detail/descriptor.hpp"
#include "detail/descriptor_set.hpp"
#include "detail/sampler.hpp"
#include "detail/execution.hpp"
#include "detail/task.hpp"

#endif

#undef VK_ 
#undef forward_
#undef VKTL_NO_UNIQUE_ADDRESS
#undef VKTL_NODISCARD
#undef VKTL_UNLIKELY
#undef VKTL_LIKELY
#undef VKTL_MAYBE_UNUSED
#undef VKTL_INS_FN_
