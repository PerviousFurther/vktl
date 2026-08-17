#pragma once

#if !defined(VKTL_NO_WINDOW)

VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct basic_window : N {
		constexpr basic_window(auto&&...infos)
			: N{ forward_(infos)... } 
		{}
		
		VK_ VkSurfaceKHR handle() const noexcept { return handle_.value; }

	protected:
		copyable_if_null<VK_ VkSurfaceKHR> handle_{ VK_NULL_HANDLE };
	};
}

#  if defined(_WIN32) 
#pragma region NOBODY LIKE WIN32
#	define WIN32_LEAN_AND_MEAN
#	define NOGDICAPMASKS
#	define NOVIRTUALKEYCODES
//#	define NOWINMESSAGES
//#	define NOWINSTYLES
#	define NOSYSMETRICS
//#	define NOMENUS
//#	define NOICONS
//#	define NOKEYSTATES
#	define NOSYSCOMMANDS
#	define NORASTEROPS
//#	define NOSHOWWINDOW
#	define OEMRESOURCE
//#	define NOATOM
#	define NOCLIPBOARD
#	define NOCOLOR
#	define NOCTLMGR
#	define NODRAWTEXT
#	define NOGDI
//#	define NOKERNEL
//#	define NOUSER
#	define NONLS
#	define NOMB
#	define NOMEMMGR
#	define NOMETAFILE
#	define NOMINMAX
//#	define NOMSG
#	define NOOPENFILE
#	define NOSCROLL
#	define NOSERVICE
#	define NOSOUND
#	define NOTEXTMETRIC
//#	define NOWH // window hook.
//#	define NOWINOFFSETS
#	define NOCOMM
#	define NOKANJI
#	define NOHELP
#	define NOPROFILER
#	define NODEFERWINDOWPOS
#	define NOMCX
namespace VK_NAMESPACE {
#	include <Windows.h>
#	include <vulkan/vulkan_win32.h>
}
#	undef WIN32_LEAN_AND_MEAN
#	undef NOGDICAPMASKS
#	undef NOVIRTUALKEYCODES
#	undef NOWINMESSAGES
#	undef NOWINSTYLES
#	undef NOSYSMETRICS
#	undef NOMENUS
#	undef NOICONS
#	undef NOKEYSTATES
#	undef NOSYSCOMMANDS
#	undef NORASTEROPS
#	undef NOSHOWWINDOW
#	undef OEMRESOURCE
#	undef NOATOM
#	undef NOCLIPBOARD
#	undef NOCOLOR
#	undef NOCTLMGR
#	undef NODRAWTEXT
#	undef NOGDI
#	undef NOKERNEL
#	undef NOUSER
#	undef NONLS
#	undef NOMB
#	undef NOMEMMGR
#	undef NOMETAFILE
#	undef NOMINMAX
#	undef NOMSG
#	undef NOOPENFILE
#	undef NOSCROLL
#	undef NOSERVICE
#	undef NOSOUND
#	undef NOTEXTMETRIC
#	undef NOWH
#	undef NOWINOFFSETS
#	undef NOCOMM
#	undef NOKANJI
#	undef NOHELP
#	undef NOPROFILER
#	undef NODEFERWINDOWPOS
#	undef NOMCX
#pragma endregion

VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct m<window, N> : basic_window<N> {
		using base = basic_window<N>;
		using native_window_type = VK_ HWND;

		constexpr m(window win, auto&&...others)
			: base{forward_(others)...}
			, info{ 
				.sType = VK_ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
				.hinstance = VK_ GetModuleHandle(nullptr),
				.hwnd = static_cast<VK_ HWND>(win.handle),
			} {
			parent_of<instance>(this)->append_extensions(VK_KHR_SURFACE_EXTENSION_NAME);
			parent_of<instance>(this)->append_extensions(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
		}

		~m() { reset(); }

		void init() {
			N::init();
			if (this->handle_) {
				assert(info.hwnd && info.hinstance);
				VK_ vkCreateWin32SurfaceKHR(handle_of<instance>(this),
					&info.surface, N::allocator(), &surface_.value)
					| popup("[WINDOW] Create surface failure.");
			}

		}

		void reset() {
			if (this->handle_) {
				VK_ vkDestroySurfaceKHR(handle_of<instance>(this), 
					::std::exchange(this->handle_, nullptr), N::allocator());
			}
		}

		uint32_t width() const noexcept {
			assert(VK_ IsWindow(info.hwnd)); // test.
			VK_ RECT rt; VK_ GetClientRect(info.hwnd, &rt);
			return uint32_t(rt.right - rt.left);
		}

		uint32_t height() const noexcept {
			assert(VK_ IsWindow(info.hwnd));
			VK_ RECT rt; VK_ GetClientRect(info.hwnd, &rt);
			return uint32_t(rt.bottom - rt.top);
		}

		auto native_window() const noexcept { return info.hwnd; }

	protected:
		VK_ VkWin32SurfaceCreateInfoKHR info;
	};
}
#  elif defined(__ANDROID__)

namespace VK_NAMESPACE {
#	include <android/native_window.h>
#	include <vulkan/vulkan_android.h>
}

VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct m<window, N> : basic_window<N> {
		using base = basic_window<N>;
		using native_window_type = VK_ ANativeWindow*;

		constexpr m(window win, auto&&...others)
			: base{ forward_(others)... }
			, info{
				.sType = VK_ VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
				.window = static_cast<VK_ ANativeWindow*>(win.handle),
			} {
			parent_of<instance>(this)->append_extensions(VK_KHR_SURFACE_EXTENSION_NAME);
			parent_of<instance>(this)->append_extensions(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
		}

		~m() { reset(); }

		void init() {
			N::init();
			if (this->handle_) {
				assert(info.window);
				VK_ vkCreateAndroidSurfaceKHR(handle_of<instance>(this),
					&info, N::allocator(), &this->handle_.value)
					| popup("[WINDOW] Create surface failure.");
			}
		}

		void reset() {
			if (this->handle_) {
				VK_ vkDestroySurfaceKHR(handle_of<instance>(this),
					::std::exchange(this->handle_, nullptr), N::allocator());
			}
		}

		uint32_t width() const noexcept {
			return info.window ? static_cast<uint32_t>(VK_ ANativeWindow_getWidth(info.window)) : 0;
		}

		uint32_t height() const noexcept {
			return info.window ? static_cast<uint32_t>(VK_ ANativeWindow_getHeight(info.window)) : 0;
		}

		auto native_window() const noexcept { return info.window; }

	protected:
		VK_ VkAndroidSurfaceCreateInfoKHR info{};
	};
}

#elif defined(__linux__)

namespace VK_NAMESPACE {
#  if defined(VKTL_USE_WAYLAND)
#    include <wayland-client.h>
#    include <vulkan/vulkan_wayland.h>
#  else
#    include <xcb/xcb.h>
#    include <vulkan/vulkan_xcb.h>
#  endif
}

VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct m<window, N> : basic_window<N> {
		using base = basic_window<N>;

#  if defined(VKTL_USE_WAYLAND)
		using native_window_type = VK_ wl_surface*;
		using create_info_type = VK_ VkWaylandSurfaceCreateInfoKHR;
#  else
		using native_window_type = VK_ xcb_window_t;
		using create_info_type = VK_ VkXcbSurfaceCreateInfoKHR;
#  endif

		constexpr m(window win, auto&&...others)
			: base{ forward_(others)... }
			, info{ make_create_info(win) } {
			parent_of<instance>(this)->append_extensions(VK_KHR_SURFACE_EXTENSION_NAME);
#  if defined(VKTL_USE_WAYLAND)
			parent_of<instance>(this)->append_extensions(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#  else
			parent_of<instance>(this)->append_extensions(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#  endif
		}

		~m() { reset(); }

		void init() {
			N::init();
			if (this->handle_) {
				create_surface();
			}
		}

		void reset() {
			if (this->handle_) {
				VK_ vkDestroySurfaceKHR(handle_of<instance>(this),
					::std::exchange(this->handle_, nullptr), N::allocator());
			}
		}

		uint32_t width() const noexcept {
#  if defined(VKTL_USE_WAYLAND)
			return win_width;
#  else
			return query_geometry().width;
#  endif
		}

		uint32_t height() const noexcept {
#  if defined(VKTL_USE_WAYLAND)
			return win_height;
#  else
			return query_geometry().height;
#  endif
		}

		auto native_window() const noexcept {
#  if defined(VKTL_USE_WAYLAND)
			return info.surface;
#  else
			return info.window;
#  endif
		}

	private:
		static constexpr create_info_type make_create_info(window win) noexcept {
#  if defined(VKTL_USE_WAYLAND)
			return {
				.sType = VK_ VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
				.display = static_cast<VK_ wl_display*>(win.display),
				.surface = static_cast<VK_ wl_surface*>(win.handle),
			};
#  else
			return {
				.sType = VK_ VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
				.connection = static_cast<VK_ xcb_connection_t*>(win.connection),
				.window = static_cast<VK_ xcb_window_t>(reinterpret_cast<uintptr_t>(win.handle)),
			};
#  endif
		}

		void create_surface() {
#  if defined(VKTL_USE_WAYLAND)
			assert(info.display && info.surface);
			VK_ vkCreateWaylandSurfaceKHR(handle_of<instance>(this),
				&info, N::allocator(), &this->handle_.value)
				| popup("[WINDOW] Create surface failure.");
#  else
			assert(info.connection && info.window);
			VK_ vkCreateXcbSurfaceKHR(handle_of<instance>(this),
				&info, N::allocator(), &this->handle_.value)
				| popup("[WINDOW] Create surface failure.");
#  endif
		}

#  if !defined(VKTL_USE_WAYLAND)
		struct size_2d { uint32_t width{ 0 }; uint32_t height{ 0 }; };

		size_2d query_geometry() const noexcept {
			if (!info.connection || !info.window) return {};
			auto cookie = VK_ xcb_get_geometry(info.connection, info.window);
			auto reply = VK_ xcb_get_geometry_reply(info.connection, cookie, nullptr);
			size_2d size{ reply ? reply->width : 0u, reply ? reply->height : 0u };
			free(reply);
			return size;
		}
#  endif

	protected:
		create_info_type info{};
#  if defined(VKTL_USE_WAYLAND)
		uint32_t win_width{ 0 };
		uint32_t win_height{ 0 };
#  endif
	};
}
#endif
#  elif defined(__APPLE__)

namespace VK_NAMESPACE {
#	include <vulkan/vulkan_metal.h>
}

VKTL_EXPORT_ namespace vktl::detail {
	template<typename N>
	struct m<window, N> : basic_window<N> {
		using base = basic_window<N>;
		using native_window_type = VK_ CAMetalLayer const*; // CAMetalLayer*

		constexpr m(window win, auto&&...others)
			: base{ forward_(others)... }
			, info{
				.sType = VK_ VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
				.pLayer = static_cast<VK_ CAMetalLayer const*>(win.handle),
			} {
			parent_of<instance>(this)->append_extensions(VK_KHR_SURFACE_EXTENSION_NAME);
			parent_of<instance>(this)->append_extensions(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
		}

		~m() { reset(); }

		void init() {
			N::init();
			if (this->handle_) {
				assert(info.pLayer);
				VK_ vkCreateMetalSurfaceEXT(handle_of<instance>(this),
					&info, N::allocator(), &this->handle_.value)
					| popup("[WINDOW] Create surface failure.");
			}
		}

		void reset() {
			if (this->handle_) {
				VK_ vkDestroySurfaceKHR(handle_of<instance>(this),
					::std::exchange(this->handle_, nullptr), N::allocator());
			}
		}

		uint32_t width() const noexcept { return win_width; }
		uint32_t height() const noexcept { return win_height; }

		auto native_window() const noexcept { return info.pLayer; }

	protected:
		VK_ VkMetalSurfaceCreateInfoEXT info{};
		uint32_t win_width{ 0 };
		uint32_t win_height{ 0 };
	};
}

#  endif


#endif