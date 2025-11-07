module;
#include <stormkit/gpu/vulkan.hpp>
#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#undef assert
module gui.windowapp;

import log;
import stormkit.core;

import grid;
import geometry;

import engine.model;
import engine.rulenode;
import parser;
import controls;

import stormkit.wsi;
import stormkit.gpu;
// import gui.render;

using namespace stormkit;
using namespace std::string_literals;
using namespace std::chrono_literals;
using clk = std::chrono::high_resolution_clock;

static const auto DEFAULT_PALETTE_FILE = "resources/palette.xml"s;
static const auto DEFAULT_MODEL_FILE   = "models/GoToGradient.xml"s;

static constexpr auto DEFAULT_GRID_EXTENT = std::dims<3>{1u, 59u, 59u};
static constexpr auto DEFAULT_TICKRATE = 60;

static constexpr auto WINDOW_TITLE = "Hroza";
static constexpr auto WINDOW_SIZE  = math::Extent2<u32>{800, 600};
static constexpr auto BUFFERING_COUNT = 2;

struct SubmissionResource {
    gpu::Fence         in_flight;
    gpu::Semaphore     image_available;
    gpu::CommandBuffer render_cmb;
};

struct SwapchainImageResource {
    Ref<const gpu::Image> image;
    gpu::ImageView        view;
    gpu::FrameBuffer      framebuffer;
    gpu::Semaphore        render_finished;
};

auto WindowApp::operator()(std::span<const std::string_view> args) noexcept -> int {
  wsi::parse_args(args);
  
  auto palettefile = DEFAULT_PALETTE_FILE;
  ilog("loading palette");
  auto default_palette = parser::Palette(parser::document(palettefile));

  auto modelarg = std::ranges::find_if(args, [](const auto& arg) static noexcept {
    return std::ranges::cbegin(std::ranges::search(arg, "models/"s)) == std::ranges::cbegin(arg);
  });
  auto modelfile =
    modelarg != std::ranges::end(args) ? std::string{*modelarg} : DEFAULT_MODEL_FILE;

  ilog("loading model");
  auto model = parser::Model(parser::document(modelfile));

  auto extent = DEFAULT_GRID_EXTENT;
  auto grid = TracedGrid{extent, model.symbols[0]};
  if (model.origin) grid[grid.area().center()] = model.symbols[1];

  auto controls = Controls {
    .tickrate = DEFAULT_TICKRATE,
    .onReset = [&grid, &model]{
      reset(model.program);
      grid = TracedGrid{grid.extents, model.symbols[0]};
      if (model.origin) grid[grid.area().center()] = model.symbols[1];
    },
  };

  ilog("start program thread");
  auto program_thread = std::jthread{ [&grid, &model, &controls](std::stop_token stop) mutable noexcept {
    auto last_time = clk::now();
    for (auto _ : model.program(grid)) { // TODO replace with a while loop conditioned on the stop token, 
      if (stop.stop_requested()) break;  //      and put the program tick inside (instead of async generator)

      controls.sleep_missing(last_time);
      controls.maybe_pause();

      last_time = clk::now();
    }

    model.halted = true;
  } };

  ilog("open stormkit window");
  auto window = wsi::Window::open(
    WINDOW_TITLE, WINDOW_SIZE, wsi::WindowFlag::DEFAULT | wsi::WindowFlag::EXTERNAL_CONTEXT
  );

  window.on<wsi::EventType::KEY_DOWN>([&window](u8 /*id*/,
                                                wsi::Key key,
                                                char /*c*/) mutable noexcept {
      if (key == wsi::Key::ESCAPE) window.close();
  });

  ilog("init stormkit vulkan");
  *gpu::initialize_backend().transform_error(monadic::assert("Failed to initialize gpu backend"));

  ilog("create gpu instance and attach surface to window");
  const auto instance = gpu::Instance::create(WINDOW_TITLE)
                          .transform_error(monadic::assert("Failed to initialize gpu instance"))
                          .value();
  const auto surface = gpu::Surface::create_from_window(instance, window)
                         .transform_error(monadic::
                                            assert("Failed to initialize window gpu surface"))
                         .value();

  // pick the best physical device
  const auto& physical_devices = instance.physical_devices();
  if (std::ranges::empty(physical_devices)) {
      elog("No render physical device found!");
      return 0;
  }
  ilog("Physical devices: {}", physical_devices);

  auto physical_device = as_ref(physical_devices.front());
  auto score           = gpu::score_physical_device(physical_device);
  for (auto i = 1u; i < std::ranges::size(physical_devices); ++i) {
      const auto& d       = physical_devices[i];
      const auto  d_score = gpu::score_physical_device(d);
      if (d_score > score) {
          physical_device = as_ref(d);
          score           = d_score;
      }
  }
  // auto _physical_device = std::ranges::max(
  //   physical_devices,
  //   std::ranges::greater {},
  //   gpu::score_physical_device
  // );

  ilog("Picked gpu: {}", *physical_device);

  // create gpu device
  const auto device = gpu::Device::create(*physical_device, instance)
                        .transform_error(monadic::assert("Failed to initialize gpu device"))
                        .value();

  // create swapchain
  const auto window_extent = window.extent();
  const auto swapchain     = gpu::SwapChain::create(device, surface, window_extent.to<3>())
                           .transform_error(monadic::assert("Failed to create swapchain"))
                           .value();

  const auto raster_queue = gpu::Queue::create(device, device.raster_queue_entry());

  const auto command_pool
    = gpu::CommandPool::create(device)
        .transform_error(monadic::assert("Failed to create raster queue command pool"))
        .value();

  // imgui related features
  
  static constexpr auto POOL_SIZES = std::array {
      gpu::DescriptorPool::Size { .type            = gpu::DescriptorType::COMBINED_IMAGE_SAMPLER,
                                 .descriptor_count = BUFFERING_COUNT /* IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE */ }
  };

  const auto descriptor_pool
    = gpu::DescriptorPool::create(device, POOL_SIZES, BUFFERING_COUNT)
         .transform_error(monadic::assert("Failed to create descriptor pool"))
         .value();

  // const auto path          = std::filesystem::path { u8"build/shaders/triangle.spv" };
  // const auto vertex_shader = gpu::Shader::load_from_file(device,
  //                                                        // SHADER_DIR "/triangle.spv",
  //                                                        path,
  //                                                        gpu::ShaderStageFlag::VERTEX)
  //                              .transform_error(monadic::assert("Failed to load vertex shader"))
  //                              .value();

  // const auto fragment_shader = gpu::Shader::load_from_file(device,
  //                                                          SHADER_DIR "/triangle.spv",
  //                                                          gpu::ShaderStageFlag::FRAGMENT)
  //                                .transform_error(monadic::
  //                                                   assert("Failed to load fragment shader"))
  //                                .value();

  // const auto pipeline_layout = gpu::PipelineLayout::create(device, {})
  //                                .transform_error(monadic::
  //                                                   assert("Failed to create pipeline layout"))
  //                                .value();

  // initialize render pass
  const auto render_pass
    = gpu::RenderPass::
        create(device,
               { .attachments = { { .format = swapchain.pixel_format() } },
                 .subpasses   = { { .bind_point            = gpu::PipelineBindPoint::GRAPHICS,
                                    .color_attachment_refs = { { .attachment_id = 0u } } } } })
          .transform_error(monadic::assert("Failed to create render pass"))
          .value();

  // initialize render pipeline
  // const auto window_viewport = gpu::Viewport {
  //     .position = { 0.f, 0.f },
  //     .extent   = window_extent.to<f32>(),
  //     .depth    = { 0.f, 1.f },
  // };
  // const auto scissor = gpu::Scissor {
  //     .offset = { 0, 0 },
  //     .extent = window_extent,
  // };

  // const auto state = gpu::RasterPipelineState {
  //     .input_assembly_state = { .topology = gpu::PrimitiveTopology::TRIANGLE_LIST, },
  //     .viewport_state       = { .viewports = { window_viewport },
  //                              .scissors  = { scissor }, },
  //     .color_blend_state
  //     = { .attachments = { { .blend_enable           = true,
  //                            .src_color_blend_factor = gpu::BlendFactor::SRC_ALPHA,
  //                            .dst_color_blend_factor = gpu::BlendFactor::ONE_MINUS_SRC_ALPHA,
  //                            .src_alpha_blend_factor = gpu::BlendFactor::SRC_ALPHA,
  //                            .dst_alpha_blend_factor = gpu::BlendFactor::ONE_MINUS_SRC_ALPHA,
  //                            .alpha_blend_operation  = gpu::BlendOperation::ADD, }, }, },
  //     .shader_state  = to_refs(vertex_shader, fragment_shader),
  // };

  // const auto pipeline = gpu::Pipeline::create(device, state, pipeline_layout, render_pass)
  //                         .transform_error(monadic::assert("Failed to create raster pipeline"))
  //                         .value();

  // create present engine resources
  auto submission_resources = init_by<std::vector<SubmissionResource>>([&](auto& out) noexcept {
      out.reserve(BUFFERING_COUNT);
      for (auto _ : range(BUFFERING_COUNT)) {
          out.push_back({
            .in_flight = gpu::Fence::create_signaled(device)
                           .transform_error(monadic::assert("Failed to create swapchain image "
                                                            "in flight fence"))
                           .value(),
            .image_available = gpu::Semaphore::create(device)
                                 .transform_error(monadic::assert("Failed to create "
                                                                  "present wait semaphore"))
                                 .value(),
            .render_cmb = command_pool.create_command_buffer()
                            .transform_error(monadic::assert("Failed to create transition "
                                                             "command buffers"))
                            .value(),
          });
      }
  });

  // transition swapchain image to present image
  const auto& images = swapchain.images();
  const auto image_count = std::ranges::size(images);

  auto transition_cmbs
    = command_pool.create_command_buffers(image_count)
        .transform_error(monadic::assert("Failed to create transition command buffers"))
        .value();

  auto image_resources = std::vector<SwapchainImageResource> {};
  image_resources.reserve(std::ranges::size(images));

  auto image_index = 0u;
  for (const auto& swap_image : images) {
      auto view = gpu::ImageView::create(device, swap_image)
                    .transform_error(core::monadic::
                                       assert("Failed to create swapchain image view"))
                    .value();
      auto framebuffer = render_pass.create_frame_buffer(device, window_extent, to_refs(view))
                           .transform_error(core::monadic::assert(
                             std::format("Failed to create framebuffer for image {}",
                                         image_index)))
                           .value();

      image_resources.push_back({
        .image           = as_ref(swap_image),
        .view            = std::move(view),
        .framebuffer     = std::move(framebuffer),
        .render_finished = gpu::Semaphore::create(device)
                             .transform_error(core::monadic::assert("Failed to create render "
                                                                    "signal semaphore"))
                             .value(),
      });

      auto& transition_cmb = transition_cmbs[image_index];
      *transition_cmb.begin(true)
         .transform_error(monadic::assert("Failed to begin texture transition command buffer"))
         .value()
         ->begin_debug_region(std::format("transition image {}", image_index))
         .transition_image_layout(swap_image,
                                  gpu::ImageLayout::UNDEFINED,
                                  gpu::ImageLayout::PRESENT_SRC)
         .end_debug_region()
         .end()
         .transform_error(monadic::assert("Failed to begin texture transition command "
                                          "buffer"));

      ++image_index;
  }

  const auto fence = gpu::Fence::create(device)
                       .transform_error(monadic::assert("Failed to create transition fence"))
                       .value();

  const auto cmbs = to_refs(transition_cmbs);

  raster_queue.submit({ .command_buffers = cmbs }, as_ref(fence))
    .transform_error(monadic::assert("Failed to submit texture transition command buffers"))
    .value();

  // wait for transition to be done
  fence.wait().transform_error(monadic::assert());

  ilog("loading imgui");
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize.x = window_extent.to<f32>().width;
  io.DisplaySize.y = window_extent.to<f32>().height;

  ilog("init vulkan imgui");
  auto init_info = ImGui_ImplVulkan_InitInfo {
    .ApiVersion = VK_API_VERSION_1_1,
    .Instance = instance.native_handle(),
    .PhysicalDevice = physical_device->native_handle(),
    .Device = device.native_handle(),
    .QueueFamily = 0,
    .Queue = raster_queue.native_handle(),
    .DescriptorPool = descriptor_pool.native_handle(),
    .RenderPass = render_pass.native_handle(),
    .MinImageCount = BUFFERING_COUNT,
    .ImageCount = BUFFERING_COUNT,
    .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
    .PipelineCache = nullptr,
    .Subpass = 0,
    .DescriptorPoolSize = 0,
    .UseDynamicRendering = false,
    .PipelineRenderingCreateInfo = {},
    .Allocator = nullptr,
    .CheckVkResultFn = [](VkResult err) static noexcept {
      expects(err == 0, std::format("[vulkan] Error: VkResult = {}", err));
    },
    .MinAllocationSize = 1024 * 1024,
  };
  // ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_1, gpu::imgui_vk_loader, std::bit_cast<void*>(&device));
  ImGui_ImplVulkan_Init(&init_info);

  auto current_frame = 0uz;
  window.event_loop([&] mutable noexcept {
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    // TODO replace with copy of grid, potentials, etc...
    ImGui::ShowDemoWindow(); // Show demo window! :)

    ImGui::Render();

    // get next swapchain image
    auto& submission_resource = submission_resources[current_frame];

    const auto& wait      = submission_resource.image_available;
    auto&       in_flight = submission_resource.in_flight;

    const auto acquire_next_image = bind_front(&gpu::SwapChain::acquire_next_image,
                                               &swapchain,
                                               100ms,
                                               std::cref(wait));
    const auto extract_index      = [](auto&& _result) static noexcept {
        auto&& [result, _image_index] = _result;
        return _image_index;
    };

    const auto image_index
      = in_flight.wait()
          .transform([&in_flight](auto&&) mutable noexcept { in_flight.reset(); })
          .and_then(acquire_next_image)
          .transform(extract_index)
          .transform_error(monadic::assert("Failed to acquire next swapchain image"))
          .value();

    const auto& swapchain_image_resource = image_resources[image_index];
    const auto& framebuffer              = swapchain_image_resource.framebuffer;
    const auto& signal                   = swapchain_image_resource.render_finished;

    static constexpr auto PIPELINE_FLAGS = std::array {
        gpu::PipelineStageFlag::COLOR_ATTACHMENT_OUTPUT
    };

    // render in it
    auto& render_cmb = submission_resource.render_cmb;
    render_cmb.reset()
       .transform_error(monadic::assert("Failed to reset render command buffer"))
       .value()
       ->begin()
       .transform_error(monadic::assert("Failed to begin render command buffer"))
       .value()
       ->begin_debug_region("Render imgui")
       .begin_render_pass(render_pass, framebuffer);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), render_cmb.native_handle());

    *render_cmb.end_render_pass()
       .end()
       .transform_error(monadic::assert("Failed to end render command buffer"))
       .value()
       ->submit(raster_queue, as_refs(wait), PIPELINE_FLAGS, as_refs(signal), as_ref(in_flight))
       .transform_error(monadic::assert("Failed to submit render command buffer"));

    // present it
    auto update_current_frame = [&current_frame](auto&&) mutable noexcept {
        if (++current_frame >= BUFFERING_COUNT) current_frame = 0;
    };

    raster_queue.present(as_refs(swapchain), as_refs(signal), as_view(image_index))
      .transform(update_current_frame)
      .transform_error(monadic::assert("Failed to present swapchain image"));

    // std::this_thread::yield();
  });
  
  raster_queue.wait_idle();
  device.wait_idle();

  ImGui_ImplVulkan_Shutdown();
  ImGui::DestroyContext();

  return 0;
}
