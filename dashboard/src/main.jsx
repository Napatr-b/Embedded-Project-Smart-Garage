import React from 'react';
import { createRoot } from 'react-dom/client';
import App from './App.jsx';
import Parking from './Parking.jsx';
import './styles.css';
const params = new URLSearchParams(window.location.search);
const showMqttDashboard = params.get('view') === 'garage' || params.has('mode');
createRoot(document.getElementById('root')).render(
  <React.StrictMode>
    {showMqttDashboard ? <App /> : <Parking />}
  </React.StrictMode>,
);
