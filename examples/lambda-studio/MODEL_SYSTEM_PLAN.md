# Lambda Studio Model System Plan

This document captures the plan for rebuilding model discovery, metadata, search, and download in `lambda-studio`.

It is intended to be a working implementation roadmap, not just a wishlist.

## Status

- Milestone A: implemented
- Milestone B: implemented
- Milestone C: partially implemented
- Milestone D: partially implemented
- Milestone E: partially implemented
- Milestone F: not started

Current state:

- local inventory is sourced from the Hugging Face cache and manual local folders
- Hugging Face search, repo browsing, and downloads are implemented
- normalized search results and repo GGUF file metadata are persisted in SQLite
- raw Hugging Face search payloads and repo tree payloads are now persisted too
- selected repository detail payloads and README snapshots are now persisted too
- cached/offline reads can fall back to the raw payload snapshots if normalized rows are missing
- the Models view now supports text search, author filtering, and server-backed sorting by downloads, likes, or recency

## Goals

- Detect local models reliably.
- Use a standard on-disk model cache instead of inventing another silo.
- Search Hugging Face models by name and other useful criteria.
- Download full model metadata and store it locally for fast browsing and offline use.
- Support GGUF-first local inference workflows cleanly.
- Scale into richer filtering, sorting, and model-management UX later.

## Current Problems

The current implementation has a few structural issues:

- Local inventory is incomplete.
  - `common_list_cached_models()` returns cached Hugging Face repo entries, but the app only surfaces entries with a concrete file path.
  - That means cached Hugging Face models can effectively disappear from the UI.
  - Relevant code:
    - [examples/lambda-studio/ModelManager.hpp](/Users/abdurrahmanavci/Projects/flux-v4/examples/lambda-studio/ModelManager.hpp:178)
    - [examples/lambda-studio/main.cpp](/Users/abdurrahmanavci/Projects/flux-v4/examples/lambda-studio/main.cpp:141)

- Model storage is pointed at an app-local folder:
  - [examples/lambda-studio/ModelManager.hpp](/Users/abdurrahmanavci/Projects/flux-v4/examples/lambda-studio/ModelManager.hpp:272)
  - That is not aligned with the Hugging Face cache layout already used by llama.cpp downloads.

- Search is too primitive.
  - Current Hugging Face search only fetches a small result set by text query and a few fields:
    - [examples/lambda-studio/ModelManager.hpp](/Users/abdurrahmanavci/Projects/flux-v4/examples/lambda-studio/ModelManager.hpp:193)
  - It does not support richer filtering, local persistence, or repo detail hydration.

- Backend orchestration does not scale.
  - The current worker model joins before starting the next operation:
    - [examples/lambda-studio/ModelManager.hpp](/Users/abdurrahmanavci/Projects/flux-v4/examples/lambda-studio/ModelManager.hpp:149)
  - That is not suitable for search, indexing, downloads, load/unload, and metadata refresh all living at once.

## Storage Strategy

### Canonical model-file store

Use the Hugging Face cache as the canonical store for downloaded model files.

Why:

- Hugging Face documents `HF_HOME` / `HF_HUB_CACHE` as the standard central local cache.
- llama.cpp already downloads Hugging Face models into that cache via `common_download_model(...)`.
- Reusing the same cache prevents duplicate downloads and keeps Lambda Studio aligned with the existing tooling stack.

This means:

- downloaded model binaries live in the Hugging Face cache
- Lambda Studio should read the cache directly for inventory
- Lambda Studio should not copy large model files into a separate app-owned model directory

### App-owned storage

Lambda Studio should still keep its own app data, but only for metadata and state.

Recommended app-owned storage:

- macOS:
  - `~/Library/Application Support/Lambda Studio/`

Store there:

- local SQLite catalog
- cached raw Hugging Face JSON
- download/job state
- user preferences for models
- app-specific indexes and search tables

Do not store the actual large GGUF binaries there.

### Other local AI apps

There is no single universal model storage directory shared by all local AI apps.

Examples:

- Hugging Face ecosystem tools standardize around the Hugging Face cache.
- Ollama uses its own `~/.ollama` directory on macOS.

So the best standard for Lambda Studio is:

- Hugging Face cache for Hugging Face-origin model files
- optional import/discovery of user-added directories later

## Proposed Architecture

Split the current `ModelManager` responsibilities into clearer services.

### 1. LocalModelIndexService

Responsibilities:

- enumerate local models from:
  - Hugging Face cache
  - optional user-added directories
- resolve cached repo entries into concrete GGUF files
- produce normalized local model instances
- refresh/install status

### 2. HfCatalogService

Responsibilities:

- search Hugging Face repositories
- fetch repo-level detail
- fetch file lists
- fetch and persist full metadata snapshots
- maintain freshness timestamps and revalidation policy

### 3. DownloadService

Responsibilities:

- queue downloads
- start/resume/cancel downloads
- surface progress and errors
- persist final file resolution into local catalog

### 4. ModelLoadService

Responsibilities:

- load/unload concrete GGUF file paths
- track active model
- report load success/failure
- coordinate with chat runtime requirements

### 5. ModelCatalogStore

Responsibilities:

- own SQLite access
- persist normalized model/search metadata
- support local filtering/search
- support offline browsing of previously seen models

## Data Model

We should keep both raw remote payloads and normalized searchable fields.

### Raw persisted payloads

Store raw JSON for:

- search result payloads
- repo detail payloads
- repo file lists
- model card / README content when available

Why:

- we preserve fidelity
- we can evolve normalized schema later
- we can debug discrepancies more easily

### Normalized tables

Suggested schema:

- `model_repos`
  - repo id
  - author
  - private/gated/disabled flags
  - pipeline tag
  - library name
  - likes
  - downloads
  - downloads all time
  - created at
  - last modified
  - sha
  - used storage

- `model_repo_tags`
  - repo id
  - tag

- `model_repo_card`
  - repo id
  - raw card data
  - extracted license
  - extracted languages
  - extracted base models
  - extracted summary/title

- `model_repo_files`
  - repo id
  - revision / sha
  - path
  - filename
  - size bytes
  - lfs/blob metadata if available
  - gguf metadata if available
  - downloadability flags

- `local_model_instances`
  - local file path
  - repo id if known
  - revision if known
  - source kind (`hf_cache`, `user_folder`, `manual_file`)
  - discovered at
  - last verified at
  - size bytes
  - installed / missing / broken

- `download_jobs`
  - job id
  - repo id
  - file path
  - revision
  - status
  - progress
  - started at / finished at
  - error

- `search_cache_entries`
  - query
  - filters
  - timestamp
  - raw response payload

## Metadata To Capture

We should store as much useful model metadata locally as we can get.

### Repo-level metadata

- model id
- author / org
- sha
- createdAt
- lastModified
- likes
- downloads
- downloadsAllTime
- private
- gated
- disabled
- pipeline_tag
- library_name
- tags
- usedStorage

### Card/config metadata

- `cardData`
- `config`
- model card / README content
- `model-index`
- `baseModels`
- `transformersInfo`
- widget/sample metadata when available

### File-level metadata

- repo file path
- filename
- size
- LFS metadata
- GGUF-specific metadata when present
- file security / scan info if available

### Local/runtime metadata

- concrete local path
- discovered source
- currently loaded state
- selected-by-user state
- last successful load
- local verification status

## Hugging Face Query Strategy

Use a two-stage fetch model.

### Stage 1: Search/listing

Use Hugging Face model search for compact list results:

- text search
- author/org
- filter set
- sort
- limit

This stage feeds:

- list views
- quick search
- pagination / load more

### Stage 2: Repo hydration

For selected or high-value results, fetch deeper repo information:

- full repo info
- file list / siblings
- card data
- config
- README / model card
- GGUF file details

This stage feeds:

- details panel
- download picker
- local metadata cache

## Search and Filtering

### Phase 1 filters

- name / repo id substring
- author / organization
- GGUF only
- public / gated
- downloads
- likes
- last modified
- file size range

### Phase 2 filters

- pipeline tag
- library name
- quantization tag
- parameter size tags
- language
- license
- base model family

### Local search

Back local search with SQLite FTS over:

- repo id
- author
- tags
- extracted card title / summary
- README text
- filenames

That gives us:

- instant re-search
- offline browsing
- richer filtering without re-hitting the network for every keystroke

## Download Flow

The intended user flow:

1. search model repos
2. open repo details
3. inspect GGUF files
4. select a file
5. start download job
6. download into the Hugging Face cache
7. refresh local index
8. surface the concrete local file as loadable

Requirements:

- detect already-cached files
- avoid duplicate downloads
- support resume
- persist progress and errors
- support offline metadata if already cached

## Local Model Discovery

We should detect local models from multiple sources.

### Primary source

- Hugging Face cache

This is the main source for downloaded models.

### Secondary sources

- user-configured directories
- manually imported `.gguf` files

These should be indexed as additional sources, not ignored.

## AppState / Event Model Changes

The current `AppState` is too small for real model management:

- [examples/lambda-studio/AppState.hpp](/Users/abdurrahmanavci/Projects/flux-v4/examples/lambda-studio/AppState.hpp:23)

We will need to add:

- remote search query state
- active filters
- remote search results
- selected repo id
- selected file id/path
- repo detail payload/state
- download jobs
- local metadata freshness
- error/status per operation

Likewise, `ModelManagerEvent` should grow into a fuller event set:

- local inventory refreshed
- remote search ready
- repo detail ready
- repo files ready
- download progress
- download done
- download error
- metadata refresh ready
- active model loaded/unloaded

## Implementation Roadmap

### Milestone A: Fix Local Inventory

Scope:

- inventory from Hugging Face cache
- surface concrete local GGUF paths
- keep optional manual folder scan
- make local models reliably appear in the Models module

Deliverables:

- `LocalModelIndexService`
- corrected local path resolution
- proper installed models list

Status:

- Implemented in the current codebase.
- Local inventory now resolves concrete GGUF files from the Hugging Face cache first, includes the explicit `LLAMA_MODEL_PATH` when it points at a GGUF file, and keeps `~/.lambda-studio/models` as a secondary manual folder.

### Milestone B: Add Hugging Face Search MVP

Scope:

- search by text
- repo list results
- basic metadata display
- repo file list
- GGUF file picker

Deliverables:

- `HfCatalogService`
- search UI state
- repo detail state

Status:

- Implemented in the current codebase.
- The Models module can now:
  - search Hugging Face model repos by query
  - browse matching repositories
  - hydrate richer metadata for the selected repository
  - inspect GGUF files for the selected repo
  - show richer search metadata such as author, pipeline tag, downloads, likes, tags, and cache state

### Milestone C: Add Download Jobs

Scope:

- queue downloads
- progress state
- success/error states
- cache refresh after completion

Deliverables:

- `DownloadService`
- persisted job model
- proper progress events

Status:

- Partially implemented in the current codebase.
- Current state:
  - users can trigger downloads for GGUF files from the Models module
  - cached files are detected and surfaced
  - download completion/error state is wired into app state
  - download attempts are now persisted into a local job history with running/completed/failed states
  - recent download history is shown in the Models view and restored on launch
- Still missing:
  - resumable/retryable download jobs
  - explicit progress reporting in the UI

### Milestone D: Add Local Catalog Database

Scope:

- SQLite-backed normalized catalog
- raw payload persistence
- offline model browsing
- local search index

Deliverables:

- `ModelCatalogStore`
- schema versioning
- migration path

Status:

- Partially implemented in the current codebase.
- Current state:
  - Lambda Studio now persists remote search results and repo-file metadata into a local SQLite catalog.
  - Selected repository detail snapshots and README content are also persisted in the catalog.
  - Search requests can preload cached results before the network refresh completes.
  - Repo-file requests can preload cached GGUF file metadata before the network refresh completes.
  - Repo-detail requests can preload cached metadata before the network refresh completes.
- Still missing:
  - schema versioning and migrations
  - offline/local search over the full catalog
  - local model instance persistence in the catalog

### Milestone E: Add Rich Filters and Sorting

Scope:

- local filter UI
- remote filter UI
- reusable filter state
- richer repo/file metadata views

Status:

- Partially implemented in the current codebase.
- Current state:
  - the remote Hugging Face browser supports text query, author/org filtering, and sort order selection
  - search caching now keys off the full search request instead of only the query text
  - stale search responses no longer overwrite the active filtered result set
- Still missing:
  - local catalog filtering/search over the full stored metadata
  - richer remote filters such as license, size range, gated/public state, and language
  - reusable filter chips / saved searches in the UI

### Milestone F: Polish and Hardening

Scope:

- retry/resume policies
- cancellation
- stale metadata refresh
- deduplication
- corrupted cache detection
- better load-path selection when a repo has multiple GGUFs

## Recommended Immediate Next Step

Start with Milestone A before building any more UI around models.

Reason:

- the current “no local models found” issue likely comes from inventory resolution, not lack of files on disk
- search/download UX will be much easier to build once “installed model” semantics are correct
- Hugging Face cache alignment should be established before we persist more app-specific assumptions

Concrete first slice:

1. replace `modelsDir()` as the primary source of truth
2. inspect Hugging Face cache layout directly
3. resolve cached repos into concrete GGUF file paths
4. surface those paths in `AppState.localModels`
5. confirm the existing Models screen can load them

## Source Notes

These sources informed the storage and metadata strategy:

- Hugging Face cache and environment docs:
  - [Manage cache](https://huggingface.co/docs/huggingface_hub/main/guides/manage-cache)
  - [Environment variables](https://huggingface.co/docs/huggingface_hub/v1.1.2/en/package_reference/environment_variables)

- Hugging Face API/library references for model search and model info:
  - [HfApi client](https://huggingface.co/docs/huggingface_hub/v0.23.5/en/package_reference/hf_api)
  - [Search the Hub](https://huggingface.co/docs/huggingface_hub/v0.17.1/guides/search)

- Ollama macOS storage reference:
  - [Ollama macOS docs](https://docs.ollama.com/macos)

- llama.cpp download/cache integration in this repo:
  - [vendor/llama.cpp/common/download.h](/Users/abdurrahmanavci/Projects/flux-v4/vendor/llama.cpp/common/download.h:53)
  - [vendor/llama.cpp/common/download.cpp](/Users/abdurrahmanavci/Projects/flux-v4/vendor/llama.cpp/common/download.cpp:713)
