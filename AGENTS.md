# AGENTS.md

## Overview
- **Project**: VKTL (Vulkan Template Library) - the most powerful vulkan header only library in the world.
- **Stack**: C++, Vulkan.

## Commands
- **Configure**: `cmd /d /s /c ""D:\Visual_Studio\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake -S . -B out\build\x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug"`
- **Build**: `cmd /d /s /c ""D:\Visual_Studio\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build out\build\x64-Debug --parallel"`
- **Test**: Launch `out\build\x64-Debug\example\hello_world\hello_world.exe` from its containing directory; close the window to finish.
- **Lint**: No formatter or lint target is currently configured.

## Working Rules
1. **Targeted Changes**: Touch only code relevant to the assigned task. Avoid broad, unsolicited refactoring.
2. **Code Standards**: Follow existing conventions. Maintain strict typing and modular structure.
3. **Protected Core**: Do not modify `objects.hpp`. If a component cannot be implemented without changing it, ask the user instead.
4. **Docs & Tests**: Update inline comments, docs, and tests when modifying function signatures or core logic.
5. **Concise Outputs**: Keep explanations concise and focus on clear, executable code.

## Repository Structure
- `/inc` - Header-only library implementation
- `/example` - Example programs and runtime acceptance coverage
- `/external` - External dependencies
- `/old`, `/old_inc` - Historical code; do not use as the primary implementation model unless the user asks

## Component Architecture

### Components and Reusable Bases
- Implement each component as a focused `m<Tag, Next>` specialization. The same component may store Vulkan creation parameters, own the live handle, and implement behavior; do not split components into "descriptor" and "handle owner" categories.
- Use the tags exposed by `public.hpp`. Check that header for the available tag names before introducing or specializing a component.
- Most tags are pure-data structs. Declare stateless tags in the singleton-like form `struct xxx_ {} xxx {};`.
- Optional or not-yet-specialized tags may remain transparent composition layers. Once a tag owns state or behavior, give it a focused `m<>` specialization instead of expanding a monolithic dispatcher.
- A component constructor normally forwards the template parameter pack received by its constructor to `N` and initializes its own state. Keep it `constexpr` where practical.
- Reusable utility bases follow the form `basic_xxx<N>`, where the derived component passes its own class-template parameter `N` to the utility base.

### Object Relationships
- Use `parent_of<tag>(this)` to access the associated parent `objects` instance that contains the requested tag. For example, `parent_of<device>(this)` accesses the parent object containing the device component.
- Use `handle_of<tag>(this)` to obtain the Vulkan handle exposed by that associated component. For example, `handle_of<device>(this)` obtains its `VkDevice`.
- Prefer `parent_of<tag>(this)` and `handle_of<tag>(this)` when a component needs an associated parent object or Vulkan handle. Do not duplicate parent objects or add virtual inheritance.
- `from<Ts...>` must copy its tuple of parent pointers. A default-initialized parent tuple compiles but crashes when initialization is forwarded.
- A swapchain is one kind of frame-host and stores its own independent `frame_count()` and `frame_index()`.
- A handle associated with a frame host should normally inherit from `frame_related_handle<N, trait<tag>>`, using the handle's corresponding tag. Fully specialize `trait<tag>` for that handle component.

### Create-info and Relocation
- Store a component's Vulkan create-info directly in the component with `protected` access. A primary object such as a buffer or image normally names its create-info `info`.
- An extension component owns its extension create-info and prepends it to the primary object's `pNext` chain while preserving the previous head: `this->create_info.pNext = ::std::exchange(N::info.pNext, &this->create_info);`.
- Treat every `objects` instance as non-trivially relocatable. A component that stores Vulkan create-info must define `constexpr void relocate()` and repopulate fields that depend on the object's current storage or address.

## Handle Lifecycle
- Objects are lazily initialized. Handle presence is the authoritative initialized/uninitialized state.
- Every handle-owning `m<tag, N>` must expose `init()` and `reset()` callable with no arguments. Defaulted `init` parameters are allowed when a derived layer must influence base initialization.
- `init()` must call `N::init()` **before acquiring the component lock**. Then acquire the lock, re-check the handle, and create it only if still empty. This avoids entering the base chain while holding a potentially non-recursive lock and keeps concurrent initialization idempotent.
- `reset()` resets only the current component and must not call `N::reset()`. Acquire the component lock before checking or changing the handle, and do nothing when it is already empty.
- Use the provided Vulkan handle wrappers. `copyable_if_null` rejects copying a live handle; `reset_if_copy` retains copied creation parameters while clearing the copied live handle. Prefer `reset_if_copy` when copies must not share a live handle.
- Handle wrappers overload address-of for Vulkan output parameters. Use `std::addressof` for internal identity checks, and return the wrapper's underlying `value` explicitly when a real reference is required.

## Thread Safety
- Components must remain agnostic to the object's selected thread semantics and may rely on the composition-provided lock interface; without a real lock layer, the interface is a compatible no-op lock.
- Use `auto _ = locker_of(object)` before any state access that may race. `locker_of(this)` is the common case, while `locker_of(pobj)` obtains an already-acquired RAII lock for another object. `lock_of(object)` only obtains the underlying lock object; it does not lock it.
- A handle accessor that must preserve synchronization beyond the call should return `locked<handle_type>{handle_wrapper, lock_of(this)}` rather than expose the raw handle without its lock.

## Type Erasure
- Use the handwritten tables under `vktl::vptr` when a public header requires another `objects` instance to be passed in and stored without exposing or fixing that object's concrete type.
- Do not introduce a new type-erasure mechanism or broaden an existing `vktl::vptr` contract without confirming the design with the user. If implementation work reveals a type-erasure requirement not covered here, ask the user before proceeding.

## Errors and Runtime Notes
- Report Vulkan/runtime failures with `throw error{error_code, error_msg}`, where `error_code` is an `int` and `error_msg` is a `const char*`. Use `assert` only for programming errors and violated internal invariants.
- The Vulkan SDK's `Vulkan::glslang` target does not bring SPIRV-Tools into the Windows link automatically. Link matching debug/release `SPIRV-Tools-opt` and `SPIRV-Tools` libraries explicitly.
- The current example intentionally has its draw call commented out. Runtime acceptance means successful Vulkan object initialization and a responsive window/message loop, not rendered triangle pixels.
