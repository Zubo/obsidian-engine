#include <SDL_video.h>
#include <glm/common.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>
#include <vk_rhi/vk_rhi.hpp>

#include <SDL2/SDL_events.h>

using namespace obsidian::vk_rhi;

void VulkanRHI::handleKeyboardInput(SDL_KeyboardEvent const& e) {
  glm::vec3 constexpr worldX{1.f, 0.f, 0.0f};
  glm::vec3 constexpr worldY{0.f, 1.f, 0.f};
  glm::vec3 constexpr worldZ{0.f, 0.f, 1.f};

  glm::vec3 const cameraForward = glm::normalize(
      glm::rotate(_cameraRotationRad.y, worldY) *
      glm::rotate(_cameraRotationRad.x, worldX) * glm::vec4{-worldZ, 1.f});

  glm::vec3 const cameraRight = glm::cross(cameraForward, worldY);
  switch (e.keysym.sym) {
  case SDLK_e:
    _cameraPos += cameraForward;
    break;
  case SDLK_d:
    _cameraPos -= cameraForward;
    break;
  case SDLK_s:
    _cameraPos -= cameraRight;
    break;
  case SDLK_f:
    _cameraPos += cameraRight;
    break;
  case SDLK_r:
    _cameraPos += worldY;
    break;
  case SDLK_w:
    _cameraPos -= worldY;
    break;
  }
}

void VulkanRHI::handleMoseInput(SDL_MouseMotionEvent const& e) {
  constexpr float pi = 3.14f;
  constexpr float camMotionFactor = 0.01f;
  _cameraRotationRad += camMotionFactor * glm::vec2{-e.yrel, -e.xrel};
  _cameraRotationRad.x =
      glm::clamp(_cameraRotationRad.x, -0.5f * pi, 0.5f * pi);
}

void VulkanRHI::handleWindowEvent(SDL_WindowEvent const& e) {
  if (e.event == SDL_WINDOWEVENT_SIZE_CHANGED) {

    vkDeviceWaitIdle(_vkDevice);
    _skipFrame = true;
    _windowExtent.width = e.data1;
    _windowExtent.height = e.data2;

    _swapchainDeletionQueue.flush();

    initSwapchain();
    initDefaultRenderPass();
    initFramebuffers();
    initDefaultPipelines();
  }
}
