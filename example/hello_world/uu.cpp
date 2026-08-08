#include "vktl.hpp"

void* create_win();
bool is_running(void* window);

struct u {
	double first;
	int second;
};
// ptr is from (some object of u).second
double* foo(int* ptr) {
	return reinterpret_cast<double*>(reinterpret_cast<char*>(ptr) - offsetof(u, second) + offsetof(u, first));
}



int main() {


	using namespace vktl;
	auto hwnd = create_win();

	object ins
		= instance{ .name = "test", .version = 0 } 
		| instance_extensions::validation_layer;
	object dev
		= ins
		| device{ .index = 0u };
	object buf
		= dev
		| buffer{ .size = 100 };
	
	// auto device_context = create(
	// 	instance{} & instance_extensions::validation_layer
	// 	| device{ 0u }
	// 	| queue_family{ 0u, 1u }
	// );
	// 
	// auto swapchain0 = create(device_context
	// 	, window{ .handle = hwnd }
	// 	| swapchain{ .min_frame_count = 3u }
	// );
	// 
	// auto engine = create(device_context // ref device context as parent.
	// 	, execution // multi thread.
	// 	| queue{ queue_duty::graphics, 0u, 0u }
	//  | queue
	// );
	// 
	// auto pass0 = create(pass & pass_extensions::render_pass
	// 	, pipe
	// 	| vertex_input{}
	// 	| default_vertex_shader{}
	// 	| buffer{ .index = 0u, .usage = resource_usage::uniform }
	// 	& buffer_view_extensions::binding{ .set = 0u, .binding = 0u }
	// 	| pixel_shader{}
	// 	| buffer{ .index = 0u, .usage = resource_usage::uniform }
	// 	& buffer_view_extensions::binding{ .set = 0u, .binding = 0u }
	// 	| blend_state{}
	// );
	// 
	// auto task0 = create(
	// 	engine
	// 	, task{ [&](auto state) {
	// 		if constexpr (state.setup) {
	// 			// examples.
	// 			auto pass0 = state.add(pass & pass_extensions::graphics
	// 				, pipe & pipe_extensions::subpass{ 0u }
	// 				| vertex_input {}
	// 				| default_vertex_shader {}
	// 				| buffer{ .index = 0u, .usage = resource_usage::uniform }
	// 					& buffer_extensions::binding{ .set = 0u, .binding = 0u }
	//					& buffer_extensions::array{ .count = 2u }
	// 				| pixel_shader
	// 				| buffer{ .usage = resource_usage::uniform }
	// 					& buffer_extensions::binding{ .set = 0u, .binding = 0u }
	// 				| blend_state { /* implmentation defined. */ }
	// 				, pipe & pipe_extensions::subpass{ 1u }
	// 				| vertex_input { /* implmentation defined. */ }
	// 				| default_vertex_shader{ /* implmentation defined. */ }
	// 				| pixel_shader{ /* implmentation defined */ }
	// 			);
	// 
	// 			auto pass1 = state.add(pass & pass_extensions::compute
	// 				, pipe
	// 				| compute_shader{ /*implmentation defined.*/ }
	// 				| buffer { .index = 0u, .offset = 0u, .size = 1u, .usage = resource_usage::structure }
	//					& buffer_extensions::binding{ .set = 0u, .binding = 0u }
	// 				, pipe & pipe_extensions::inheritance{ .index = 0u }
	// 				| compute_shader{ /*implmentation defined.*/ }
	// 			);
	// 
	// 			auto pass2 = state.add(pass & pass_extensions::graphics
	// 				, pipe
	// 				| image{ .index = 0u, }
	// 				| vertex_input{ /* implmentation defined. */ }
	// 				| default_vertex_shader{ /*implmentation defined.*/ }
	// 				| buffer{ .usage = resource_usage::uniform }
	// 					& buffer_extensions::binding {.set = 0u, .binding = 0u }
	// 				| pixel_shader{ /* implmentation defined */ }
	// 				| image{ }
	// 					& image_extensions::binding{.set = 0u, .binding = 0u }
	// 				, pipe
	// 				| default_vertex_shader{ /*implmentation defined.*/ }
	// 				| pixel_shader{ /* implmentation defined */ }
	// 			);
	// 
	// 			auto static_memory_allocator 
	//				= state.add(memory_allocator & memory_allocator_extensions::linear);
	// 
	//			auto resources = state.bind(static_memory_allocator, pass0, pass1);
	//			buffer.upload();
	// 			state
	// 				.add(pass0, pass2)
	// 				.add(pass1, pass2);
	// 		
	// 			// end function and it should create all static resource.
	// 		}
	// 	
	// 		// these stage execute in engine's task thread.
	// 		
	// 		// usually call before refreshing.
	// 		else if constexpr (state.initialize) {
	// 			// combine bind points by specified index.
	// 			// this will enable bindpoints[1u]'s resource index after bindpoints[0u]'s  
	// 			// which means current actual bind points is (use buffer scope for example, image are with same mechaism):
	// 			// buffer scope: 
	// 			//  bind index |          resource
	// 			//      0      | buffer0 from bindpoints[0]
	// 			//      1      | buffer1 from bindpoints[0]
	// 			//      2      | buffer0 from bindpoints[1]
	// 			//      3      | buffer1 from bindpoints[1]
	// 			//      4      | buffer2 from bindpoints[1]
	// 			state.bind({ 0u, 1u });
	// 		
	// 			state.copy(buffer_view{  }, image_view{  });
	// 		}
	// 		// refreshing command buffer, (you need to design what else it can do).
	// 		else if constexpr (state.refreshing) {
	// 			state.bind({ 0u, 1u });
	// 		
	// 			auto command0 = state.split();
	// 			auto semaphore0 = command0
	// 				.draw(0u, 0u
	// 					, buffer_view{.index = 0u, }) // pass 0u, pipe 0u, buffer[0] (vertex buffer)
	// 				.draw(0u, 1u
	// 					, { buffer_view{.index = 0u, }, buffer_view{.index = 1u, } }
	// 					, buffer_view{.index = 0u })
	// 				.dispatch(/* omitted. */); // pass 0u, pipe 0u, buffer[0, 1] (vertex buffers), buffer[0] (index buffer)
	// 		
	// 			auto command1 = state.split();
	// 			auto semaphore1 = command1
	// 				.draw(0u, 0u, buffer_view);
	// 		
	// 				/* other call is omitted. */
	// 		}
	// 		// execute from engine when call emit::execute, should not write command buffer, 
	// 		// it designed for per frame submition (or not submit payload) and (you need to design what else it can do).
	// 		else if constexpr (state.execute) {
	// 			state.submit();
	// 		}
	// 	} }
	// );
	// 
	// while (is_running(hwnd)) {
	// 	task0.send(emit::refresh{}); // trigger single task's re-record. 
	// 	engine.send(emit::execute{}); // trigger all task's submition.
	// }


	// auto draw_triangle = create(
	// 	, describe(pass{}
	// 		, pass_extensions::render_pass{})
	// 	// subpass 0
	// 	, describe(pipe{ 0u }
	// 		, extensions::graphics{})
	// 	, describe(image_view{ 0u }
	// 		, image_format::format_color{}
	// 		, image_view_extensions::attachment{ .clear = true })
	// 	// pipe 0
	// 	, describe(vertex_input{})
	// 	, describe(buffer_view{}, buffer_view_extensions::vertex_binding<vktl::math::vec4f>())
	// 	, describe(default_vertex_shader{})
	// 	, describe(pixel_shader{})
	// 	, describe(draw{}
	// 		, draw_extensions::draw_vertex_by_range{ 0u, 1u }
	// 		, draw_extensions::instanced{})
	// );

	// while (is_running(hwnd)) {
	// 	draw_triangle.send(event::record{}, graphics_engine);
	// }
}

// using namespace sync_point_extensions;
// template<>
// struct meta_of<sync_points> {
// 
//     template<typename T>
//     struct info : T {
//         using semaphore_info = transformed<staged<unsynced<unattached<>>>, T>;
// 
//         constexpr void append_event(unsynced<unattached<>> new_event) {
//             receive(events, new_event);
//         }
// 
//         constexpr void append_fence(unsynced<unattached<>> fence) {
//             receive(fences, fence);
//         }
// 
//         // template<::std::convertible_to<>>
//         constexpr void receive(semaphore_info semaphore) requires(requires{ semaphore.stages; }) {
//             auto it = receive(semaphores, semaphore);
//             it->stages |= semaphore.stages;
//         }
// 
//         vector<semaphore_info> semaphores;
//         vector<unsynced<unattached<>>> events;
//         vector<unsynced<unattached<>>> fences;
// 
//     private:
//         static constexpr auto receive(auto& vec, auto val) {
//             auto it = vec.begin();
//             while (it != vec.end() && it->wait != val.wait) { it++; }
//             while (it != vec.end() && it->wait == val.wait && it->index < val.index) { it++; }
//             if (it == vec.end() || it->wait != val.wait || it->index != val.index) {
//                 it = vec.insert(it, ::std::move(val));
//             }
//             return it;
//         }
//     };
// 
//     template<typename T>
//     struct make : T {
//         template<infomation_of<sync_points> F>
//         make(F&& info, auto&&...others) : T{ forward_(others)... } {
//             auto& dv = T::template parent<device>();
// 
//             events_.reserve(info.events.size());
//             for (auto idx : info.events) {
//                 events_.push_back(dv->event(idx));
//             }
// 
//             semaphores_.reserve(info.semaphores.size());
//             for (auto idx : info.semaphores) {
//                 semaphores_.push_back(dv->semaphore(idx));
//             }
// 
//             fences_.reserve(info.fences.size());
//             for (auto idx : info.fences) {
//                 fences_.push_back(dv->fence(idx));
//             }
//         }
// 
//     private:
//         vector<transformed<VK_N_ VkPipelineStageFlags, T>> stages_wait;
//         vector<VK_N_ VkSemaphore> wait_semaphores_;
//         vector<VK_N_ VkSemaphore> emit_semaphores_;
//         
//         vector<VK_N_ VkFence> wait_fences_;
//         vector<VK_N_ VkFence> emit_fences_;
// 
//         vector<VK_N_ VkEvent> emit_events_;
//         vector<VK_N_ VkEvent> wait_events_;
//     };
// };
