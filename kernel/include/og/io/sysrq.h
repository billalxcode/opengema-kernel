#pragma once

// Magic SysRq filter shared by UART and keyboard input paths.

namespace og::io {

using SysRqHandler = void (*)(char key) noexcept;

// Handler runs outside the input lock, in input context. It must be short.
void SetSysRqHandler(SysRqHandler handler) noexcept;

// Returns true when the character was consumed by the SysRq state machine.
// The prefix is Ctrl-A (0x01); the following key is passed to the handler.
[[nodiscard]] bool SysRqFilter(char value) noexcept;

}  // namespace og::io
