---
title: "GitHub Actions: Docker Build + Push"
type: concept
sources:
  - raw/docs/github-actions-docker-workflow.md
related:
  - wiki/concepts/docker-build-strategy.md
  - wiki/concepts/docker-tag-strategy.md
confidence: high
updated: 2026-05-23
---

# GitHub Actions: Docker Build + Push

CI/CD automation for building and pushing `xolegator/pinba-engine` images to Docker Hub.

## Required Secrets

Set in GitHub: **Settings → Secrets and variables → Actions**

| Secret | Value |
|--------|-------|
| `DOCKERHUB_USERNAME` | `xolegator` |
| `DOCKERHUB_TOKEN` | Docker Hub Personal Access Token (not password) |

Generate token at hub.docker.com/settings/security → New Access Token.

## Workflow Design

Two separate jobs (not a matrix) because each Dockerfile has a different tag scheme:
- MySQL 8.0: `8.0`, `latest`, `8.0-v{version}`
- MySQL 8.4: `8.4-lts`, `8.4-lts-v{version}`

A shared matrix with `docker/metadata-action` would require complex template gymnastics.
Separate jobs with explicit tag lists are simpler and easier to read.

Trigger: **tag push only** (`v*`). Branch pushes do not push to Docker Hub.

## Workflow File

Place at `.github/workflows/docker-push.yml`:

```yaml
name: Docker Build and Push

on:
  push:
    tags:
      - 'v*'

jobs:
  build-mysql80:
    name: Build MySQL 8.0 image
    runs-on: ubuntu-latest
    permissions:
      contents: read
    steps:
      - uses: actions/checkout@v4

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Log in to Docker Hub
        uses: docker/login-action@v3
        with:
          username: ${{ secrets.DOCKERHUB_USERNAME }}
          password: ${{ secrets.DOCKERHUB_TOKEN }}

      - name: Extract version from tag
        id: version
        run: echo "version=${GITHUB_REF_NAME#v}" >> "$GITHUB_OUTPUT"

      - name: Build and push MySQL 8.0
        uses: docker/build-push-action@v6
        with:
          context: .
          file: Dockerfile.mysql80
          push: true
          tags: |
            ${{ secrets.DOCKERHUB_USERNAME }}/pinba-engine:8.0
            ${{ secrets.DOCKERHUB_USERNAME }}/pinba-engine:latest
            ${{ secrets.DOCKERHUB_USERNAME }}/pinba-engine:8.0-v${{ steps.version.outputs.version }}
          cache-from: type=gha,scope=mysql80
          cache-to: type=gha,mode=max,scope=mysql80

  build-mysql84:
    name: Build MySQL 8.4 LTS image
    runs-on: ubuntu-latest
    permissions:
      contents: read
    steps:
      - uses: actions/checkout@v4

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Log in to Docker Hub
        uses: docker/login-action@v3
        with:
          username: ${{ secrets.DOCKERHUB_USERNAME }}
          password: ${{ secrets.DOCKERHUB_TOKEN }}

      - name: Extract version from tag
        id: version
        run: echo "version=${GITHUB_REF_NAME#v}" >> "$GITHUB_OUTPUT"

      - name: Build and push MySQL 8.4 LTS
        uses: docker/build-push-action@v6
        with:
          context: .
          file: Dockerfile.mysql84
          push: true
          tags: |
            ${{ secrets.DOCKERHUB_USERNAME }}/pinba-engine:8.4-lts
            ${{ secrets.DOCKERHUB_USERNAME }}/pinba-engine:8.4-lts-v${{ steps.version.outputs.version }}
          cache-from: type=gha,scope=mysql84
          cache-to: type=gha,mode=max,scope=mysql84
```

## How Versioning Works

On `git tag v2.0.1 && git push --tags`:
- `GITHUB_REF_NAME` = `v2.0.1`
- `${GITHUB_REF_NAME#v}` strips the `v` prefix → `2.0.1`
- Tags pushed: `8.0`, `latest`, `8.0-v2.0.1`, `8.4-lts`, `8.4-lts-v2.0.1`

## BuildKit Cache

Each Dockerfile gets its own GHA cache scope (`scope=mysql80`, `scope=mysql84`) so they
don't evict each other. On subsequent runs, unchanged layers (especially the large MySQL
source download) are served from cache → significantly faster builds.

## Action Versions

| Action | Version | Notes |
|--------|---------|-------|
| `actions/checkout` | v4 | Current stable |
| `docker/setup-buildx-action` | v3 | Current stable |
| `docker/login-action` | v3 | Current stable |
| `docker/build-push-action` | v6 | Current stable (2024-2025) |

Do not use `latest` in action refs — pin to major versions for reproducibility.

See: [[docker-build-strategy]], [[docker-tag-strategy]]
