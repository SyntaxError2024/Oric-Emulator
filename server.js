const http = require('http');
const fs = require('fs');
const path = require('path');

const server = http.createServer((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
  res.end('Oric Atmos Win32 (MinGW64 C - GDI) - Projet C Windows dans /src');
});

const PORT = 3000;
server.listen(PORT, '0.0.0.0', () => {
  console.log(`Serveur prêt sur http://localhost:${PORT}`);
});
