import { defineConfig, loadEnv } from 'vite';
import react from '@vitejs/plugin-react';
import { parkingHandler } from './server/parking.js';

export default defineConfig(({ mode }) => ({
  plugins: [react(), {
    name: 'smart-garage-parking',
    configureServer(server) {
      const env = loadEnv(mode, process.cwd(), ['PARKING_', 'SUPABASE_']);
      const handle = parkingHandler(env, { local: true });
      server.middlewares.use((req, res, next) =>
        req.url?.split('?')[0] === '/api/parking' ? handle(req, res) : next(),
      );
    },
  }],
  server: { watch: { ignored: ['**/docs/**', '**/tmp/**'] } },
}));
