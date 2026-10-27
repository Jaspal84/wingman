-- Disposable local PostgreSQL fixture. The auth/storage definitions in this file
-- emulate database contracts only; this is not GoTrue or the Storage HTTP service.
create role anon nologin;
create role authenticated nologin;
create schema auth;
create table auth.users(id uuid primary key);
insert into auth.users values ('11111111-1111-1111-1111-111111111111'),('22222222-2222-2222-2222-222222222222');
create function auth.uid() returns uuid language sql stable as $$ select nullif(current_setting('request.jwt.claim.sub',true),'')::uuid $$;
grant usage on schema auth to anon,authenticated;
grant execute on function auth.uid() to anon,authenticated;
create schema storage;
create table storage.buckets(id text primary key,name text,public boolean,file_size_limit bigint,allowed_mime_types text[]);
create table storage.objects(id uuid primary key default gen_random_uuid(),bucket_id text references storage.buckets(id),name text);
alter table storage.objects enable row level security;
create function storage.foldername(text) returns text[] language sql immutable as $$ select string_to_array($1,'/') $$;
grant usage on schema storage to anon,authenticated;
grant select,insert,delete on storage.objects to authenticated;
grant select on storage.objects to anon;
grant execute on function storage.foldername(text) to authenticated;
insert into storage.buckets(id,name,public) values('jaspro-private','jaspro-private',true);
\ir ../Supabase/migrations/0001_library_sync.sql
\ir ../Supabase/migrations/0001_library_sync.sql
do $$ begin if (select public from storage.buckets where id='jaspro-private') is distinct from false or (select file_size_limit from storage.buckets where id='jaspro-private') is distinct from 268435456 or (select allowed_mime_types @> array['audio/wav','audio/aiff','audio/flac'] from storage.buckets where id='jaspro-private') is distinct from true then raise exception 'Bucket privacy/limits not enforced'; end if; end $$;
begin;
set local role authenticated;
set local request.jwt.claim.sub = '11111111-1111-1111-1111-111111111111';
do $$ declare result jsonb; begin
  result := public.sync_sample(repeat('a',64),'{"favorite":true}'::jsonb,0);
  if (result->>'revision')::int <> 1 then raise exception 'Insert revision failure'; end if;
  result := public.sync_sample(repeat('a',64),'{"favorite":false}'::jsonb,1);
  if (result->>'revision')::int <> 2 then raise exception 'Update revision failure'; end if;
  begin perform public.sync_sample(repeat('a',64),'{}'::jsonb,1); raise exception 'Stale revision accepted'; exception when serialization_failure then null; end;
  begin perform public.sync_sample('invalid','{}'::jsonb,0); raise exception 'Invalid hash accepted'; exception when check_violation then null; end;
  begin insert into public.sample_metadata(owner_id,content_hash) values(auth.uid(),repeat('b',64)); raise exception 'Direct write accepted'; exception when insufficient_privilege then null; end;
  if (select count(*) from public.sample_metadata) <> 1 then raise exception 'Owner cannot read own metadata'; end if;
end $$;
insert into storage.objects(bucket_id,name) values('jaspro-private','11111111-1111-1111-1111-111111111111/hash/source.wav');
do $$ begin
  begin insert into storage.objects(bucket_id,name) values('jaspro-private','22222222-2222-2222-2222-222222222222/hash/source.wav'); raise exception 'Cross-owner storage insert accepted'; exception when insufficient_privilege then null; end;
end $$;
set local request.jwt.claim.sub = '22222222-2222-2222-2222-222222222222';
do $$ begin
  if exists(select 1 from public.sample_metadata) then raise exception 'Cross-owner metadata read'; end if;
  if exists(select 1 from storage.objects) then raise exception 'Cross-owner storage read'; end if;
  begin perform public.sync_sample(repeat('a',64),'{}'::jsonb,2); raise exception 'Cross-owner update accepted'; exception when serialization_failure then null; end;
end $$;
delete from storage.objects;
set local request.jwt.claim.sub = '11111111-1111-1111-1111-111111111111';
do $$ begin
  if (select count(*) from storage.objects) <> 1 then raise exception 'Cross-owner delete removed object'; end if;
  perform public.sync_sample(repeat('a',64),'{}'::jsonb,2,true);
  if not (select deleted from public.sample_metadata limit 1) then raise exception 'Tombstone not stored'; end if;
end $$;
set local role anon;
set local request.jwt.claim.sub = '';
do $$ begin
  if exists(select 1 from storage.objects) then raise exception 'Anonymous storage read'; end if;
  begin perform * from public.sample_metadata; raise exception 'Anonymous metadata read accepted'; exception when insufficient_privilege then null; end;
  begin perform public.sync_sample(repeat('a',64),'{}'::jsonb,0); raise exception 'Anonymous RPC accepted'; exception when insufficient_privilege then null; end;
end $$;
rollback;
select 'Supabase SQL owner/RLS/revision/storage-policy checks passed' as result;
