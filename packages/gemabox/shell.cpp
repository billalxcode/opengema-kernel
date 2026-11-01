#include "gemabox/applets.h"

#include "gemapi/fs.h"
#include "gemapi/io.h"
#include "gemapi/process.h"

namespace {

constexpr char kPrompt[] = "gemabox> ";

enum class CommandResult {
  kContinue,
  kExit,
};

using CommandFn = CommandResult (*)(const char* args,
                                   gemapi::usize args_length);

struct Command {
  const char* name;
  CommandFn run;
};

void cat_file(const char* path) {
  auto file = gemapi::fs::File::open(path);
  if (!file) {
    gemapi::io::write("cat: open failed\n");
    return;
  }
  char buffer[64];
  for (;;) {
    auto count = file.value().read(buffer, sizeof(buffer));
    if (!count) {
      gemapi::io::write("cat: read failed\n");
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

bool run_path(const char* path, const char* name) {
  const char* argv[] = {name};
  auto child = gemapi::process::spawn(path, argv, 1);
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

bool run_external(const char* name) {
  char path[128];

  gemapi::usize cursor = 0;
  cursor = append_text(path, sizeof(path), cursor, "/commands/");
  cursor = append_text(path, sizeof(path), cursor, name);
  if (!finish_path(path, sizeof(path), cursor)) {
    return false;
  }
  if (gemapi::fs::stat(path)) {
    return run_path(path, name);
  }

  cursor = 0;
  cursor = append_text(path, sizeof(path), cursor, "/applications/");
  cursor = append_text(path, sizeof(path), cursor, name);
  cursor = append_text(path, sizeof(path), cursor, "/");
  cursor = append_text(path, sizeof(path), cursor, name);
  if (!finish_path(path, sizeof(path), cursor)) {
    return false;
  }
  if (!gemapi::fs::stat(path)) {
    return false;
  }
  return run_path(path, name);
}

CommandResult RunExit(const char* /*args*/, gemapi::usize /*args_length*/) {
  return CommandResult::kExit;
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
  cat_file(args);
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

constexpr Command kCommands[] = {
  {"help", &RunHelp},
  {"echo", &RunEcho},
  {"cat", &RunCat},
  {"ls", &RunList},
  {"clear", &RunClear},
  {"exit", &RunExit},
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
      if (command->run(args, args_length) == CommandResult::kExit) {
        return 0;
      }
      continue;
    }

    if (args_length == 0U && run_external(line)) {
      continue;
    }

    gemapi::io::write("unknown: ");
    gemapi::io::write(line, size);
    gemapi::io::write("\n");
  }
}

}  // namespace gemabox
