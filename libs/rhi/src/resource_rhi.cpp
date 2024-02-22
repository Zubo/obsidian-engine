#include <obsidian/core/logging.hpp>
#include <obsidian/rhi/resource_rhi.hpp>

using namespace obsidian::rhi;

ResourceTransferRHI::ResourceTransferRHI(
    std::future<void> transferCompletedFuture)
    : _transferCompletedFuture{std::move(transferCompletedFuture)} {}

bool ResourceTransferRHI::transferStarted() const {
  return _transferCompletedFuture.valid();
}

void ResourceTransferRHI::waitCompleted() const {
  if (!_transferCompletedFuture.valid()) {
    OBS_LOG_WARN("waitCompleted called on transfer that wasn't in progress");
    return;
  }

  _transferCompletedFuture.wait();
}
