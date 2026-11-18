export function eventStore(env) {
  async function request(query, options = {}) {
    const { SUPABASE_URL: url, SUPABASE_SECRET_KEY: key } = env;
    if (!url || !key) throw new Error('Missing Supabase configuration');
    const endpoint = new URL('/rest/v1/garage_events', url);
    endpoint.search = new URLSearchParams(query).toString();
    const response = await fetch(endpoint, {
      ...options,
      headers: { apikey: key, ...options.headers },
      redirect: 'error',
      signal: AbortSignal.timeout(4000),
    });
    if (!response.ok) throw new Error(`Supabase Data API returned HTTP ${response.status}`);
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
