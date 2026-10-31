# clang-format dan clang-tidy

## clang-format

```yaml
BasedOnStyle: LLVM
ColumnLimit: 100
IndentWidth: 2
PointerAlignment: Left
SortIncludes: true
```

## clang-tidy minimal

```
cert-*,cppcoreguidelines-*,modernize-*,
readability-*,bugprone-*,performance-*,
hicpp-*,clang-analyzer-*
```

## Tolak otomatis

- naked `new` atau `delete`.
- narrowing tanpa cast eksplisit.
- union tanpa `variant`.
- fungsi return `Status` tanpa `[[nodiscard]]`.
