#include "og/platform/fdt.h"

#include "memory.h"
#include "og/platform/fdt_internal.h"

namespace og::platform::fdt {

using namespace detail;

Result<std::span<const uint8_t>> FindProperty(uint32_t node,
                                              const char* name) noexcept {
  if (!Ready()) {
    return Result<std::span<const uint8_t>>::Err(Status::kNotInit);
  }
  if (name == nullptr) {
    return Result<std::span<const uint8_t>>::Err(Status::kInvalidArg);
  }
  const Result<uint32_t> body = NodeBodyOffset(node);
  if (body.is_err()) {
    return Result<std::span<const uint8_t>>::Err(body.status());
  }
  uint32_t offset = body.value();

  while (offset + 4U <= StructEnd()) {
    const uint32_t token = ReadU32(offset);
    offset += 4U;
    if (token == kTokenProperty) {
      if (!Valid(offset, 8)) {
        return Result<std::span<const uint8_t>>::Err(Status::kInvalidArg);
      }
      const uint32_t length = ReadU32(offset);
      const uint32_t name_offset = ReadU32(offset + 4U);
      const uint32_t data_offset = offset + 8U;
      if (!Valid(data_offset, length)) {
        return Result<std::span<const uint8_t>>::Err(Status::kInvalidArg);
      }
      const char* property_name = StringsEntry(name_offset);
      if (property_name == nullptr) {
        return Result<std::span<const uint8_t>>::Err(Status::kInvalidArg);
      }
      if (mem::Same(property_name, name)) {
        return Result<std::span<const uint8_t>>::Ok(std::span<const uint8_t>(
          Base() + data_offset, length));
      }
      offset = Align4(data_offset + length);
      continue;
    }
    break;
  }

  return Result<std::span<const uint8_t>>::Err(Status::kNotFound);
}

}  // namespace og::platform::fdt
