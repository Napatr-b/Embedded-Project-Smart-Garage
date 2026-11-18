import { defineConfig, loadEnv } from 'vite';
import react from '@vitejs/plugin-react';
import { parkingHandler } from './server/parking.js';

function mountParkingApi(server) {
  const env = loadEnv(server.config.mode, server.config.envDir, ['PARKING_', 'SUPABASE_']);
  const handle = parkingHandler(env, { local: true });
  server.middlewares.use((req, res, next) =>
    req.url?.split('?')[0] === '/api/parking' ? handle(req, res) : next(),
  );
}

export default defineConfig({
  plugins: [react(), {
    name: 'smart-garage-parking',
    configureServer: mountParkingApi,
    configurePreviewServer: mountParkingApi,
  }],
  server: { watch: { ignored: ['**/docs/**', '**/tmp/**'] } },
});
