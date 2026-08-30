#pragma once

#if defined(VKTL_EXPORT_MODULE)
#define VKTL_EXPORT_ export
#else
#define VKTL_EXPORT_
#endif

#if !defined(VKTL_NO_STD)
#define VKTL_HAVE_STD_ 1
#else
#define VKTL_HAVE_STD_ 0
#endif

#define VKTL_VERSION uint32_t(0)

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(no_unique_address)
#define VKTL_NO_UNIQUE_ADDRESS [[no_unique_address, msvc::no_unique_address]]
#endif
#if __has_cpp_attribute(nodiscard)
#define VKTL_NODISCARD [[nodiscard]]
#endif
#if __has_cpp_attribute(likely)
#define VKTL_LIKELY [[likely]]
#endif
#if __has_cpp_attribute(unlikely)
#define VKTL_UNLIKELY [[unlikely]]
#endif
#if __has_cpp_attribute(maybe_unused)
#define VKTL_MAYBE_UNUSED [[maybe_unused]]
#endif
#if __has_cpp_attribute(noreturn)
#define VKTL_NORETURN [[noreturn]]
#endif
#endif

// #define VKTL_LAMBDA_INLINE_
// #define VKTL_STATIC_OPERATOR_

#if !defined(VKTL_NO_UNIQUE_ADDRESS)
#define VKTL_NO_UNIQUE_ADDRESS
#endif
#if !defined(VKTL_NODISCARD)
#define VKTL_NODISCARD
#endif
#if !defined(VKTL_UNLIKELY)
#define VKTL_UNLIKELY
#endif
#if !defined(VKTL_LIKELY)
#define VKTL_LIKELY
#endif
#if !defined(VKTL_MAYBE_UNUSED)
#define VKTL_MAYBE_UNUSED
#endif
#if !defined(VKTL_NORETURN)
#define VKTL_NORETURN
#endif

#if VKTL_HAVE_STD_
#include <concepts>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
// for align_val_t
#include <new>
#endif

#if !defined(assert) && (VKTL_HAVE_STD_ && !defined(VKTL_NO_ASSERT))
#include <assert.h>
#endif

#if defined(assert)
#define VKTL_ASSERT(...) assert((__VA_ARGS__))
#else
#if defined(_MSC_VER)
#define VKTL_ASSERT(x) __assume(x)
#elif defined(__clang__)
#define VKTL_ASSERT(x) __builtin_assume(x)
#elif defined(__GNU__)
#define VKTL_ASSERT(x) __attribute__((assume(x)))
#else
#define VKTL_ASSERT(x)
#endif
#endif

#include "detail/public.hpp"

#include "detail/math.hpp"

//--------------------------DETAIL SCOPE---------------------------

#if !defined(VKTL_NO_DETAIL)

#if VKTL_HAVE_STD_
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <deque>
#include <exception>
#include <initializer_list>
#include <list>
#include <map>
#include <mutex>
#include <ranges>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#endif

#if !defined(VK_NAMESPACE)
#define VK_NAMESPACE
#endif

#define VK_ VK_NAMESPACE::

namespace VK_NAMESPACE {
#include <vulkan/vulkan.h>
}

#if defined(VKTL_NO_WINDOW) || !defined(VK_KHR_surface)
#define VKTL_HAVE_WINDOW 0
#else
#define VKTL_HAVE_WINDOW 1
#endif

#define forward_(x) ::std::forward<decltype(x)>(x)

// Fucking clangd reorder includes if they are adjacent.

#include "detail/utils.hpp"

#include "detail/container.hpp"

#include "detail/objects.hpp"

#include "detail/common.hpp"

#include "detail/frame_related.hpp"

VKTL_EXPORT_ namespace vktl {
  using detail::cross_thread_shared;
  using detail::lockable;
  using detail::object;
  using detail::shared;
  using detail::operator|;
}

#include "detail/instance.hpp"

#if VKTL_HAVE_WINDOW
#include "detail/window.hpp"
#endif

#include "detail/device.hpp"

#include "detail/pass.hpp"
#include "detail/pipe.hpp"

#if VKTL_HAVE_WINDOW
#include "detail/swapchain.hpp"
#endif

#include "detail/resource.hpp"

#include "detail/buffer.hpp"
#include "detail/image.hpp"
#include "detail/sampler.hpp"

#include "detail/descriptor.hpp"

#include "detail/bind_set.hpp"

#include "detail/sync.hpp"

#include "detail/commands.hpp"

#include "detail/execution.hpp"
#include "detail/task.hpp"

#if !defined(VKTL_NO_COMPILER)
#  include "detail/compiler.hpp"
#endif

#endif

// #undef VK_
#undef forward_
#undef VKTL_NO_UNIQUE_ADDRESS
#undef VKTL_NODISCARD
#undef VKTL_UNLIKELY
#undef VKTL_LIKELY
#undef VKTL_MAYBE_UNUSED
#undef VKTL_INS_FN_
#undef VKTL_ASSERT