#include "gemapi/fs.h"
#include "gemapi/io.h"

namespace {

bool SameText(const char* lhs, gemapi::usize length, const char* rhs) {
  gemapi::usize index = 0;
  while (index < length && rhs[index] != '\0') {
    if (lhs[index] != rhs[index]) {
      return false;
    }
    ++index;
  }
  return index == length && rhs[index] == '\0';
}

const char* Field(const char* line, gemapi::usize length, gemapi::u32 index,
                  gemapi::usize& field_length) {
  gemapi::usize start = 0;
  gemapi::u32 current = 0;
  for (gemapi::usize position = 0; position <= length; ++position) {
    if (position == length || line[position] == '\t') {
      if (current == index) {
        field_length = position - start;
        return line + start;
      }
      ++current;
      start = position + 1U;
    }
  }
  field_length = 0;
  return nullptr;
}

void PrintField(const char* label, const char* value,
                gemapi::usize length) {
  gemapi::usize label_length = 0;
  while (label[label_length] != '\0') {
    ++label_length;
  }
  gemapi::io::write(label, label_length);
  gemapi::io::write(value, length);
  gemapi::io::write("\n");
}

}  // namespace

int main(int argc, char** argv, char** /*envp*/) {
  auto file = gemapi::fs::File::open("/system/packages/registry");
  if (!file) {
    gemapi::io::write("pkg: registry unavailable\n");
    return 1;
  }
  char data[4096];
  gemapi::usize used = 0;
  for (;;) {
    auto count = file.value().read(data + used, sizeof(data) - used - 1U);
    if (!count) {
      gemapi::io::write("pkg: read failed\n");
      return 1;
    }
    if (count.value() == 0U) {
      break;
    }
    used += count.value();
    if (used >= (sizeof(data) - 1U)) {
      break;
    }
  }
  data[used] = '\0';

  if (argc == 1 || (argc == 2 && SameText(argv[1], 4U, "list"))) {
    gemapi::io::write("name version author\n");
    gemapi::usize start = 0;
    while (start < used) {
      gemapi::usize end = start;
      while (end < used && data[end] != '\n') {
        ++end;
      }
      if (end > start) {
        gemapi::usize name_length = 0;
        gemapi::usize version_length = 0;
        gemapi::usize author_length = 0;
        const char* name = Field(data + start, end - start, 0, name_length);
        const char* version = Field(data + start, end - start, 1, version_length);
        const char* author = Field(data + start, end - start, 2, author_length);
        if (name != nullptr && version != nullptr && author != nullptr) {
          gemapi::io::write(name, name_length);
          gemapi::io::write(" ");
          gemapi::io::write(version, version_length);
          gemapi::io::write(" ");
          gemapi::io::write(author, author_length);
          gemapi::io::write("\n");
        }
      }
      start = end + 1U;
    }
    return 0;
  }

  if (argc == 3 && SameText(argv[1], 4U, "info")) {
    gemapi::usize start = 0;
    while (start < used) {
      gemapi::usize end = start;
      while (end < used && data[end] != '\n') {
        ++end;
      }
      gemapi::usize name_length = 0;
      const char* name = Field(data + start, end - start, 0, name_length);
      if (name != nullptr &&
          SameText(name, name_length, argv[2])) {
        gemapi::usize length = 0;
        PrintField("name: ", Field(data + start, end - start, 0, length),
                   length);
        PrintField("version: ", Field(data + start, end - start, 1, length),
                   length);
        PrintField("author: ", Field(data + start, end - start, 2, length),
                   length);
        PrintField("description: ",
                   Field(data + start, end - start, 3, length), length);
        PrintField("exec: ", Field(data + start, end - start, 4, length),
                   length);
        return 0;
      }
      start = end + 1U;
    }
    gemapi::io::write("pkg: not found\n");
    return 1;
  }

  gemapi::io::write("pkg: usage: pkg list | pkg info <name>\n");
  return 1;
}
