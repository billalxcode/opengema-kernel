# Firecrawl Workflow

Simpan di `.firecrawl/`, baca dengan `jq`.

```sh
firecrawl search "QEMU virt aarch64 memory map" -o .firecrawl/search-virt.json --json
jq -r '.data.web[].url' .firecrawl/search-virt.json
firecrawl scrape https://example.com/docs -o .firecrawl/scrape.json --json
firecrawl map https://example.com/docs -o .firecrawl/map.json --json
firecrawl crawl https://example.com/docs --max-pages 20 -o .firecrawl/crawl.json --json
```

Developer search untuk kode nyata:

```sh
firecrawl search "PL011 init QEMU" --categories developer -o .firecrawl/dev.json --json
jq -r '.data.developer[] | .url, .description' .firecrawl/dev.json
```

Hormati `FIRECRAWL_NO_SEARCH_FEEDBACK=1`.
