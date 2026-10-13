// Replay analysis inside the local visualiser (cambc view <replay.zip>).
// Paste each block into the page console (or run via a browser automation tool).
//
// 1) Find the live game object through React internals.
(() => {
  const root = document.getElementById('root') || document.body.firstElementChild;
  const key = Object.keys(root).find(k => k.startsWith('__reactContainer'));
  const seen = new Set();
  const scan = (o, d) => {
    if (!o || typeof o !== 'object' || seen.has(o) || d > 6 || window.__game) return;
    seen.add(o);
    try { if (typeof o.snapshot === 'function' && typeof o.seek === 'function') { window.__game = o; return; } } catch (e) {}
    for (const k of Object.keys(o)) { if (k === 'return' || k === '_owner') continue; let v; try { v = o[k]; } catch (e) { continue; } if (v && typeof v === 'object') scan(v, d + 1); }
  };
  const stack = [root[key].current || root[key]];
  while (stack.length && !window.__game) { const f = stack.pop(); if (!f) continue; scan(f.memoizedState, 0); scan(f.memoizedProps, 0); if (f.child) stack.push(f.child); if (f.sibling) stack.push(f.sibling); }
  window.__names = ['ryth', 'vesper', 'vendetta', 'cassi', 'issac', 'stellar', 'prism', 'marcella', 'willow', 'ren', 'varg', 'tavi'];
  return !!window.__game;
})();

// 2) Extract per-round champion state and attack/cast events. Re-run until p == N (each call ~25 s).
//    rounds[r][i] = [team, type, alive, x, y, hp, gems, buffs]; events = [round, kind(4 attack, 6 cast), actorIndex, toX, toY, slot]
(() => {
  const g = window.__game, tl = g.mainTimeline;
  window.__N = g.timelineInfo(tl).size; window.__rounds = window.__rounds || {}; window.__events = window.__events || [];
  window.__p = window.__p || 0; window.__lastRound = window.__lastRound ?? -1;
  const t0 = performance.now(); let p = window.__p;
  while (p < window.__N && performance.now() - t0 < 25000) {
    g.seek(tl, p); const s = g.snapshot(); const st = s.step;
    if (s.round !== window.__lastRound) {
      window.__lastRound = s.round;
      window.__rounds[s.round] = s.champions.map(c => [c.team, c.type, c.alive ? 1 : 0, c.position ? c.position.x : -1, c.position ? c.position.y : -1, Math.round(c.health), c.gems, (c.buffs || []).map(b => b.type).join('')]);
    }
    if (st.kind === 4 || st.kind === 6) window.__events.push([s.round, st.kind, st.actor.index, st.to.x, st.to.y, st.abilitySlot === undefined ? -1 : st.abilitySlot]);
    p++;
  }
  window.__p = p; return { p, N: window.__N };
})();

// 3) Death report: who died where, local numbers 5 rounds earlier, and who hit them.
(() => {
  const R = window.__rounds, E = window.__events, nm = window.__names;
  const cheb = (p, q) => Math.max(Math.abs(p[0] - q[0]), Math.abs(p[1] - q[1]));
  const ok = r => R[r] && R[r].length >= 6; const out = [];
  const last = Math.max(...Object.keys(R).map(Number));
  for (let r = 2; r <= last; r++) {
    if (!ok(r - 1) || !ok(r)) continue; const A = R[r - 1], B = R[r];
    for (let c = 0; c < A.length; c++) {
      if (!(A[c][2] === 1 && B[c][2] === 0)) continue;
      const team = A[c][0], vpos = [A[c][3], A[c][4]], hit = {};
      for (const ev of E) {
        if (ev[0] < r - 8 || ev[0] > r || !ok(ev[0])) continue; const act = R[ev[0]][ev[2]];
        if (!act || act[0] === team) continue;
        if (cheb([ev[3], ev[4]], vpos) <= 2) { const k = nm[act[1]] + (ev[1] === 6 ? '*s' + ev[5] : ''); hit[k] = (hit[k] || 0) + 1; }
      }
      const rr = ok(Math.max(2, r - 5)) ? R[Math.max(2, r - 5)] : A; let al = 0, en = 0;
      for (let d = 0; d < rr.length; d++) { if (d === c || rr[d][2] !== 1) continue; if (cheb([rr[d][3], rr[d][4]], [rr[c][3], rr[c][4]]) <= 7) { if (rr[d][0] === team) al++; else en++; } }
      out.push(`R${r} T${team} ${nm[A[c][1]]} @(${vpos}) hp5ago=${rr[c][5]} gems=${A[c][6]} allies7=${al} enemies7=${en} by=${JSON.stringify(hit)}`);
    }
  }
  return out.join('\n');
})();
