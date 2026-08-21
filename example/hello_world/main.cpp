 #include "vktl.hpp"

void* create_win();
bool is_running(void* window);

#include <typeinfo>
#include <iostream>

int main() {
	using namespace vktl;
	auto hwnd = create_win();

	object ins
		= instance{ .name = "test", .version = 0 }
	| instance_extensions::validation_layer;

	object win
		= ins
		| window{ .handle = hwnd };

	object cpl
		= ins
		| compiler
		| compiler_extensions::glsl;

	object dev
		= ins
		| device{ .index = 0u }
	| device_extensions::queue_family{ .family = 0u, .count = 1u };

	object alc
		= dev
		| memory_allocator
		| memory_allocator_extensions::budget
		| memory_allocator_extensions::dedicated
		| memory_allocator_extensions::buddy
		| memory_allocator_extensions::allow_buffer
		| memory_allocator_extensions::allow_image;

	object dlc
		= dev
		| descriptor_allocator
		| descriptor_allocator_extensions::allow_set;

	// static buffer.
	object vertex
		= dev
		| buffer{ .size = sizeof(float) * 32 }
		| buffer_extensions::mappable; // bigger buffer is better.
	vertex.upload({
		0.0f, -0.5f, 1.0f, 0.0f, 0.0f,
		0.5f, 0.5f, 0.0f,  1.0f, 0.0f,
		-0.5f, 0.5f, 0.0f, 0.0f, 1.0f
	});

	object swc
		= dev | win // swapchain usually have two parents.
		| swapchain{ .min_frame_count = 2u };
	auto& swc_image = swc;

	// swapchain actually is frame related object, thus,
	// object `inherit` from

	// dynamic buffer.
	object uniform
		= swc
		| buffer{ .size = sizeof(float) * 16 }
		| buffer_extensions::mappable;
	uniform.upload({
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	});

	// append resource in allocator so that it can automatically 
	// bind when allocate resoruces.
	alc.append(vertex);
	alc.append(uniform);

	object set0
		= swc
		| bind_set
		| descriptor_set_extensions::allow_buffer
		| descriptor_set_extensions::allow_image; // bind set is a glue between pass and resources.
	                // bind set need to connect pass first to get resources usages.
	                // and then bind view on it to take effort.
	object pass_triangle
		= dev 
		| pass | pass_extensions::render_pass
		| pipe | pipe_extensions::subpass{ 0u }
		| vertex_shader{ cpl.append("hello_world.vert", VK_NAMESPACE::VK_SHADER_STAGE_VERTEX_BIT) } // TODO: maybe make VK_SHADER_STAGE_VERTEX_BIT as independent flags. 
		| uniform_buffer{ .index = 0u }
		| resource_usage_extensions::bind_on_set{ .set = 0u, .binding = 0u }
		| fragment_shader{ cpl.append("hello_world.frag", VK_NAMESPACE::VK_SHADER_STAGE_FRAGMENT_BIT) }
		| attachment{ .index = 0u, }
		| pass_extensions::format_color{
			.order = image_bits_order::bgra,
			.rbits = 8u,
			.gbits = 8u,
			.bbits = 8u,
			.abits = 8u,
			.rtype = image_bits_type::unorm,
			.gtype = image_bits_type::unorm,
			.btype = image_bits_type::unorm,
			.atype = image_bits_type::unorm,
		}; // image index is independent.

	// pass will not bind bind set.
	// fill operation: fill set layout bindings and other jobs.
	pass_triangle.fill(set0);
	dlc.append(set0);

	// BE NOTICED: set0 is same frame related with uniform view and 
	object uniform_view 
		= uniform
		| buffer_view;

	set0.bind(0, uniform_view);
	set0.bind(0, swc_image);

	object exec 
		= dev
		| execution{ .thread_count = 1u }
		| queue{ .family = 0u, .index = 0u }
		| queue_extensions::graphics
		| queue_extensions::present;

	object draw_triangle
		= exec
		| task{ 
		// this is task object's descriptor.
		// since some state will changed, the function will be invoked by multiple time.
		[&](auto state) {	
			auto worker = state.worker(0u);
			auto commands = worker.commands(queue_duty::graphics);
			// Drawing remains intentionally disabled in this acceptance example.
			auto submission = state.submit(commands);
			submission.present(swc);
		} };

	// refresh will trigger all binded object to initialize.
	draw_triangle.refresh();
	while (is_running(hwnd)) {
		// we can do upload here. since it is simple showcase, this will skipped.
		// uniform.upload({
		// 	1.0f, 0.0f, 0.0f, 0.0f,
		// 	0.0f, 1.0f, 0.0f, 0.0f,
		// 	0.0f, 0.0f, 1.0f, 0.0f,
		// 	0.0f, 0.0f, 0.0f, 1.0f,
		// });
		
		draw_triangle.submit();
		exec.submit(); 
	}
}
