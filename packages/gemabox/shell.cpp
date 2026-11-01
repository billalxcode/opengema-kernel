#include "gemabox/applets.h"

#include "gemapi/fs.h"
#include "gemapi/io.h"
#include "gemapi/process.h"
#include "gemapi/system.h"

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

constexpr Command kCommands[] = {
  {"help", &RunHelp},
  {"echo", &RunEcho},
  {"cat", &RunCat},
  {"ls", &RunList},
  {"clear", &RunClear},
  {"acpi", &RunAcpi},
  {"halt", &RunHalt},
  {"shutdown", &RunShutdown},
  {"reboot", &RunReboot},
};

constexpr gemapi::usize kCommandCount =
  sizeof(kCommands) / sizeof(kCommands[0]);

CommandResult RunHelp(const char* /*args*/, gemapi::usize /*args_length*/) {
  char text[128];
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
