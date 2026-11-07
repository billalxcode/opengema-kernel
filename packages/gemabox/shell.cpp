#include "gemabox/applets.h"

#include "gemapi/fs.h"
#include "gemapi/io.h"
#include "gemapi/process.h"
#include "gemapi/system.h"
#include "gemapi/time.h"

namespace {

constexpr char kPrompt[] = "gemabox> ";
constexpr char kTimezonePath[] = "/volumes/boot/gsl.tz";
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

gemapi::usize append_padded(char* out, gemapi::usize capacity,
                            gemapi::usize cursor, gemapi::u64 value,
                            gemapi::usize width) {
  char digits[20];
  gemapi::usize count = 0;
  do {
    digits[count] = static_cast<char>('0' + (value % 10U));
    ++count;
    value /= 10U;
  } while (value > 0U && count < sizeof(digits));
  while (count < width && count < sizeof(digits)) {
    digits[count] = '0';
    ++count;
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

gemapi::usize append_octal(char* out, gemapi::usize capacity,
                          gemapi::usize cursor, gemapi::u64 value,
                          gemapi::usize width) {
  char digits[20];
  gemapi::usize count = 0;
  do {
    digits[count] = static_cast<char>('0' + (value % 8U));
    ++count;
    value /= 8U;
  } while (value > 0U && count < sizeof(digits));
  while (count < width && count < sizeof(digits)) {
    digits[count] = '0';
    ++count;
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

gemapi::usize format_unix_time(char* out, gemapi::usize capacity,
                               gemapi::u64 seconds,
                               gemapi::i64 offset_minutes) {
  const gemapi::u64 days = seconds / 86400U;
  const gemapi::u64 clock = seconds % 86400U;
  const gemapi::u64 hour = clock / 3600U;
  const gemapi::u64 minute = (clock % 3600U) / 60U;
  const gemapi::u64 second = clock % 60U;

  const gemapi::i64 shifted =
    static_cast<gemapi::i64>(days) + static_cast<gemapi::i64>(719468);
  const gemapi::i64 era =
    (shifted >= 0 ? shifted : shifted - 146096) / 146097;
  const gemapi::u64 day_of_era =
    static_cast<gemapi::u64>(shifted - era * 146097);
  const gemapi::u64 year_of_era =
    (day_of_era - (day_of_era / 1460U) + (day_of_era / 36524U) -
     (day_of_era / 146096U)) /
    365U;
  gemapi::i64 year =
    static_cast<gemapi::i64>(year_of_era) + era * 400;
  const gemapi::u64 day_of_year =
    day_of_era - (365U * year_of_era + year_of_era / 4U -
                  year_of_era / 100U);
  const gemapi::u64 month_part = (5U * day_of_year + 2U) / 153U;
  const gemapi::u64 day =
    day_of_year - (153U * month_part + 2U) / 5U + 1U;
  const gemapi::u64 month =
    month_part < 10U ? month_part + 3U : month_part - 9U;
  if (month <= 2U) {
    ++year;
  }

  gemapi::usize cursor = 0;
  cursor = append_padded(out, capacity, cursor,
                         static_cast<gemapi::u64>(year), 4);
  cursor = append_text(out, capacity, cursor, "-");
  cursor = append_padded(out, capacity, cursor, month, 2);
  cursor = append_text(out, capacity, cursor, "-");
  cursor = append_padded(out, capacity, cursor, day, 2);
  cursor = append_text(out, capacity, cursor, " ");
  cursor = append_padded(out, capacity, cursor, hour, 2);
  cursor = append_text(out, capacity, cursor, ":");
  cursor = append_padded(out, capacity, cursor, minute, 2);
  cursor = append_text(out, capacity, cursor, ":");
  cursor = append_padded(out, capacity, cursor, second, 2);
  cursor = append_text(out, capacity, cursor, " UTC");
  if (offset_minutes != 0) {
    const bool negative = offset_minutes < 0;
    const gemapi::u64 absolute = static_cast<gemapi::u64>(
      negative ? -offset_minutes : offset_minutes);
    cursor = append_text(out, capacity, cursor, negative ? "-" : "+");
    cursor = append_padded(out, capacity, cursor, absolute / 60U, 2);
    cursor = append_text(out, capacity, cursor, ":");
    cursor = append_padded(out, capacity, cursor, absolute % 60U, 2);
  }
  cursor = append_text(out, capacity, cursor, "\n");
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

bool parse_octal(const char* text, gemapi::usize length,
                 gemapi::u64& value) {
  if (text == nullptr || length == 0U) {
    return false;
  }
  gemapi::u64 result = 0;
  for (gemapi::usize index = 0; index < length; ++index) {
    const char symbol = text[index];
    if (symbol < '0' || symbol > '7') {
      return false;
    }
    result = (result * 8U) + static_cast<gemapi::u64>(symbol - '0');
    if (result > 0777U) {
      return false;
    }
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

bool next_token(const char*& cursor, const char* end, char* out,
                gemapi::usize capacity, gemapi::usize& length) {
  while (cursor < end && *cursor == ' ') {
    ++cursor;
  }
  if (cursor >= end) {
    return false;
  }
  bool quoted = false;
  gemapi::usize count = 0;
  while (cursor < end) {
    const char value = *cursor;
    if (value == '"') {
      quoted = !quoted;
      ++cursor;
      continue;
    }
    if (!quoted && value == ' ') {
      break;
    }
    if ((count + 1U) < capacity) {
      out[count] = value;
      ++count;
    }
    ++cursor;
  }
  if (quoted) {
    return false;
  }
  out[count] = '\0';
  length = count;
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

CommandResult RunStat(const char* args, gemapi::usize args_length) {
  char path[128];
  gemapi::usize length = 0;
  const char* cursor = args;
  const char* end = args + args_length;
  if (!next_token(cursor, end, path, sizeof(path), length) ||
      length >= (sizeof(path) - 1U)) {
    gemapi::io::write("stat: missing or invalid path\n");
    return CommandResult::kContinue;
  }
  char extra[2];
  gemapi::usize extra_length = 0;
  if (next_token(cursor, end, extra, sizeof(extra), extra_length)) {
    gemapi::io::write("stat: usage: stat <path>\n");
    return CommandResult::kContinue;
  }
  const auto info = gemapi::fs::stat(path);
  if (!info) {
    gemapi::io::write("stat: failed\n");
    return CommandResult::kContinue;
  }
  const gemapi::fs::Kind kind = info.value().kind;
  const char* kind_text = "other";
  if (kind == gemapi::fs::Kind::file) {
    kind_text = "file";
  } else if (kind == gemapi::fs::Kind::directory) {
    kind_text = "directory";
  } else if (kind == gemapi::fs::Kind::device) {
    kind_text = "device";
  }

  char text[96];
  gemapi::usize text_length = 0;
  text_length = append_text(text, sizeof(text), text_length, "type: ");
  text_length = append_text(text, sizeof(text), text_length, kind_text);
  text_length = append_text(text, sizeof(text), text_length, "\nsize: ");
  text_length = append_dec(text, sizeof(text), text_length, info.value().size);
  text_length = append_text(text, sizeof(text), text_length, "\nmode: ");
  text_length = append_octal(text, sizeof(text), text_length,
                             info.value().mode, 4);
  text_length = append_text(text, sizeof(text), text_length, "\nuid: ");
  text_length = append_dec(text, sizeof(text), text_length, info.value().uid);
  text_length = append_text(text, sizeof(text), text_length, "\ngid: ");
  text_length = append_dec(text, sizeof(text), text_length, info.value().gid);
  text_length = append_text(text, sizeof(text), text_length, "\n");
  gemapi::io::write(text, text_length);

  if (info.value().mtime == 0U) {
    gemapi::io::write("mtime: -\n");
    return CommandResult::kContinue;
  }
  const auto zone = gemapi::time::timezone_minutes();
  const gemapi::i64 offset = zone ? zone.value() : 0;
  char when[48];
  const gemapi::usize when_length =
    format_unix_time(when, sizeof(when), info.value().mtime, offset);
  gemapi::io::write("mtime: ");
  gemapi::io::write(when, when_length);
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

CommandResult RunDate(const char* args, gemapi::usize args_length) {
  trim(args, args_length);
  if (args_length != 0U) {
    gemapi::u64 seconds = 0;
    if (!parse_u64(args, args_length, seconds)) {
      gemapi::io::write("date: usage: date [unix-seconds]\n");
      return CommandResult::kContinue;
    }
    if (gemapi::time::set_wall_seconds(seconds) != gemapi::Status::ok) {
      gemapi::io::write("date: set failed\n");
      return CommandResult::kContinue;
    }
  }
  const auto now = gemapi::time::local_seconds();
  const auto zone = gemapi::time::timezone_minutes();
  if (!now || !zone) {
    gemapi::io::write("date: unavailable\n");
    return CommandResult::kContinue;
  }
  char text[48];
  const gemapi::usize cursor =
    format_unix_time(text, sizeof(text), now.value(), zone.value());
  gemapi::io::write(text, cursor);
  return CommandResult::kContinue;
}

CommandResult RunTz(const char* args, gemapi::usize args_length) {
  trim(args, args_length);
  if (text_equals(args, args_length, "save")) {
    const auto zone = gemapi::time::timezone_minutes();
    if (!zone) {
      gemapi::io::write("tz: unavailable\n");
      return CommandResult::kContinue;
    }
    auto file = gemapi::fs::File::open(
      kTimezonePath, gemapi::fs::kOpenWrite | gemapi::fs::kOpenCreate |
                       gemapi::fs::kOpenTruncate);
    if (!file) {
      gemapi::io::write("tz: save failed\n");
      return CommandResult::kContinue;
    }
    char text[16];
    gemapi::usize cursor = 0;
    gemapi::i64 minutes = zone.value();
    if (minutes < 0) {
      text[cursor] = '-';
      ++cursor;
      minutes = -minutes;
    }
    cursor = append_dec(text, sizeof(text), cursor,
                        static_cast<gemapi::u64>(minutes));
    text[cursor] = '\n';
    ++cursor;
    if (file.value().write(text, cursor) != gemapi::Status::ok) {
      gemapi::io::write("tz: save failed\n");
      return CommandResult::kContinue;
    }
    gemapi::io::write("tz: saved\n");
    return CommandResult::kContinue;
  }
  if (args_length != 0U) {
    const bool negative = args[0] == '-';
    const char* digits = args;
    gemapi::usize digits_length = args_length;
    if (negative || args[0] == '+') {
      ++digits;
      --digits_length;
    }
    gemapi::u64 magnitude = 0;
    if (!parse_u64(digits, digits_length, magnitude) ||
        magnitude > 840U) {
      gemapi::io::write("tz: usage: tz [-840..840]\n");
      return CommandResult::kContinue;
    }
    const gemapi::i64 minutes = negative ? -static_cast<gemapi::i64>(magnitude)
                                         : static_cast<gemapi::i64>(magnitude);
    if (gemapi::time::set_timezone_minutes(minutes) != gemapi::Status::ok) {
      gemapi::io::write("tz: failed\n");
      return CommandResult::kContinue;
    }
  }
  const auto zone = gemapi::time::timezone_minutes();
  if (!zone) {
    gemapi::io::write("tz: unavailable\n");
    return CommandResult::kContinue;
  }
  char text[24];
  gemapi::usize cursor = append_text(text, sizeof(text), 0, "tz: ");
  const gemapi::i64 minutes = zone.value();
  if (minutes == 0) {
    cursor = append_text(text, sizeof(text), cursor, "UTC\n");
  } else {
    const bool negative = minutes < 0;
    const gemapi::u64 absolute = static_cast<gemapi::u64>(
      negative ? -minutes : minutes);
    cursor = append_text(text, sizeof(text), cursor, negative ? "-" : "+");
    cursor = append_padded(text, sizeof(text), cursor, absolute / 60U, 2);
    cursor = append_text(text, sizeof(text), cursor, ":");
    cursor = append_padded(text, sizeof(text), cursor, absolute % 60U, 2);
    cursor = append_text(text, sizeof(text), cursor, "\n");
  }
  gemapi::io::write(text, cursor);
  return CommandResult::kContinue;
}

CommandResult RunPriority(const char* args, gemapi::usize args_length) {
  if (args_length > 0U) {
    gemapi::u64 value = 0;
    if (!parse_u64(args, args_length, value)) {
      gemapi::io::write("prio: invalid argument\n");
      return CommandResult::kContinue;
    }
    if (gemapi::process::set_priority(value) != gemapi::Status::ok) {
      gemapi::io::write("prio: failed\n");
      return CommandResult::kContinue;
    }
  }
  const auto current = gemapi::process::priority();
  if (!current) {
    gemapi::io::write("prio: unavailable\n");
    return CommandResult::kContinue;
  }
  char text[32];
  gemapi::usize cursor = append_text(text, sizeof(text), 0, "priority: ");
  cursor = append_dec(text, sizeof(text), cursor, current.value());
  cursor = append_text(text, sizeof(text), cursor, "\n");
  gemapi::io::write(text, cursor);
  return CommandResult::kContinue;
}

CommandResult RunPs(const char* /*args*/, gemapi::usize /*args_length*/) {
  cat_file("/proc/processes", "ps");
  return CommandResult::kContinue;
}

CommandResult RunId(const char* /*args*/, gemapi::usize /*args_length*/) {
  const auto uid = gemapi::process::user_id();
  const auto gid = gemapi::process::group_id();
  if (!uid || !gid) {
    gemapi::io::write("id: unavailable\n");
    return CommandResult::kContinue;
  }
  char text[48];
  gemapi::usize cursor = append_text(text, sizeof(text), 0, "uid: ");
  cursor = append_dec(text, sizeof(text), cursor, uid.value());
  cursor = append_text(text, sizeof(text), cursor, " gid: ");
  cursor = append_dec(text, sizeof(text), cursor, gid.value());
  cursor = append_text(text, sizeof(text), cursor, "\n");
  gemapi::io::write(text, cursor);
  return CommandResult::kContinue;
}

CommandResult RunSetId(const char* args, gemapi::usize args_length,
                       bool group) {
  trim(args, args_length);
  gemapi::u64 value = 0;
  if (!parse_u64(args, args_length, value)) {
    gemapi::io::write(group ? "setgid: usage: setgid <id>\n"
                            : "setuid: usage: setuid <id>\n");
    return CommandResult::kContinue;
  }
  const gemapi::Status status =
    group ? gemapi::process::set_group_id(value)
          : gemapi::process::set_user_id(value);
  if (status != gemapi::Status::ok) {
    gemapi::io::write(group ? "setgid: failed\n" : "setuid: failed\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunSetUid(const char* args, gemapi::usize args_length) {
  return RunSetId(args, args_length, false);
}

CommandResult RunSetGid(const char* args, gemapi::usize args_length) {
  return RunSetId(args, args_length, true);
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
  char path[128];
  gemapi::usize length = 0;
  const char* cursor = args;
  const char* end = args + args_length;
  if (!next_token(cursor, end, path, sizeof(path), length) ||
      length >= (sizeof(path) - 1U)) {
    gemapi::io::write("mkdir: missing or invalid path\n");
    return CommandResult::kContinue;
  }
  char extra[2];
  gemapi::usize extra_length = 0;
  if (next_token(cursor, end, extra, sizeof(extra), extra_length)) {
    gemapi::io::write("mkdir: usage: mkdir <path>\n");
    return CommandResult::kContinue;
  }
  if (gemapi::fs::mkdir(path) != gemapi::Status::ok) {
    gemapi::io::write("mkdir: failed\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunRm(const char* args, gemapi::usize args_length) {
  char path[128];
  gemapi::usize length = 0;
  const char* cursor = args;
  const char* end = args + args_length;
  if (!next_token(cursor, end, path, sizeof(path), length) ||
      length >= (sizeof(path) - 1U)) {
    gemapi::io::write("rm: missing or invalid path\n");
    return CommandResult::kContinue;
  }
  char extra[2];
  gemapi::usize extra_length = 0;
  if (next_token(cursor, end, extra, sizeof(extra), extra_length)) {
    gemapi::io::write("rm: usage: rm <path>\n");
    return CommandResult::kContinue;
  }
  const auto info = gemapi::fs::stat(path);
  if (!info) {
    gemapi::io::write("rm: failed\n");
    return CommandResult::kContinue;
  }
  const gemapi::Status status =
    info.value().kind == gemapi::fs::Kind::directory
      ? gemapi::fs::rmdir(path)
      : gemapi::fs::unlink(path);
  if (status != gemapi::Status::ok) {
    gemapi::io::write("rm: failed\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunMv(const char* args, gemapi::usize args_length) {
  char old_path[128];
  char new_path[128];
  gemapi::usize old_length = 0;
  gemapi::usize new_length = 0;
  const char* cursor = args;
  const char* end = args + args_length;
  if (!next_token(cursor, end, old_path, sizeof(old_path), old_length) ||
      !next_token(cursor, end, new_path, sizeof(new_path), new_length) ||
      old_length >= (sizeof(old_path) - 1U) ||
      new_length >= (sizeof(new_path) - 1U)) {
    gemapi::io::write("mv: usage: mv <old> <new>\n");
    return CommandResult::kContinue;
  }
  char extra[2];
  gemapi::usize extra_length = 0;
  if (next_token(cursor, end, extra, sizeof(extra), extra_length)) {
    gemapi::io::write("mv: usage: mv <old> <new>\n");
    return CommandResult::kContinue;
  }
  if (gemapi::fs::rename(old_path, new_path) != gemapi::Status::ok) {
    gemapi::io::write("mv: failed\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunChmod(const char* args, gemapi::usize args_length) {
  char mode_text[8];
  char path[128];
  gemapi::usize mode_length = 0;
  gemapi::usize path_length = 0;
  const char* cursor = args;
  const char* end = args + args_length;
  if (!next_token(cursor, end, mode_text, sizeof(mode_text), mode_length) ||
      !next_token(cursor, end, path, sizeof(path), path_length) ||
      mode_length >= (sizeof(mode_text) - 1U) ||
      path_length >= (sizeof(path) - 1U)) {
    gemapi::io::write("chmod: usage: chmod <mode> <path>\n");
    return CommandResult::kContinue;
  }
  char extra[2];
  gemapi::usize extra_length = 0;
  if (next_token(cursor, end, extra, sizeof(extra), extra_length)) {
    gemapi::io::write("chmod: usage: chmod <mode> <path>\n");
    return CommandResult::kContinue;
  }
  gemapi::u64 mode = 0;
  if (!parse_octal(mode_text, mode_length, mode)) {
    gemapi::io::write("chmod: invalid mode\n");
    return CommandResult::kContinue;
  }
  if (gemapi::fs::chmod(path, mode) != gemapi::Status::ok) {
    gemapi::io::write("chmod: failed\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunChown(const char* args, gemapi::usize args_length) {
  char uid_text[16];
  char gid_text[16];
  char path[128];
  gemapi::usize uid_length = 0;
  gemapi::usize gid_length = 0;
  gemapi::usize path_length = 0;
  const char* cursor = args;
  const char* end = args + args_length;
  if (!next_token(cursor, end, uid_text, sizeof(uid_text), uid_length) ||
      !next_token(cursor, end, gid_text, sizeof(gid_text), gid_length) ||
      !next_token(cursor, end, path, sizeof(path), path_length) ||
      uid_length >= (sizeof(uid_text) - 1U) ||
      gid_length >= (sizeof(gid_text) - 1U) ||
      path_length >= (sizeof(path) - 1U)) {
    gemapi::io::write("chown: usage: chown <uid> <gid> <path>\n");
    return CommandResult::kContinue;
  }
  char extra[2];
  gemapi::usize extra_length = 0;
  if (next_token(cursor, end, extra, sizeof(extra), extra_length)) {
    gemapi::io::write("chown: usage: chown <uid> <gid> <path>\n");
    return CommandResult::kContinue;
  }
  gemapi::u64 uid = 0;
  gemapi::u64 gid = 0;
  if (!parse_u64(uid_text, uid_length, uid) ||
      !parse_u64(gid_text, gid_length, gid)) {
    gemapi::io::write("chown: invalid uid or gid\n");
    return CommandResult::kContinue;
  }
  if (gemapi::fs::chown(path, uid, gid) != gemapi::Status::ok) {
    gemapi::io::write("chown: failed\n");
  }
  return CommandResult::kContinue;
}

CommandResult RunMount(const char* args, gemapi::usize args_length) {
  char type[16];
  char path[128];
  gemapi::usize type_length = 0;
  gemapi::usize path_length = 0;
  const char* cursor = args;
  const char* end = args + args_length;
  if (!next_token(cursor, end, type, sizeof(type), type_length) ||
      !next_token(cursor, end, path, sizeof(path), path_length) ||
      type_length >= (sizeof(type) - 1U) ||
      path_length >= (sizeof(path) - 1U)) {
    gemapi::io::write("mount: usage: mount <type> <path>\n");
    return CommandResult::kContinue;
  }
  char extra[2];
  gemapi::usize extra_length = 0;
  if (next_token(cursor, end, extra, sizeof(extra), extra_length)) {
    gemapi::io::write("mount: usage: mount <type> <path>\n");
    return CommandResult::kContinue;
  }
  if (gemapi::fs::mount(type, path) != gemapi::Status::ok) {
    gemapi::io::write("mount: failed\n");
    return CommandResult::kContinue;
  }
  gemapi::io::write("mount: mounted\n");
  return CommandResult::kContinue;
}

CommandResult RunUmount(const char* args, gemapi::usize args_length) {
  char path[128];
  gemapi::usize length = 0;
  const char* cursor = args;
  const char* end = args + args_length;
  if (!next_token(cursor, end, path, sizeof(path), length) ||
      length >= (sizeof(path) - 1U)) {
    gemapi::io::write("umount: usage: umount <path>\n");
    return CommandResult::kContinue;
  }
  char extra[2];
  gemapi::usize extra_length = 0;
  if (next_token(cursor, end, extra, sizeof(extra), extra_length)) {
    gemapi::io::write("umount: usage: umount <path>\n");
    return CommandResult::kContinue;
  }
  if (gemapi::fs::umount(path) != gemapi::Status::ok) {
    gemapi::io::write("umount: failed\n");
    return CommandResult::kContinue;
  }
  gemapi::io::write("umount: unmounted\n");
  return CommandResult::kContinue;
}

constexpr Command kCommands[] = {
  {"help", &RunHelp},
  {"echo", &RunEcho},
  {"cat", &RunCat},
  {"ls", &RunList},
  {"stat", &RunStat},
  {"clear", &RunClear},
  {"acpi", &RunAcpi},
  {"uptime", &RunUptime},
  {"date", &RunDate},
  {"tz", &RunTz},
  {"sleep", &RunSleep},
  {"ps", &RunPs},
  {"prio", &RunPriority},
  {"id", &RunId},
  {"setuid", &RunSetUid},
  {"setgid", &RunSetGid},
  {"dmesg", &RunDmesg},
  {"loglevel", &RunLogLevel},
  {"mkdir", &RunMkdir},
  {"rm", &RunRm},
  {"mv", &RunMv},
  {"chmod", &RunChmod},
  {"chown", &RunChown},
  {"mount", &RunMount},
  {"umount", &RunUmount},
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
