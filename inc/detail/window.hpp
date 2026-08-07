#pragma once

#if !defined(VKTL_NO_WINDOW)
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
#  endif

VKTL_EXPORT_ namespace vktl::detail {

}

#endif