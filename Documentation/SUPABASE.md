# Optional Supabase setup and validation

The application works offline with local licensed WAV/AIFF/FLAC files. No project
URL or credential is bundled. No Supabase project was available for this delivery;
client tests use an injected in-memory transport. The migration and owner/RLS/revision/storage-path policies also passed a real disposable PostgreSQL 17 test with fixture auth/storage schemas; no hosted service was used.

## Owner setup

1. Create your Supabase project. Apply `Supabase/migrations/0001_library_sync.sql`
   through your project's SQL migration workflow. Review it before execution.
   It creates owner-scoped `sample_metadata`, a revision-checked RPC, and the
   private `jaspro-private` Storage bucket. The reserved app bucket is forced private with the specified size/MIME limits even if
   it already exists. No commercial sample catalogue is created.
2. In Authentication, configure your intended email/password policy and create
   or invite test accounts through Supabase. This desktop client signs into
   existing accounts; it does not implement signup, recovery, OAuth or MFA flows.
   Keep email confirmation enabled as appropriate for your deployment.
3. Configure **SUPABASE_URL** (your HTTPS project origin, with no path/query) and
   **SUPABASE_PUBLISHABLE_KEY** in environment settings. Leave both unset until
   your project exists. The publishable key is intended for public clients and
   does not replace RLS. Never use a service-role key in the app. Never paste
   passwords, refresh tokens, signing credentials or service-role keys into chat.
4. For restricted egress, add your exact project hostname in environment settings.
   Preserve existing allowlist entries. No host addition was saved without a
   known project URL. Restart the app after setting its process environment.
5. Use Cloud / Account to sign in. Passwords and session tokens are kept in
   memory; tokens are not stored in presets, projects, SQLite or logs. Refresh
   explicitly when the session expires. Signing out clears the local session,
   including when the logout request fails. Restarting requires a fresh sign-in.

## Sync behavior

Local source audio and metadata remain usable independently of the network.
`Push local metadata` copies instrument/genre/mood/collection, favorite, corrected
BPM and key into a persistent per-account outbox keyed by the source SHA-256. It
then sends expected revisions through `sync_sample`. Source paths are not sent.
The server derives ownership from the authenticated UID, not a client owner field.
A successful push increments the server revision. Partial acknowledgements are
persisted; unacknowledged writes survive an offline restart.

Pulls are paged full-table reads rather than a transactionally consistent snapshot;
repeat sync after concurrent insertions. This is explicit sync, not real-time replication.

A revision conflict stops sync and retains pending edits. It never silently
replaces the remote revision. `Use remote metadata` explicitly discards pending
outbox writes and the cached remote view, pulls the account's metadata and applies it to matching local source
hashes. It is a remote-wins operation and may replace local annotations. On an
initial account sync, pull before pushing if another machine already has records.
Local metadata edits are sent only on explicit Push, not automatically after each
keystroke. The protocol supports deletion tombstones; the current UI does not
provide remote deletion. Collections are single user tags, not ordered playlists.

Audio upload is separate and opt-in. Select a source in Library, select Cloud,
confirm ownership/upload rights and click Upload. Uploads are limited to 256 MiB,
verify the content hash, and use private `UID/hash/filename` paths. Binary downloads
verify SHA-256 before returning bytes. The client has a download abstraction;
remote audio browsing/downloading is not exposed in this UI. Existing objects are
not overwritten by uploads. Storage and metadata may incur your provider's costs.

The desktop transport uses HTTPS with normal TLS verification, finite connection
timeouts, bounded responses and no redirects carrying authentication headers.
Linux uses libcurl with the inherited cloud proxy and CA trust. No certificate
verification is disabled. Requests and response bodies containing credentials are
not logged. The provider interface is available for future authorized commercial
API adapters; none is implemented or advertised.

## Required live integration validation (not yet executed)

After deploying to a disposable project with two test accounts:

- Sign in as account A, push local metadata, restart, sign in and pull it onto a
  second local library containing the same source hashes. Confirm no local paths
  or raw audio were sent by metadata sync.
- Sign in as B and verify A's metadata is absent and direct REST/table/storage
  access to A's UUID is denied. Test both authenticated roles and anonymous access.
- Verify the private bucket rejects public reads, cross-owner insert/select/delete,
  unauthorized MIME types and objects over its configured size limit.
- Create simultaneous edits on two devices. Confirm stale expected revisions fail,
  pending changes survive, and explicit remote-wins resolution works.
- Interrupt a push between acknowledgements; restart and retry. Confirm failures
  retain pending records and acknowledgements advance only the matching hash.
- Test expiration, refresh, invalid passwords, revoked sessions, offline startup,
  account switching and malformed/oversized responses. Confirm no token reaches
  project/library/log files and no account reuses another account's outbox.
- Upload a licensed small fixture with consent; download it through the client
  abstraction and verify the hash. Verify absent consent makes no upload request.

Mocked client tests establish protocol/state behavior only. They do not establish
that PostgreSQL migrations, RLS, GoTrue auth or protected Storage work in a live
project. Do not mark cloud acceptance complete until those tests pass.

## Executed local database integration

`Tests/test_supabase_rls.py` initializes a temporary PostgreSQL cluster, uses only
a private Unix socket, creates fixture auth/storage contracts, applies the migration
twice and runs `Tests/supabase_rls.sql`. It verifies idempotence, bucket privacy/limits,
owner reads, blocked cross-owner metadata/storage reads/writes/deletes, anonymous
access denial, revision increments, stale revisions, invalid hashes and tombstones.
The server is stopped and its data removed afterwards. No persistent database
service, credentials or Supabase project are needed.

Enable this optional CTest suite with `WINGMAN_TEST_POSTGRES_BIN` and
`WINGMAN_TEST_POSTGRES_SHARE` pointing to an installed PostgreSQL's bin/share
directories. The managed environment installer configures its retained local tools.
The auth/storage fixtures do not implement JWT validation, GoTrue, Storage HTTP,
MIME enforcement or provider deployment configuration; those require the live tests
above. This local test is not proof of hosted Supabase readiness.
