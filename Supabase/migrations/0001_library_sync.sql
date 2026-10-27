-- Run in the owner's Supabase project. No service-role key belongs in the app.
begin;
create table if not exists public.sample_metadata (
  owner_id uuid not null references auth.users(id) on delete cascade,
  content_hash text not null check(content_hash ~ '^[0-9a-f]{64}$'),
  metadata jsonb not null default '{}'::jsonb check(jsonb_typeof(metadata) = 'object' and octet_length(metadata::text) <= 16384),
  revision bigint not null default 1 check(revision > 0),
  deleted boolean not null default false,
  updated_at timestamptz not null default now(),
  primary key(owner_id, content_hash)
);
alter table public.sample_metadata enable row level security;
drop policy if exists metadata_owner_select on public.sample_metadata;
create policy metadata_owner_select on public.sample_metadata for select to authenticated using ((select auth.uid()) = owner_id);
-- Writes go through the version-checked RPC. Direct client table writes are denied.
revoke all on public.sample_metadata from anon, authenticated;
grant select on public.sample_metadata to authenticated;
create index if not exists sample_metadata_cursor on public.sample_metadata(owner_id, updated_at, content_hash);

create or replace function public.sync_sample(p_hash text, p_metadata jsonb, p_revision bigint, p_deleted boolean default false)
returns jsonb language plpgsql security definer set search_path = '' as $$
declare result public.sample_metadata; uid uuid := auth.uid();
begin
  if uid is null then raise exception 'Authentication required' using errcode = '28000'; end if;
  if p_revision < 0 or p_revision is null or p_hash is null or p_metadata is null or p_deleted is null then
    raise exception 'Invalid sync request' using errcode = '22023';
  end if;
  if p_revision = 0 then
    insert into public.sample_metadata(owner_id, content_hash, metadata, deleted)
      values(uid, p_hash, p_metadata, p_deleted)
      on conflict do nothing returning * into result;
  else
    update public.sample_metadata set metadata = p_metadata, revision = revision + 1,
      deleted = p_deleted, updated_at = clock_timestamp()
      where owner_id = uid and content_hash = p_hash and revision = p_revision returning * into result;
  end if;
  if result.owner_id is null then raise exception 'Revision conflict' using errcode = '40001'; end if;
  return to_jsonb(result);
end;
$$;
revoke all on function public.sync_sample(text,jsonb,bigint,boolean) from public, anon;
grant execute on function public.sync_sample(text,jsonb,bigint,boolean) to authenticated;

-- Original audio upload is explicit/opt-in. Paths are owner UUID/content hash/name.
insert into storage.buckets(id,name,public,file_size_limit,allowed_mime_types)
values('jaspro-private','jaspro-private',false,268435456,array['audio/wav','audio/x-wav','audio/aiff','audio/x-aiff','audio/flac'])
on conflict(id) do update set public = false, file_size_limit = excluded.file_size_limit, allowed_mime_types = excluded.allowed_mime_types;
drop policy if exists jaspro_audio_select on storage.objects;
create policy jaspro_audio_select on storage.objects for select to authenticated
  using(bucket_id = 'jaspro-private' and (storage.foldername(name))[1] = (select auth.uid())::text);
drop policy if exists jaspro_audio_insert on storage.objects;
create policy jaspro_audio_insert on storage.objects for insert to authenticated
  with check(bucket_id = 'jaspro-private' and (storage.foldername(name))[1] = (select auth.uid())::text);
drop policy if exists jaspro_audio_delete on storage.objects;
create policy jaspro_audio_delete on storage.objects for delete to authenticated
  using(bucket_id = 'jaspro-private' and (storage.foldername(name))[1] = (select auth.uid())::text);
commit;
