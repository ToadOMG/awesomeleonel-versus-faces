// Servidor de salas para Versus Faces. Sin dependencias: node server.js
const http = require('http');
const rooms = new Map();
const genCode = () => [...Array(4)].map(() => String.fromCharCode(65 + (Math.random() * 26 | 0))).join('');
const send = (res, o, c = 200) => { res.writeHead(c, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(o)); };
const cos = j => ({ icon: j.icon | 0 || 1, c1: j.c1 | 0, c2: j.c2 | 0 });

http.createServer((req, res) => {
	if (req.method !== 'POST') return send(res, { error: 'POST only' }, 405);
	let body = '';
	req.on('data', d => body += d);
	req.on('end', () => {
		let j = {};
		try { j = JSON.parse(body || '{}'); } catch {}
		if (req.url === '/create') {
			let c; do c = genCode(); while (rooms.has(c));
			rooms.set(c, { levelID: j.levelID | 0, mainLevel: !!j.mainLevel, face: j.face | 0,
				players: { p1: { pct: 0, done: false, ...cos(j) } }, ready: false, t: Date.now() });
			return send(res, { room: c, playerId: 'p1' });
		}
		const room = rooms.get(String(j.room || '').toUpperCase());
		if (!room) return send(res, { error: 'sala no existe' }, 404);
		if (req.url === '/join') {
			if (room.players.p2) return send(res, { error: 'sala llena' }, 409);
			room.players.p2 = { pct: 0, done: false, ...cos(j) };
			room.ready = true;
			const h = room.players.p1;
			return send(res, { playerId: 'p2', levelID: room.levelID, mainLevel: room.mainLevel, face: room.face,
				opp: { icon: h.icon, c1: h.c1, c2: h.c2 } });
		}
		if (req.url === '/status') {
			const g = room.players.p2;
			return send(res, { ready: room.ready, opp: g ? { icon: g.icon, c1: g.c1, c2: g.c2 } : null });
		}
		if (req.url === '/progress') {
			const me = room.players[j.playerId];
			if (!me) return send(res, { error: 'jugador' }, 400);
			me.pct = +j.pct || 0;
			me.done = me.done || !!j.done;
			me.quit = me.quit || !!j.quit;
			me.deaths = j.deaths | 0;
			me.best = Math.max(me.best || 0, +j.best || 0, me.pct);
			const o = room.players[j.playerId === 'p1' ? 'p2' : 'p1'] || {};
			return send(res, { opponentPct: o.pct || 0, opponentDone: !!o.done, opponentQuit: !!o.quit,
				opponentDeaths: o.deaths | 0, opponentBest: o.best || 0 });
		}
		send(res, { error: 'ruta' }, 404);
	});
}).listen(process.env.PORT || 3000, () => console.log('Versus server en puerto', process.env.PORT || 3000));

setInterval(() => { for (const [c, r] of rooms) if (Date.now() - r.t > 3600e3) rooms.delete(c); }, 60000);
