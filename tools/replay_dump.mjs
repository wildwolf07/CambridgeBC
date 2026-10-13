// Headless replay extraction: serves the cached cambc visualiser, loads each replay in Chromium and dumps
// per-round state plus every action step to JSON for offline analysis (tools/replay_stats.py).
//
// usage: node tools/replay_dump.mjs OUT_DIR replay1.zip [replay2.zip ...]
//   (.repl26a bundles and replay.zip files both work; output is OUT_DIR/<basename>.json)
//
// JSON layout:
//   drafts[team] = ChampionDraft ids; types = ChampionType per champion index; teams = team per champion index
//   rounds[r] = {sub:[a,b], ch:[[alive,x,y,hp,maxHp,gems,gold,earned,kills,deaths,assists,chanType,chanTimer,str,foc]...],
//                camps:[[hp,awake,x,y]...], locks:[[team,x,y,hits]...], wards:[[team,x,y]...],
//                groups:[[state(1 alive,2 reward,3 respawning),captureTeam(2 none),captureTimer,respawnRounds]...]}
//   camps = [[type,group,x,y,maxHp]...] (spawn data from the first snapshot)
//   ev = [[round, kind, actorIndex, fromX, fromY, toX, toY, slot, actorKind(1 champion, 2 camp)]...]  kinds: 4 move 5 attack 6 cast 11 recall
//        12 submit 13 buy 20 consumable 3 end-turn (dropped)
import { chromium } from '/opt/node22/lib/node_modules/playwright/index.mjs';
import fs from 'fs';
import http from 'http';
import os from 'os';
import path from 'path';

const [outDir, ...reps] = process.argv.slice(2);
if (!outDir || reps.length === 0) {
    console.error('usage: node tools/replay_dump.mjs OUT_DIR replay.zip ...');
    process.exit(1);
}
fs.mkdirSync(outDir, { recursive: true });

const assetsRoot = path.join(os.homedir(), '.cache', 'cambc', 'assets');
const visDir = fs.readdirSync(assetsRoot).map(d => path.join(assetsRoot, d, 'visualiser')).find(d => fs.existsSync(d));
if (!visDir) throw new Error('no cached visualiser; run `cambc view` once');

const MIME = { '.js': 'text/javascript', '.html': 'text/html', '.wasm': 'application/wasm', '.png': 'image/png', '.css': 'text/css', '.ttf': 'font/ttf' };
let current = null;
const server = http.createServer((req, res) => {
    const u = new URL(req.url, 'http://x');
    if (u.pathname.endsWith('/replay.zip')) {
        res.writeHead(200, { 'Content-Type': 'application/zip' });
        res.end(fs.readFileSync(current));
        return;
    }
    const f = path.join(visDir, u.pathname === '/' ? 'index.html' : decodeURIComponent(u.pathname));
    if (!f.startsWith(visDir) || !fs.existsSync(f)) {
        res.writeHead(404);
        res.end();
        return;
    }
    res.writeHead(200, { 'Content-Type': MIME[path.extname(f)] || 'application/octet-stream' });
    res.end(fs.readFileSync(f));
});
await new Promise(r => server.listen(0, '127.0.0.1', r));
const port = server.address().port;

const browser = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--disable-gpu-vsync'] });

for (const rep of reps) {
    current = path.resolve(rep);
    const name = path.basename(rep).replace(/\.(zip|repl26a)$/, '').replace(/[^A-Za-z0-9_.-]+/g, '_');
    if (fs.existsSync(path.join(outDir, name + '.json'))) continue;
    const page = await browser.newPage({ viewport: { width: 64, height: 64 } });
    try {
        await page.goto(`http://127.0.0.1:${port}/?replay=replay.zip`);
        await page.waitForFunction(() => {
            const root = document.getElementById('root');
            if (!root) return false;
            const key = Object.keys(root).find(k => k.startsWith('__reactContainer'));
            if (!key) return false;
            const seen = new Set();
            const scan = (o, d) => {
                if (!o || typeof o !== 'object' || seen.has(o) || d > 6 || window.__game) return;
                seen.add(o);
                try {
                    if (typeof o.snapshot === 'function' && typeof o.seek === 'function') {
                        window.__game = o;
                        return;
                    }
                } catch (e) {}
                for (const k of Object.keys(o)) {
                    if (k === 'return' || k === '_owner') continue;
                    let v;
                    try { v = o[k]; } catch (e) { continue; }
                    if (v && typeof v === 'object') scan(v, d + 1);
                }
            };
            const stack = [root[key].current || root[key]];
            while (stack.length && !window.__game) {
                const f = stack.pop();
                if (!f) continue;
                scan(f.memoizedState, 0);
                scan(f.memoizedProps, 0);
                if (f.child) stack.push(f.child);
                if (f.sibling) stack.push(f.sibling);
            }
            return !!window.__game;
        }, null, { timeout: 90000, polling: 500 });
        // Stop the visualiser's render loop: extraction only needs the engine, and software WebGL eats the CPU.
        await page.evaluate(() => { window.requestAnimationFrame = () => 0; });
        const data = await page.evaluate(() => {
            const g = window.__game, tl = g.mainTimeline;
            const N = g.timelineInfo(tl).size;
            const rounds = {};
            const ev = [];
            let last = -1, camps = null, types = null, teams = null, drafts = null;
            const num = v => (typeof v === 'bigint' ? Number(v) : v);
            for (let p = 0; p < N; p++) {
                g.seek(tl, p);
                // timelineState() is ~13x cheaper than snapshot(); take a full snapshot only when the round changes.
                const ts = g.timelineState();
                const st = ts.step;
                const s = (ts.round !== last || !types || !camps) ? g.snapshot() : null;
                if (!s) {
                    if (st.kind !== 3 && st.kind !== 19 && st.kind !== 0)
                        ev.push([st.round, st.kind, st.actor.index, st.from.x, st.from.y, st.to.x, st.to.y, st.abilitySlot === undefined ? -1 : st.abilitySlot, st.actor.kind]);
                    continue;
                }
                if (!types && s.champions.length) {
                    types = s.champions.map(c => c.type);
                    teams = s.champions.map(c => c.team);
                    drafts = s.drafts;
                }
                if (!camps && s.camps.length && s.camps.every(c => c.position)) {
                    camps = s.camps.map(c => [c.type, c.group, c.position.x, c.position.y, c.maxHealth]);
                }
                if (s.round !== last) {
                    last = s.round;
                    rounds[s.round] = {
                        sub: s.submittedGems.map(num),
                        ch: s.champions.map(c => [c.alive ? 1 : 0, c.position ? c.position.x : -1, c.position ? c.position.y : -1,
                            Math.round(c.health), Math.round(c.maxHealth), c.gems, c.gold, c.earnedGold,
                            c.kda ? c.kda[0] : 0, c.kda ? c.kda[1] : 0, c.kda ? c.kda[2] : 0, c.channelType, c.channelTimer,
                            Math.round(c.strength.effective), Math.round(c.focus.effective)]),
                        camps: s.camps.map(c => [Math.round(c.health), c.awake ? 1 : 0, c.position ? c.position.x : -1, c.position ? c.position.y : -1]),
                        locks: (s.altarLocks || []).map(l => [l.team, l.position.x, l.position.y, l.hitsRemaining]),
                        wards: (s.wards || []).map(w => [w.team, w.position ? w.position.x : -1, w.position ? w.position.y : -1]),
                        groups: (s.groups || []).map(q => [q.state, q.captureTeam, q.captureTimer, q.respawnRounds]),
                    };
                }
                if (st.kind !== 3 && st.kind !== 19 && st.kind !== 0)
                    ev.push([st.round, st.kind, st.actor.index, st.from.x, st.from.y, st.to.x, st.to.y, st.abilitySlot === undefined ? -1 : st.abilitySlot, st.actor.kind]);
            }
            const s = g.snapshot();
            return { N, outcome: s.outcome, final_round: s.round, final_sub: s.submittedGems.map(num), drafts, types, teams, camps, rounds, ev };
        });
        fs.writeFileSync(path.join(outDir, name + '.json'), JSON.stringify(data));
        console.log(`${name}: rounds=${data.final_round} sub=${data.final_sub} outcome=${data.outcome} steps=${data.N}`);
    } catch (e) {
        console.log(`${name}: FAILED ${String(e).slice(0, 300)}`);
    }
    await page.close();
}
await browser.close();
server.close();
