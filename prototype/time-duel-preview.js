import { loadEngine } from './time-duel-engine.js';

const element = id => document.getElementById(id);
const seconds = (value, digits = 3) => (value / 1000).toFixed(digits);
const nowUs = () => Math.round(performance.now() * 1000);
const audio = DuelAudio.create();
let engine, state, timer, holdTimer, held = null;
let soundOn = true, focused = !document.hidden, dimmed = false, exiting = false;
let lastActivity = performance.now();
const nameOf = player => player === 1 && state?.mode === 'ai' ? 'AI 教官' : player === 0 ? '特工 01' : '特工 02';
const descriptions = {
  home: ['不比手快，比时间感', '顶部键切换双人／AI 模式，中部键切换声音，底部 OK 键开始。'],
  target: ['记住目标，按 OK 开始', '按下 OK 开始计时，感觉时间到了再按一次停止。'],
  first: ['凭感觉，再按一次停止', '计时中不显示时间或进度，也不播放背景音乐。'],
  second: ['同一个目标，再来一次', '另一位的成绩仍然封存，按 OK 停止自己的计时。'],
  handover: ['接过终端，记住同一个目标', '按一次 OK 直接开始，先手成绩暂不公开。'],
  ai_wait: ['等待 AI 教官', '教官独立估时，双方成绩完成后等待你确认。'],
  sealed: ['准备好再揭晓', '胜负、数值和比分都尚未揭晓；按 OK 确认。'],
  celebrate: ['本轮训练完成', '动画结束后显示详细成绩，也可按 OK 跳过。'],
  result: ['看完成绩，再继续', '按 OK 进入下一轮；平局使用同一目标重赛。'],
  final: ['整场训练结束', '稍后返回首页；再按 OK 才会开始新比赛。'],
};
  const text = (copy, top, left = 8, width = 224, style = '') => `<div class="fw-label ${style}" style="left:${left}px;top:${top}px;width:${width}px">${copy}</div>`;
  const agent = (player, cheering, left, top, animate = false) => `<img class="fw-agent ${animate ? 'fw-cheer' : ''}" src="../assets/images/time-duel/device/duel_agent_${player}${cheering ? '_win' : ''}.png" width="80" height="112" alt="" style="left:${left}px;top:${top}px">`;
  const duo = top => agent(0, false, 18, top) + agent(1, false, 142, top) + text('VS', top + 46, 100, 40, 'fw-small fw-gold');
  const targetPanel = target => `<div class="fw-target">${text('目标时长', 8, 4, 184)}${text(seconds(target, 1) + ' s', 38, 4, 184, 'fw-number fw-gold')}</div>`;

  function screenMarkup(view) {
    let body = '', hint = '', status = `第 ${view.round} 回合  ${view.score.join(' : ')}`;
    switch (view.phase) {
      case 'home':
        status = '时间感训练';
        body = text('掐秒挑战', 0, 8, 224, 'fw-title fw-gold') + text('不比手快，比时间感', 34) + duo(60)
          + text(view.mode === 'ai' ? '单人练习 · AI 教官' : '双人对抗 · 先赢三局', 176)
          + text('UP 模式 · DOWN 声音' + (soundOn ? '开' : '关'), 209);
        hint = 'OK 开始训练'; break;
      case 'target':
        body = text(nameOf(view.current) + ' 的回合', 7) + text('时间感训练', 35)
          + targetPanel(view.target)
          + text('记住时长，凭感觉掐秒', 193);
        hint = 'OK 开始计时'; break;
      case 'timing':
        body = text(nameOf(view.current) + ' 的回合', 8) + agent(view.current, false, 80, 47)
          + text('TRUST YOUR GUT', 172, 8, 224, 'fw-small fw-gold') + text('感觉时间到了，就按 OK', 203);
        hint = 'OK 停止计时'; break;
      case 'handover':
        body = text(nameOf(view.current) + '，轮到你', 7) + text('训练成绩暂不公开', 35)
          + targetPanel(view.target) + text(view.automatic[1 - view.current] ? '上一位已自动停止' : '接过终端，按 OK 开始', 193);
        hint = 'OK 开始计时'; break;
      case 'ai_wait':
        body = text('AI 教官正在挑战', 30) + agent(1, false, 80, 62) + text('训练成绩暂不公开', 200);
        hint = '等待教官完成'; break;
      case 'sealed':
        body = duo(24) + text('谁的时间感更准？', 153) + text('双方成绩已锁定', 188);
        hint = 'OK 揭晓本轮结果'; break;
      case 'celebrate':
        body = text(view.winner === null ? 'DRAW' : 'ROUND WIN', 0, 5, 230, 'fw-medium fw-gold');
        body += view.winner === null ? text('一样精准，本轮重赛', 28) + duo(57)
          : text(nameOf(view.winner) + ' 赢下本轮', 28) + agent(view.winner, true, 80, 58, true);
        body += text(view.score.join(' : '), 177, 8, 224, 'fw-number fw-gold');
        hint = 'OK 跳过动画'; break;
      case 'result':
        body = text(view.winner === null ? '误差相同，重赛本轮' : nameOf(view.winner) + ' 更精准', 2)
          + text('目标 ' + seconds(view.target, 1) + ' 秒', 30);
        body += [0, 1].map(player => `<div class="fw-card fw-player-${player}" style="left:${player === 0 ? 10 : 126}px">`
          + text(nameOf(player), 6, 2, 100, 'fw-player-color') + text(seconds(view.elapsed[player]), 31, 2, 100, 'fw-medium')
          + text('误差', 58, 2, 100, 'fw-player-color') + text(view.errors[player] + ' ms', 81, 2, 100, 'fw-small')
          + (view.automatic[player] ? text('自动停止', 100, 2, 100, 'fw-player-color') : '') + '</div>').join('');
        body += text('训练战绩  ' + view.score.join(' : '), 197);
        hint = Math.max(...view.score) === 3 ? 'OK 查看整场结果' : view.winner === null ? 'OK 重赛这一局' : 'OK 下一回合'; break;
      case 'final':
        status = '训练结束';
        body = text('训练优胜', 3, 8, 224, 'fw-title fw-gold') + text(nameOf(view.winner) + ' 赢得本场对抗', 38)
          + agent(view.winner, true, 80, 75, true) + text('*', 86, 32, 40, 'fw-number fw-gold') + text('*', 117, 167, 40, 'fw-number fw-gold')
          + text(view.score.join(' : '), 189, 8, 224, 'fw-number fw-gold');
        hint = 'OK 返回首页'; break;
    }
    return `<div class="fw-background"></div><div class="fw-header">${text(status, 5, 18, 160)}${text('--%', 7, 182, 40, 'fw-small')}</div><div class="fw-body">${body}</div><div class="fw-hint">${hint}</div>${text('长按 OK 返回游戏选择', 300)}`;
  }
  function currentStep(view) {
    if (view.phase === 'timing') return view.finished.some(Boolean) ? 'second' : 'first';
    return view.phase;
  }
  function statusLabel(view, player) {
    if (view.phase === 'home') return '等待开局';
    if (['celebrate', 'result', 'final'].includes(view.phase)) return view.winner === null ? '平局 · 比分不变' : view.winner === player ? (view.phase === 'final' ? '本场训练优胜' : '本回合获胜') : '本回合落后';
    if (view.finished[player]) return '已完成 · 成绩封存';
    if (view.current !== player) return '等待自己的回合';
    return view.phase === 'timing' ? '正在挑战' : view.phase === 'handover' ? '接过终端，直接开始' : '准备开始';
  }

function render() {
  if (!engine) return;
  state = engine.state();
  const step = currentStep(state);
  element('screen').innerHTML = screenMarkup(state);
  element('screen').dataset.phase = state.phase;
  element('screen').classList.toggle('dimmed', dimmed);
  for (const player of [0, 1]) {
    const seat = element('seat-' + player);
    seat.classList.toggle('active', ['target', 'timing', 'handover'].includes(state.phase) && player === state.current);
    seat.innerHTML = `<div class="seat-avatar"><img src="../assets/images/time-duel/device/duel_agent_${player}.png" alt=""></div><div><div class="seat-name">${nameOf(player)}</div><div class="seat-status">${statusLabel(state, player)}</div></div><div class="seat-score">${state.score[player]}</div>`;
  }
  element('up').disabled = state.phase !== 'home';
  element('down').disabled = state.phase !== 'home';
  element('sound').disabled = state.phase !== 'home';
  element('sound').setAttribute('aria-pressed', String(soundOn));
  element('sound').textContent = '声音：' + (soundOn ? '开' : '关');
  element('game-round').textContent = state.phase === 'home' ? '240 × 320 · 共享 C 核心' : `第 ${state.round} 回合 · 先赢三局`;
  element('state-index').textContent = dimmed ? '闲置变暗 · 按键只唤醒' : state.mode === 'ai' ? 'AI 练习' : '同机双人';
  element('state-title').textContent = descriptions[step][0];
  element('state-copy').textContent = descriptions[step][1];
  element('state-facts').textContent = `比分 ${state.score.join(' : ')}；${state.target === null ? '目标隐藏' : '目标 ' + seconds(state.target, 1) + ' 秒'}`;
  element('demo-label').textContent = '自由试玩 · 状态与判定来自固件 C/Wasm';
  element('audio-note').textContent = !audio.available ? '浏览器音频不可用，仍可试玩' : !soundOn ? '声音已关闭' : state.phase === 'timing' ? '计时中不播放背景音乐' : !audio.unlocked ? '首次操作后播放声音' : '声音已开启';
}
function syncSound(cue) {
  audio.update(soundOn && focused && !document.hidden && !dimmed && !exiting, state?.phase !== 'timing', cue);
}
function apply(action, cue) {
  if (!engine || exiting) return;
  const before = engine.state();
  const changed = action();
  state = engine.state();
  if (!changed) return;
  if (state.phase === 'timing') cue = 'start';
  else if (before.phase === 'timing') cue = 'stop';
  else if (state.phase === 'final') cue = 'match';
  else if (state.phase === 'celebrate' && state.winner !== null) cue = state.mode === 'ai' && state.winner === 1 ? 'lose' : 'win';
  syncSound(cue);
  render();
}
function release() {
  clearTimeout(holdTimer);
  if (held) element(held.button)?.classList.remove('pressed');
  held = null;
}
function cancelInput() {
  release();
  if (!engine || exiting) return;
  engine.pause(nowUs()); // Cancels the attempt; never shift its clock or grant hidden time.
  dimmed = false;
  lastActivity = performance.now();
  audio.stop();
  render();
}
function leave() {
  if (!engine || exiting) return;
  exiting = true;
  release();
  clearInterval(timer);
  engine.exit();
  audio.stop();
  window.location.assign('/games.html?selected=1');
}
function press(button, source) {
  if (!engine || exiting || held) return;
  held = {button, source};
  element(button).classList.add('pressed');
  focused = true;
  audio.unlock();
  lastActivity = performance.now();
  if (button === 'ok') holdTimer = setTimeout(leave, 1000);
  if (dimmed) {
    dimmed = false;
    syncSound('button');
    render();
    return; // PRESS only wakes; the separate long-press exit remains available.
  }
  if (button === 'ok') {
    apply(() => engine.key(2, nowUs()), 'button');
  } else if (state.phase === 'home') {
    if (button === 'up') apply(() => engine.key(0, nowUs()), 'button');
    else { soundOn = !soundOn; syncSound('button'); render(); }
  }
}
function bindInputs() {
  for (const button of ['up', 'down', 'ok']) {
    const key = element(button);
    key.addEventListener('pointerdown', event => {
      if (event.button !== 0) return;
      event.preventDefault();
      key.setPointerCapture(event.pointerId);
      press(button, 'pointer:' + event.pointerId);
    });
    key.addEventListener('pointerup', event => {
      if (held?.source === 'pointer:' + event.pointerId) release();
    });
    key.addEventListener('pointercancel', cancelInput);
    key.addEventListener('lostpointercapture', () => { if (held?.button === button) cancelInput(); });
  }
  const mapping = {Space:'ok', Enter:'ok', ArrowUp:'up', ArrowDown:'down'};
  window.addEventListener('keydown', event => {
    const button = mapping[event.code];
    if (!button || event.altKey || event.metaKey || event.ctrlKey) return;
    // Keep keyboard activation of utility buttons independent of the hardware keys.
    if (event.target?.closest?.('button') && !['up','down','ok'].includes(event.target.id)) return;
    event.preventDefault();
    if (!event.repeat) press(button, event.code);
  });
  window.addEventListener('keyup', event => {
    if (!mapping[event.code]) return;
    event.preventDefault();
    if (held?.source === event.code) release();
  });
  window.addEventListener('blur', () => { focused = false; cancelInput(); });
  window.addEventListener('focus', () => { focused = true; });
  document.addEventListener('visibilitychange', () => {
    focused = !document.hidden;
    if (document.hidden) cancelInput();
  });
  window.addEventListener('pagehide', () => {
    exiting = true; release(); clearInterval(timer); engine.exit(); audio.stop();
  });
  element('reset').onclick = cancelInput;
  element('sound').onclick = () => { press('down', 'utility'); release(); };
}
async function start() {
  for (const id of ['up','down','ok','sound','reset']) element(id).disabled = true;
  element('screen').textContent = '正在加载共享游戏核心…';
  try {
    engine = await loadEngine();
    state = engine.state();
    for (const id of ['up','down','ok','sound','reset']) element(id).disabled = false;
    bindInputs();
    render();
    timer = setInterval(() => {
      if (!focused || document.hidden || exiting) return;
      apply(() => engine.tick(nowUs()));
      if (!dimmed && state.phase !== 'timing' && performance.now() - lastActivity >= 60000) {
        dimmed = true; syncSound(); render();
      }
    }, 16);
  } catch (error) {
    element('screen').textContent = '游戏加载失败，请使用本地 HTTP 服务打开并重新加载。';
    element('demo-label').textContent = error.message;
    console.error('Time Challenge failed to load', error);
  }
}
start();
