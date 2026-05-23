# LLM Knowledge Base

This directory implements the [Karpathy LLM Wiki pattern](https://gist.github.com/karpathy/442a6bf555914893e9891c11519de94f):
source documents go into `raw/`, an LLM agent compiles them into `wiki/`, and everything
is browsable in Obsidian.

**Human role:** curate `raw/` sources, ask questions.  
**LLM role:** compile `wiki/`, maintain index and log, answer queries.

## Quick start

```
knowledge/
├── SCHEMA.md     ← LLM instructions (read this first)
├── raw/          ← DROP YOUR DOCUMENTS HERE (gitignored)
│   ├── articles/ ← web articles (.md via Obsidian Web Clipper)
│   ├── repos/    ← excerpts/notes from related repos
│   ├── docs/     ← official docs, specs, man pages
│   └── assets/   ← images referenced from articles
├── wiki/         ← LLM-compiled knowledge (tracked in git)
│   ├── index.md
│   ├── log.md
│   ├── overview.md
│   ├── concepts/
│   ├── sources/
│   └── comparisons/
└── output/       ← ephemeral query results (gitignored)
```

## How to use

1. **Add a source**: drop a `.md` file into `raw/articles/` (use Obsidian Web Clipper).
2. **Ingest**: ask the LLM agent: *"ingest raw/articles/your-file.md"*
3. **Query**: ask the LLM: *"what does the wiki know about MySQL plugin ABI?"*
4. **Browse**: open Obsidian vault at this `knowledge/` folder.

## Obsidian setup

- Open vault: point Obsidian at this `knowledge/` folder.
- Recommended plugins: **Dataview**, **Local Graph**, **Templater**.
- Web Clipper: install browser extension, set save path to `raw/articles/`.
- Image download: Obsidian → Settings → Files → set attachment folder to `raw/assets/`.

## Git policy

- `raw/` and `output/` are **gitignored** (see `.gitignore`).
- `wiki/` is **tracked in git** — compiled knowledge is public and versioned.
- `SCHEMA.md` and this `README.md` are tracked.
