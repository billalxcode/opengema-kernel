#include "gemabox/applets.h"

#include "gemapi/fs.h"
#include "gemapi/io.h"
#include "gemapi/paths.h"
#include "gemapi/process.h"
#include "gemapi/system.h"
#include "gemapi/time.h"

namespace {

constexpr char kPrompt[] = "gemabox> ";
constexpr gemapi::usize kMaxArgs = 8;
constexpr gemapi::usize kMaxName = 128;

enum class CommandResult {
  kContinue,
  kHalt,
};

using CommandFn = CommandResult (*)(const char* args,
                                   gemapi::usize args_length);

struct Command {
  const char* name;
  CommandFn run;
};

void cat_file(const char* path, const char* label) {
  auto file = gemapi::fs::File::open(path);
  if (!file) {
    gemapi::io::print(label);
    gemapi::io::write(": open failed\n");
    return;
  }
  char buffer[64];
  for (;;) {
    auto count = file.value().read(buffer, sizeof(buffer));
    if (!count) {
      gemapi::io::print(label);
      gemapi::io::write(": read failed\n");
      break;
    }
    if (count.value() == 0U) {
      break;
    }
    gemapi::io::write(buffer, count.value());
  }
}

void list_dir(const char* path) {
  for (gemapi::u64 index = 0;; ++index) {
    gemapi::fs::DirEntry entry{};
    auto found = gemapi::fs::list(path, index, entry);
    if (!found) {
      if (found.status() != gemapi::Status::ok) {
        gemapi::io::write("ls: list failed\n");
      }
      return;
    }
    if (!found.value()) {
      return;
    }
    gemapi::io::write(entry.name);
    if (entry.kind == gemapi::fs::Kind::directory) {
      gemapi::io::write("/");
    }
    gemapi::io::write("\n");
  }
}

bool run_path(const char* path, const char* const* argv,
              gemapi::usize argc) {
  auto child = gemapi::process::spawn(path, argv, argc);
  if (!child) {
    gemapi::io::write("run: spawn failed\n");
    return true;
  }
  int status = 0;
  if (gemapi::process::wait(child.value(), status) != gemapi::Status::ok) {
    gemapi::io::write("run: wait failed\n");
  }
  return true;
}

gemapi::usize append_text(char* out, gemapi::usize capacity,
                          gemapi::usize cursor, const char* text) {
  for (gemapi::usize index = 0; text[index] != '\0'; ++index) {
    if (cursor >= capacity) {
      return capacity;
    }
    out[cursor] = text[index];
    ++cursor;
  }
  return cursor;
}

gemapi::usize append_dec(char* out, gemapi::usize capacity,
                         gemapi::usize cursor, gemapi::u64 value) {
  char digits[20];
  gemapi::usize count = 0;
  if (value == 0U) {
    digits[count] = '0';
    ++count;
  }
  while (value > 0U) {
    digits[count] = static_cast<char>('0' + (value % 10U));
    ++count;
    value /= 10U;
  }
  while (count > 0U) {
    if (cursor >= capacity) {
      return capacity;
    }
    --count;
    out[cursor] = digits[count];
    ++cursor;
  }
  return cursor;
}

bool parse_u64(const char* text, gemapi::usize length, gemapi::u64& value) {
  if (text == nullptr || length == 0U) {
    return false;
  }
  gemapi::u64 result = 0;
  for (gemapi::usize index = 0; index < length; ++index) {
    const char symbol = text[index];
    if (symbol < '0' || symbol > '9') {
      return false;
    }
    const gemapi::u64 digit = static_cast<gemapi::u64>(symbol - '0');
    if (result > ((~0ULL - digit) / 10U)) {
      return false;
    }
    result = (result * 10U) + digit;
  }
  value = result;
  return true;
}

void trim(const char*& text, gemapi::usize& length) {
  while (length > 0U && text[0] == ' ') {
    ++text;
    --length;
  }
  while (length > 0U && text[length - 1U] == ' ') {
    --length;
  }
}

bool split_two(const char* args, gemapi::usize args_length,
               const char*& first, gemapi::usize& first_length,
               const char*& second, gemapi::usize& second_length) {
  trim(args, args_length);
  if (args_length == 0U) {
    return false;
  }
  gemapi::usize split = 0;
  while (split < args_length && args[split] != ' ') {
    ++split;
  }
  if (split >= args_length) {
    return false;
  }
  first = args;
  first_length = split;
  const char* rest = args + split;
  gemapi::usize rest_length = args_length - split;
  trim(rest, rest_length);
  if (rest_length == 0U) {
    return false;
  }
  second = rest;
  second_length = rest_length;
  return true;
}

bool text_equals(const char* text, gemapi::usize length, const char* value) {
  gemapi::usize index = 0;
  while (index < length && value[index] != '\0' && text[index] == value[index]) {
    ++index;
  }
  return index == length && value[index] == '\0';
}

const char* log_level_name(gemapi::system::LogLevel level) {
  switch (level) {
    case gemapi::system::LogLevel::error:
      return "error";
    case gemapi::system::LogLevel::warn:
      return "warn";
    case gemapi::system::LogLevel::info:
      return "info";
    case gemapi::system::LogLevel::debug:
      return "debug";
    default:
      return "unknown";
  }
}

bool parse_log_level(const char* text, gemapi::usize length,
                     gemapi::system::LogLevel& level) {
  if (length == 1U && text[0] >= '0' && text[0] <= '3') {
    level = static_cast<gemapi::system::LogLevel>(
      static_cast<gemapi::u64>(text[0] - '0'));
    return true;
  }
  if (text_equals(text, length, "error")) {
    level = gemapi::system::LogLevel::error;
    return true;
  }
  if (text_equals(text, length, "warn")) {
    level = gemapi::system::LogLevel::warn;
    return true;
  }
  if (text_equals(text, length, "info")) {
    level = gemapi::system::LogLevel::info;
    return true;
  }
  if (text_equals(text, length, "debug")) {
    level = gemapi::system::LogLevel::debug;
    return true;
  }
  return false;
}

bool finish_path(char* path, gemapi::usize capacity, gemapi::usize cursor) {
  if (cursor >= capacity) {
    return false;
  }
  path[cursor] = '\0';
  return true;
}

bool FindExternalPath(const char* name, char* buffer,
                      gemapi::usize capacity, const char** out_path) {
  if (name == nullptr || name[0] == '\0') {
    return false;
  }
  if (name[0] == '/') {
    if (!gemapi::fs::stat(name)) {
      return false;
    }
    *out_path = name;
    return true;
  }

  gemapi::usize cursor = 0;
  cursor = append_text(buffer, capacity, cursor, "/commands/");
  cursor = append_text(buffer, capacity, cursor, name);
  if (finish_path(buffer, capacity, cursor) && gemapi::fs::stat(buffer)) {
    *out_path = buffer;
    return true;
  }

  cursor = 0;
  cursor = append_text(buffer, capacity, cursor, "/applications/");
  cursor = append_text(buffer, capacity, cursor, name);
  cursor = append_text(buffer, capacity, cursor, "/");
  cursor = append_text(buffer, capacity, cursor, name);
  if (!finish_path(buffer, capacity, cursor) || !gemapi::fs::stat(buffer)) {
    return false;
  }
  *out_path = buffer;
  return true;
}

gemapi::usize BuildArgv(char* text, gemapi::usize length,
                        const char* argv[], gemapi::usize capacity) {
  gemapi::usize argc = 0;
  gemapi::usize index = 0;
  while (index < length) {
    while (index < length && text[index] == ' ') {
      ++index;
    }
    if (index >= length) {
      break;
    }
    if (argc >= capacity) {
      break;
    }
    argv[argc] = text + index;
    ++argc;
    while (index < length && text[index] != ' ') {
      ++index;
    }
    if (index < length) {
      text[index] = '\0';
      ++index;
    }
  }
  return argc;
}

CommandResult RunHalt(const char* /*args*/, gemapi::usize /*args_length*/) {
  return CommandResult::kHalt;
}

CommandResult RunHelp(const char* /*args*/, gemapi::usize /*args_length*/);

CommandResult RunEcho(const char* args, gemapi::usize args_length) {
  gemapi::io::write(args, args_length);
  gemapi::io::write("\n");
  return CommandResult::kContinue;
}

CommandResult RunCat(const char* args, gemapi::usize args_length) {
  if (args_length == 0U) {
    gemapi::io::write("cat: missing path\n");
    return CommandResult::kContinue;
  }
  cat_file(args, "cat");
  return CommandResult::kContinue;
}

CommandResult RunList(const char* args, gemapi::usize args_length) {
  if (args_length == 0U) {
    list_dir("/");
  } else {
    list_dir(args);
  }
  return CommandResult::kContinue;
}

CommandResult RunClear(const char* /*args*/, gemapi::usize /*args_length*/) {
  gemapi::io::write("\x1b[2J\x1b[H");
  return CommandResult::kContinue;
}

CommandResult RunAcpi(const char* /*args*/, gemapi::usize /*args_length*/) {
  cat_file("/proc/acpi", "acpi");
  return CommandResult::kContinue;
}

CommandResult RunShutdown(const char* /*args*/,
                          gemapi::usize /*args_length*/) {
  if (gemapi::system::shutdown() != gemapi::Status::ok) {
    gemapi::io::write("shutdown: not supported\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunReboot(const char* /*args*/, gemapi::usize /*args_length*/) {
  if (gemapi::system::reboot() != gemapi::Status::ok) {
    gemapi::io::write("reboot: not supported\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunUptime(const char* /*args*/, gemapi::usize /*args_length*/) {
  char text[64];
  gemapi::usize cursor =
    append_text(text, sizeof(text), 0, "uptime: ");
  cursor = append_dec(text, sizeof(text), cursor, gemapi::time::now_ms());
  cursor = append_text(text, sizeof(text), cursor, " ms\n");
  gemapi::io::write(text, cursor);
  return CommandResult::kContinue;
}

CommandResult RunPs(const char* /*args*/, gemapi::usize /*args_length*/) {
  cat_file("/proc/processes", "ps");
  return CommandResult::kContinue;
}

CommandResult RunDmesg(const char* /*args*/, gemapi::usize /*args_length*/) {
  gemapi::io::write("dmesg:\n");
  cat_file("/proc/dmesg", "dmesg");
  return CommandResult::kContinue;
}

CommandResult RunLogLevel(const char* args, gemapi::usize args_length) {
  trim(args, args_length);
  if (args_length != 0U) {
    gemapi::system::LogLevel level = gemapi::system::LogLevel::info;
    if (!parse_log_level(args, args_length, level)) {
      gemapi::io::write("loglevel: expected error, warn, info, debug, or 0-3\n");
      return CommandResult::kContinue;
    }
    if (gemapi::system::set_log_level(level) != gemapi::Status::ok) {
      gemapi::io::write("loglevel: failed\n");
      return CommandResult::kContinue;
    }
  }

  const gemapi::system::LogLevel level = gemapi::system::log_level();
  char text[64];
  gemapi::usize cursor = append_text(text, sizeof(text), 0, "loglevel: ");
  cursor = append_text(text, sizeof(text), cursor, log_level_name(level));
  cursor = append_text(text, sizeof(text), cursor, " (");
  cursor = append_dec(text, sizeof(text), cursor,
                      static_cast<gemapi::u64>(level));
  cursor = append_text(text, sizeof(text), cursor, ")\n");
  gemapi::io::write(text, cursor);
  return CommandResult::kContinue;
}

CommandResult RunPersist(const char* args, gemapi::usize args_length) {
  trim(args, args_length);
  char path[96];
  char path2[96];
  char path3[96];
  char path4[96];
  gemapi::usize cursor =
    append_text(path, sizeof(path), 0, gemapi::paths::kBootVolume);
  cursor = append_text(path, sizeof(path), cursor, "/persist.bin");
  if (!finish_path(path, sizeof(path), cursor)) {
    gemapi::io::write("persist: path too long\n");
    return CommandResult::kContinue;
  }
  cursor = append_text(path2, sizeof(path2), 0, gemapi::paths::kBootVolume);
  cursor = append_text(path2, sizeof(path2), cursor, "/persist2.bin");
  if (!finish_path(path2, sizeof(path2), cursor)) {
    gemapi::io::write("persist: path too long\n");
    return CommandResult::kContinue;
  }
  cursor = append_text(path3, sizeof(path3), 0, gemapi::paths::kBootVolume);
  cursor = append_text(path3, sizeof(path3), cursor, "/testdir");
  if (!finish_path(path3, sizeof(path3), cursor)) {
    gemapi::io::write("persist: path too long\n");
    return CommandResult::kContinue;
  }
  cursor = append_text(path4, sizeof(path4), 0, gemapi::paths::kBootVolume);
  cursor = append_text(path4, sizeof(path4), cursor, "/renamed.bin");
  if (!finish_path(path4, sizeof(path4), cursor)) {
    gemapi::io::write("persist: path too long\n");
    return CommandResult::kContinue;
  }

  constexpr char kMessage[] = "persist ok\n";
  if (text_equals(args, args_length, "write") ||
      text_equals(args, args_length, "create")) {
    const bool create = text_equals(args, args_length, "create");
    const char* target = create ? path2 : path;
    auto file = gemapi::fs::File::open(
      target, gemapi::fs::kOpenWrite | (create ? gemapi::fs::kOpenCreate : 0));
    if (!file) {
      gemapi::io::write("persist: open failed\n");
      return CommandResult::kContinue;
    }
    if (file.value().write(kMessage, sizeof(kMessage) - 1U) !=
        gemapi::Status::ok) {
      gemapi::io::write("persist: write failed\n");
      return CommandResult::kContinue;
    }
    if (create) {
      gemapi::io::write("persist: create ok\n");
    } else {
      gemapi::io::write("persist: write ok\n");
    }
    return CommandResult::kContinue;
  }

  if (text_equals(args, args_length, "read") ||
      text_equals(args, args_length, "read2")) {
    const bool created = text_equals(args, args_length, "read2");
    const char* target = created ? path2 : path;
    auto file = gemapi::fs::File::open(target);
    if (!file) {
      gemapi::io::write("persist: open failed\n");
      return CommandResult::kContinue;
    }
    char buffer[16]{};
    auto count = file.value().read(buffer, sizeof(buffer));
    if (!count || count.value() < (sizeof(kMessage) - 1U) ||
        !text_equals(buffer, sizeof(kMessage) - 1U, kMessage)) {
      gemapi::io::write("persist: read failed\n");
      return CommandResult::kContinue;
    }
    if (created) {
      gemapi::io::write("persist: read2 ok\n");
    } else {
      gemapi::io::write("persist: read ok\n");
    }
    return CommandResult::kContinue;
  }

  if (text_equals(args, args_length, "mkdir")) {
    if (gemapi::fs::mkdir(path3) != gemapi::Status::ok) {
      gemapi::io::write("persist: mkdir failed\n");
      return CommandResult::kContinue;
    }
    gemapi::io::write("persist: mkdir ok\n");
    return CommandResult::kContinue;
  }

  if (text_equals(args, args_length, "delete")) {
    const auto info = gemapi::fs::stat(path3);
    if (!info) {
      gemapi::io::write("persist: delete failed\n");
      return CommandResult::kContinue;
    }
    const gemapi::Status status =
      info.value().kind == gemapi::fs::Kind::directory
        ? gemapi::fs::rmdir(path3)
        : gemapi::fs::unlink(path3);
    if (status != gemapi::Status::ok) {
      gemapi::io::write("persist: delete failed\n");
      return CommandResult::kContinue;
    }
    gemapi::io::write("persist: delete ok\n");
    return CommandResult::kContinue;
  }

  if (text_equals(args, args_length, "rename")) {
    if (gemapi::fs::rename(path2, path4) != gemapi::Status::ok) {
      gemapi::io::write("persist: rename failed\n");
      return CommandResult::kContinue;
    }
    gemapi::io::write("persist: rename ok\n");
    return CommandResult::kContinue;
  }

  if (text_equals(args, args_length, "truncate")) {
    auto file = gemapi::fs::File::open(
      path, gemapi::fs::kOpenWrite | gemapi::fs::kOpenTruncate);
    if (!file) {
      gemapi::io::write("persist: truncate failed\n");
      return CommandResult::kContinue;
    }
    const auto info = gemapi::fs::stat(path);
    if (!info || info.value().size != 0U) {
      gemapi::io::write("persist: truncate failed\n");
      return CommandResult::kContinue;
    }
    gemapi::io::write("persist: truncate ok\n");
    return CommandResult::kContinue;
  }

  if (text_equals(args, args_length, "empty")) {
    const auto info = gemapi::fs::stat(path);
    if (!info || info.value().size != 0U) {
      gemapi::io::write("persist: empty failed\n");
      return CommandResult::kContinue;
    }
    gemapi::io::write("persist: empty ok\n");
    return CommandResult::kContinue;
  }

  gemapi::io::write(
    "persist: usage: persist write|read|create|read2|mkdir|delete|rename|truncate|empty\n");
  return CommandResult::kContinue;
}

CommandResult RunSleep(const char* args, gemapi::usize args_length) {
  if (args_length == 0U) {
    gemapi::io::write("sleep: missing milliseconds\n");
    return CommandResult::kContinue;
  }
  gemapi::u64 milliseconds = 0;
  if (!parse_u64(args, args_length, milliseconds)) {
    gemapi::io::write("sleep: invalid milliseconds\n");
    return CommandResult::kContinue;
  }
  if (gemapi::time::sleep_ms(milliseconds) != gemapi::Status::ok) {
    gemapi::io::write("sleep: failed\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunMkdir(const char* args, gemapi::usize args_length) {
  trim(args, args_length);
  if (args_length == 0U) {
    gemapi::io::write("mkdir: missing path\n");
    return CommandResult::kContinue;
  }
  if (gemapi::fs::mkdir(args) != gemapi::Status::ok) {
    gemapi::io::write("mkdir: failed\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunRm(const char* args, gemapi::usize args_length) {
  trim(args, args_length);
  if (args_length == 0U) {
    gemapi::io::write("rm: missing path\n");
    return CommandResult::kContinue;
  }
  const auto info = gemapi::fs::stat(args);
  if (!info) {
    gemapi::io::write("rm: failed\n");
    return CommandResult::kContinue;
  }
  const gemapi::Status status =
    info.value().kind == gemapi::fs::Kind::directory
      ? gemapi::fs::rmdir(args)
      : gemapi::fs::unlink(args);
  if (status != gemapi::Status::ok) {
    gemapi::io::write("rm: failed\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunMv(const char* args, gemapi::usize args_length) {
  const char* old_path = nullptr;
  const char* new_path = nullptr;
  gemapi::usize old_length = 0;
  gemapi::usize new_length = 0;
  if (!split_two(args, args_length, old_path, old_length, new_path,
                 new_length)) {
    gemapi::io::write("mv: usage: mv <old> <new>\n");
    return CommandResult::kContinue;
  }
  char old_buffer[128];
  char new_buffer[128];
  if (old_length >= sizeof(old_buffer) || new_length >= sizeof(new_buffer)) {
    gemapi::io::write("mv: path too long\n");
    return CommandResult::kContinue;
  }
  for (gemapi::usize index = 0; index < old_length; ++index) {
    old_buffer[index] = old_path[index];
  }
  old_buffer[old_length] = '\0';
  for (gemapi::usize index = 0; index < new_length; ++index) {
    new_buffer[index] = new_path[index];
  }
  new_buffer[new_length] = '\0';
  if (gemapi::fs::rename(old_buffer, new_buffer) != gemapi::Status::ok) {
    gemapi::io::write("mv: failed\n");
  }
  return CommandResult::kContinue;
}

constexpr Command kCommands[] = {
  {"help", &RunHelp},
  {"echo", &RunEcho},
  {"cat", &RunCat},
  {"ls", &RunList},
  {"clear", &RunClear},
  {"acpi", &RunAcpi},
  {"uptime", &RunUptime},
  {"sleep", &RunSleep},
  {"ps", &RunPs},
  {"dmesg", &RunDmesg},
  {"loglevel", &RunLogLevel},
  {"persist", &RunPersist},
  {"mkdir", &RunMkdir},
  {"rm", &RunRm},
  {"mv", &RunMv},
  {"halt", &RunHalt},
  {"shutdown", &RunShutdown},
  {"reboot", &RunReboot},
};

constexpr gemapi::usize kCommandCount =
  sizeof(kCommands) / sizeof(kCommands[0]);

CommandResult RunHelp(const char* /*args*/, gemapi::usize /*args_length*/) {
  char text[192];
  gemapi::usize cursor = append_text(text, sizeof(text), 0, "commands: ");
  for (gemapi::usize index = 0; index < kCommandCount; ++index) {
    if (index != 0U) {
      cursor = append_text(text, sizeof(text), cursor, ", ");
    }
    cursor = append_text(text, sizeof(text), cursor, kCommands[index].name);
  }
  cursor = append_text(text, sizeof(text), cursor, "\n");
  gemapi::io::write(text, cursor);
  return CommandResult::kContinue;
}

const Command* FindCommand(const char* name, gemapi::usize length) {
  for (gemapi::usize command_index = 0; command_index < kCommandCount;
       ++command_index) {
    const Command& command = kCommands[command_index];
    gemapi::usize index = 0;
    while (index < length && command.name[index] != '\0' &&
           command.name[index] == name[index]) {
      ++index;
    }
    if (index == length && command.name[index] == '\0') {
      return &command;
    }
  }
  return nullptr;
}

}  // namespace

namespace gemabox {

int run_shell() {
  char line[128];
  for (;;) {
    gemapi::io::write(kPrompt);
    auto length = gemapi::io::read_line(line, sizeof(line));
    if (!length) {
      return 1;
    }
    const gemapi::usize size = length.value();
    if (size == 0U) {
      continue;
    }

    gemapi::usize name_length = 0;
    while (name_length < size && line[name_length] != ' ') {
      ++name_length;
    }
    const char* args = line + name_length;
    gemapi::usize args_length = size - name_length;
    if (args_length != 0U) {
      ++args;
      --args_length;
    }

    const Command* command = FindCommand(line, name_length);
    if (command != nullptr) {
      if (command->run(args, args_length) == CommandResult::kHalt) {
        return 0;
      }
      continue;
    }

    char name[kMaxName];
    const gemapi::usize name_copy =
      name_length < (sizeof(name) - 1U) ? name_length : (sizeof(name) - 1U);
    for (gemapi::usize index = 0; index < name_copy; ++index) {
      name[index] = line[index];
    }
    name[name_copy] = '\0';

    char path_buffer[kMaxName];
    const char* path = nullptr;
    if (FindExternalPath(name, path_buffer, sizeof(path_buffer), &path)) {
      const char* argv[kMaxArgs];
      const gemapi::usize argc = BuildArgv(line, size, argv, kMaxArgs);
      if (argc > 0U) {
        (void)run_path(path, argv, argc);
        continue;
      }
    }

    gemapi::io::write("unknown: ");
    gemapi::io::write(line, size);
    gemapi::io::write("\n");
  }
}

}  // namespace gemabox
