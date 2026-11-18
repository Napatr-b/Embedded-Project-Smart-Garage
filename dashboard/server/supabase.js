const schema = 'public';
const table = 'garage_events';
const errorMessages = {
  PGRST106: 'The public schema is not exposed by the Data API',
  PGRST125: 'Invalid path in the Data API request',
  PGRST202: 'Could not find the requested function in the schema cache',
  PGRST205: "Could not find the table 'public.garage_events' in the schema cache",
  '42P01': 'The public.garage_events relation does not exist',
  '42501': 'Insufficient privileges for the parking storage request',
};

export function eventStore(env) {
  async function request(query, options = {}) {
    const { SUPABASE_URL: url, SUPABASE_SECRET_KEY: key } = env;
    if (!url || !key) throw new Error('Missing Supabase configuration');
    const endpoint = new URL(`/rest/v1/${table}`, url);
    endpoint.search = new URLSearchParams(query).toString();
    const response = await fetch(endpoint, {
      ...options,
      headers: {
        apikey: key,
        [options.method === 'POST' ? 'Content-Profile' : 'Accept-Profile']: schema,
        ...options.headers,
      },
      redirect: 'error',
      signal: AbortSignal.timeout(4000),
    });
    if (!response.ok) {
      const body = await response.json().catch(() => null);
      const code = typeof body?.code === 'string' &&
        /^(?:PGRST[0-9]{3}|[0-9][A-Z0-9]{4})$/.test(body.code) ? body.code : null;
      // PostgreSQL errors can include row contents. Log only fixed diagnostic messages.
      const message = errorMessages[code] ?? `Request to /rest/v1/${table} (schema ${schema}) failed`;
      throw new Error(`Supabase Data API returned HTTP ${response.status}${code ? ` ${code}` : ''}: ${message}`);
    }
    return response.json();
  }

  return {
    async insert(event) {
      const [inserted] = await request({ on_conflict: 'event_id', select: 'event_id' }, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          Prefer: 'resolution=ignore-duplicates,return=representation',
        },
        body: JSON.stringify(event),
      });
      return Boolean(inserted);
    },
    async find(eventId) {
      const [row] = await request({
        select: 'gateway_id,event_type,occurred_at,state_to',
        event_id: `eq.${eventId}`,
        limit: '1',
      });
      return row;
    },
    async latest(gatewayId) {
      const [row] = await request({
        select: 'state_to,occurred_at',
        gateway_id: `eq.${gatewayId}`,
        event_type: 'eq.OCCUPANCY_CHANGED',
        order: 'occurred_at.desc,ingested_at.desc,event_id.desc',
        limit: '1',
      });
      return row;
    },
  };
}
