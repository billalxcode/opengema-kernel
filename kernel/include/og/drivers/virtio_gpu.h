#pragma once

// virtio-gpu 2D driver on top of the modern PCI transport. Sets up a
// scanout and exposes its backing memory through a framebuffer console.
// Control queue requests are polled, no interrupt path yet.

#include <cstdint>

#include "og/core/status.h"
#include "og/dma/dma.h"
#include "og/drivers/pci.h"
#include "og/drivers/virtio.h"
#include "og/io/console.h"
#include "og/io/framebuffer.h"

namespace og::drivers::virtio {

class Gpu {
 public:
  Gpu() noexcept = default;
  ~Gpu() = default;

  Gpu(const Gpu&) = delete;
  Gpu& operator=(const Gpu&) = delete;

  [[nodiscard]] Status init(const pci::Info& info) noexcept;

  // Uploads a rectangle from the backing store to the host scanout.
  [[nodiscard]] Status flush(uint32_t x, uint32_t y, uint32_t width,
                             uint32_t height) noexcept;

  [[nodiscard]] io::Console console() noexcept { return console_.console(); }
  [[nodiscard]] bool is_ready() const noexcept { return ready_; }
  [[nodiscard]] uint32_t width() const noexcept { return width_; }
  [[nodiscard]] uint32_t height() const noexcept { return height_; }
  [[nodiscard]] uint32_t stride() const noexcept { return stride_; }
  [[nodiscard]] uint32_t resource_id() const noexcept { return resource_id_; }
  [[nodiscard]] uint32_t num_scanouts() const noexcept { return num_scanouts_; }
  [[nodiscard]] uint64_t mmio_base() const noexcept {
    return transport_.common_base();
  }
  [[nodiscard]] uint64_t dma_bytes() const noexcept {
    return request_memory_.size + response_memory_.size + framebuffer_.size +
           queue_.memory_bytes();
  }

 private:
  [[nodiscard]] Status command(const void* request, uint32_t request_size,
                               uint32_t expected_response, void* response,
                               uint32_t response_size) noexcept;
  [[nodiscard]] Status create_resource() noexcept;
  [[nodiscard]] Status attach_backing() noexcept;
  [[nodiscard]] Status set_scanout() noexcept;
  static Status PresentTrampoline(void* context, uint32_t x, uint32_t y,
                                  uint32_t width, uint32_t height) noexcept;

  PciTransport transport_;
  Queue queue_;
  dma::DmaBuffer request_memory_{};
  dma::DmaBuffer response_memory_{};
  dma::DmaBuffer framebuffer_{};
  io::FramebufferConsole console_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t stride_ = 0;
  uint32_t resource_id_ = 1;
  uint32_t num_scanouts_ = 0;
  bool ready_ = false;
};

[[nodiscard]] bool IsVirtioGpu(const pci::Info& info) noexcept;

}  // namespace og::drivers::virtio
