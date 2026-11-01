#pragma once

// TTY input queue shared by UART and keyboard drivers.

namespace og::io::tty {

void PushChar(char value) noexcept;
[[nodiscard]] bool PopChar(char& value) noexcept;
void Clear() noexcept;

}  // namespace og::io::tty
