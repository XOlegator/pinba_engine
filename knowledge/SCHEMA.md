# LLM Knowledge Base — Schema & Instructions

**For AI agents working in this repository**: this file describes how the knowledge base
in `knowledge/` works and what you must do when operating on it.

---

## Directory Layout

```
knowledge/
├── SCHEMA.md          ← this file; LLM reads it first, never modifies it
├── raw/               ← IMMUTABLE: human drops source documents here
│   ├── articles/      ← web articles (.md, clipped via Obsidian Web Clipper)
│   ├── repos/         ← notes/excerpts from related repos and projects
│   ├── docs/          ← official documentation excerpts, specs, man pages
│   └── assets/        ← images referenced from articles
├── wiki/              ← LLM-COMPILED: never manually edited
│   ├── index.md       ← master catalog of all wiki pages (auto-maintained)
│   ├── log.md         ← activity timeline: every ingest/query/lint logged here
│   ├── overview.md    ← high-level synthesis of the entire knowledge base
│   ├── concepts/      ← concept articles (one topic = one file)
│   ├── sources/       ← one summary page per raw/ document
│   └── comparisons/   ← side-by-side comparison articles
└── output/            ← ephemeral query outputs (gitignored)
```

**raw/ is read-only for agents.** Never create, edit, or delete files in raw/.
**wiki/ is write-only for humans.** Humans do not edit wiki/ directly; only agents do.

---

## Frontmatter for all wiki/ pages

Every wiki page must have this YAML frontmatter:

```yaml
---
title: "Human-readable title"
type: concept | source | comparison | overview | index | log
sources:
  - raw/articles/filename.md    # which raw docs this page draws from
related:
  - wiki/concepts/other-page.md  # cross-links to related pages
confidence: high | medium | low   # how confident the LLM is in the content
updated: YYYY-MM-DD
---
```

---

## Workflows

### INGEST — process a new document from raw/

Trigger: "ingest raw/articles/foo.md" or "process the new files in raw/"

Steps:
1. Read the raw document fully.
2. Create `wiki/sources/{slug}.md` — a structured summary: key facts, quotes, relevance.
3. Update or create `wiki/concepts/*.md` pages for each new concept introduced.
4. Add [[wiki-links]] to connect related pages (use `[[concept-name]]` notation).
5. Update `wiki/index.md` — add the new source and any new concept pages.
6. Append an entry to `wiki/log.md`: date, what was ingested, which pages were touched.
7. Optionally update `wiki/overview.md` if the new source significantly changes the picture.

### QUERY — answer a question using the wiki

Trigger: "what does the wiki say about X?" or any open question

Steps:
1. Read `wiki/index.md` to find relevant pages.
2. Read all relevant concept and source pages.
3. Synthesize an answer.
4. If the answer reveals new knowledge worth keeping: write it to `output/{topic}-{date}.md`.
5. Ask the user if they want to file the output back into the wiki as a new concept page.

### LINT — health check the wiki

Trigger: "lint the wiki" or "check the knowledge base"

Steps:
1. Read all wiki/ pages.
2. Report: orphan pages (no incoming links), contradictions between pages, stale confidence scores.
3. Suggest new concept pages that are referenced but don't exist yet.
4. Suggest new questions to investigate based on gaps.
5. Append a lint report to `wiki/log.md`.

---

## Naming Conventions

- raw files: keep original name or use descriptive kebab-case slugs.
- wiki/sources/: `{source-slug}.md` matching the raw filename.
- wiki/concepts/: `{concept-name}.md` in kebab-case (e.g., `mysql-plugin-abi.md`).
- wiki/comparisons/: `{thing-a}-vs-{thing-b}.md`.
- output/: `{topic}-{YYYY-MM-DD}.md`.

---

## Cross-references

Use `[[page-name]]` wikilinks throughout wiki pages (Obsidian-compatible).
The index.md should contain a full link list. The LLM must update index.md on every change.

---

## What belongs in raw/ (examples for this project)

- MySQL Plugin API documentation articles
- Pinba UDP protocol specification
- Related GitHub repos: original pinba_engine, PHP extension, Pinboard
- Articles about MySQL storage engines
- Performance monitoring approaches for PHP
- Docker image tagging best practices articles
- Any MySQL 8.0/8.4 migration notes

---

## Language Policy

**All wiki/ content must be written in English.** This includes page titles, body text,
frontmatter fields, table headings, code comments inside wiki prose, and log entries.

Russian (or any other language) must not appear in any file under wiki/.
When translating existing Russian content, translate accurately — do not summarize.
Preserve all technical detail.

---

## Revision / Audit Procedure

Trigger: "audit the wiki", "revision", or when a significant batch of changes has landed in the repo.

Steps:
1. Read all wiki/ pages.
2. Cross-check facts against the actual codebase state:
   - Workflow files in `.github/workflows/`
   - Configuration files: `debian/control`, `debian/rules`, `.github/mysql-versions.json`
   - Git log for recent commits that may have superseded documented behavior
3. Identify and fix:
   - Stale facts (behavior that has since changed)
   - Contradictions between pages
   - Duplicated information across pages
   - Russian (or non-English) content that must be translated
   - Pages listed in `index.md` that no longer exist or have wrong descriptions
   - References to future work that is now implemented
4. Update `updated:` frontmatter dates on every modified page.
5. Append an audit entry to `wiki/log.md` summarising what was stale, what changed.
6. Update `wiki/index.md` if any pages were added, removed, or had their key topic change.
7. Optionally update `wiki/overview.md` if the high-level picture changed.

---

## What does NOT belong in wiki/ (even if tempting)

- Code. Code lives in src/. Wiki describes concepts, not implementations.
- Task lists or in-progress work. That's for git issues/PRs.
- Credentials, server-specific paths, or personal configs.
- Ephemeral debugging notes. Those go in output/, not wiki/.
- Non-English text.

---

## Local Environment Policy

**Never embed local filesystem paths in wiki/ content.** The knowledge base must be
portable across all developers and environments.

Forbidden patterns:
- Absolute paths on a developer's machine (e.g., `/mnt/projects/sites/pinba_engine/`)
- Paths that reveal a specific OS or user home directory (e.g., `/home/alice/`, `C:\Users\bob\`)
- Hostnames of local dev servers (e.g., `www.myproject.local`, `pinboard-new.local`)
- Environment-specific credentials or IP addresses

Instead, refer to:
- Repository-relative paths: `knowledge/wiki/concepts/`, `src/`, `.github/workflows/`
- GitHub repository URLs: `github.com/XOlegator/pinba_engine`
- Project or service names: "the Pinboard project", "XOlegator/pinba_extension repository"
- Standard OS paths that are part of the documented software (e.g., `/usr/lib/php/`, `/etc/php/`)

This rule applies to all files under `wiki/`: concepts, sources, comparisons, index, log, and overview.
