begin;

create table public.door_commands (
  command_id text primary key,
  gateway_id text not null,
  requested_at timestamptz not null default now(),
  requested_by text not null,
  action text not null,
  status text not null,
  expires_at timestamptz not null,
  ack_at timestamptz,
  finished_at timestamptz,
  failure_reason text
);

create table public.garage_events (
  event_id text primary key,
  gateway_id text not null,
  occurred_at timestamptz not null,
  ingested_at timestamptz not null default now(),
  event_type text not null,
  source text not null,
  state_from text,
  state_to text,
  reason_code text,
  command_id text references public.door_commands (command_id),
  check (event_type <> 'OCCUPANCY_CHANGED' or
    (state_to is not null and state_to in ('OCCUPIED', 'VACANT')))
);

create index garage_events_latest_parking_idx
  on public.garage_events (gateway_id, occurred_at desc, ingested_at desc, event_id desc)
  where event_type = 'OCCUPANCY_CHANGED';

create table public.sensor_readings (
  reading_id text primary key,
  node_id text not null,
  node_boot_id text not null,
  seq_no bigint not null check (seq_no >= 0),
  sampled_at timestamptz not null,
  ingested_at timestamptz not null default now(),
  temperature_c double precision,
  humidity_pct double precision,
  light_raw integer,
  aqi integer,
  tvoc_ppb integer,
  eco2_ppm integer,
  unique (node_id, node_boot_id, seq_no)
);

alter table public.garage_events enable row level security;
alter table public.sensor_readings enable row level security;
alter table public.door_commands enable row level security;

revoke all on public.garage_events, public.sensor_readings, public.door_commands
  from public, anon, authenticated;
grant select, insert on public.garage_events, public.sensor_readings to service_role;
grant select, insert, update on public.door_commands to service_role;

commit;
