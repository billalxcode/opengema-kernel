#pragma once

// Kernel entry contract. The boot loader calls kmain exactly once.

namespace og {

struct BootInfo;

void kmain(BootInfo* info) noexcept;

}  // namespace og
