# Dashboard (Vercel)

Planned web dashboard:
- Live readings per node (node ID shown, for the scalability score)
- Bay occupancy + door state (Closed / Opening / Open / Closing / Fault)
- Event history (from Google Sheets)
- Open / Close / Stop controls via MQTT (authorized, and the gateway still enforces local interlocks)

Stack: TBD (e.g. Next.js or plain HTML+JS) with MQTT over WSS.
Secrets in Vercel environment variables, never in Git. See `.env.example` once created.
