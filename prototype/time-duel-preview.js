(() => {
  const element = id => document.getElementById(id);
  const seconds = (value, digits = 3) => (value / 1000).toFixed(digits);
  const randomTarget = () => Duel.targets[Math.floor(Math.random() * Duel.targets.length)];
  const previewBattery = 86;
  const audio = DuelAudio.create();
  const steps = [
    ['home', '训练首页'], ['target', '目标时长'], ['first', '先手计时'],
    ['handover', '交接终端'], ['second', '后手计时'], ['sealed', '确认揭晓'],
    ['celebrate', '胜利动画'], ['result', '详细成绩'], ['final', '整场胜利'],
  ];
  const descriptions = {
    home: ['不比手快，比时间感', '两名特工共用一台终端，轮流估算同一个目标时长。误差更小者赢一轮，先赢三轮者获胜。首页 DOWN 切换声音。'],
    target: ['记住目标，按 OK 开始', '目标在 1.0–6.0 秒之间随机抽取，间隔 0.5 秒。不是越快越好，而是越接近目标越好。'],
    first: ['凭感觉，再按一次停止', '计时中不显示目标、数字或进度条，角色保持静止，背景音乐暂停。超过目标 3 秒会自动停止。'],
    handover: ['接过终端，记住同一个目标', '交接页与固件一致：去掉双人 VS，复用目标页的大号时长卡片。先手成绩仍封存；按一次 OK 直接计时，不增加确认步骤。'],
    second: ['同一个目标，再来一次估时', '按 OK 结束后，双方成绩一起锁定。此时不公开胜负，不加分，等待额外一次 OK 揭晓。'],
    sealed: ['先等双方准备好，再揭晓', '两份成绩已锁定，胜负和具体数值都不公开，比分也保持不变。不会自动跳走；按 OK 才揭晓本轮。'],
    celebrate: ['先庆祝，再看详细成绩', '确认之后才判定胜负并加分，单独播放回合获胜动画。约 1.5 秒后进入详细成绩，OK 可跳过动画。'],
    result: ['看完数值，再进入下一回合', '同时展示双方估时（秒）和绝对误差（毫秒）。本页一直停留，按 OK 才继续。平局保留目标重赛；双人模式交替先手。'],
    final: ['整场结束，先回首页', '最后一轮仍先显示回合动画与详细成绩，确认后再显示整场胜利。此页停留 3.5 秒或按 OK 返回首页，再按一次 OK 才开启下一场。'],
    menu: ['已退出游戏', '实机此时返回硬件菜单。网页只展示退出边界，不模拟其他硬件功能；按 OK 重新进入掐秒挑战首页。'],
  };
  const scenarios = [
    { id: 'settlement', name: '确认 → 动画 → 成绩', copy: '先停在封存页。按设备 OK 体验真实节奏，或用下一步逐页查看。', states: ['sealed', 'celebrate', 'result'] },
    { id: 'handover', name: '交接直接计时', copy: '接过终端，按一次 OK 直接开始；再按一次结束。', states: ['handover', 'second', 'sealed'] },
    { id: 'decider', name: '决胜局 → 回首页', copy: '确认前 2:2；确认后 3:2。最后一轮成绩看完，才进入整场胜利。', states: ['deciding-sealed', 'deciding-win', 'deciding-result', 'final', 'home'] },
    { id: 'tie', name: '平局重赛', copy: '1.980 秒与 2.020 秒，误差都是 20 ms。比分不变，交换先手，用同一目标重赛。', states: ['tie-sealed', 'tie-win', 'tie-result', 'tie-target'] },
    { id: 'timeout', name: '超时自动停止', copy: '目标 6 秒，9 秒时自动停止。先手超时后仍先交接，不提前公开成绩。', states: ['timeout', 'timeout-sealed', 'timeout-result'] },
  ];
  let state = Duel.create('duo'), soundOn = true, frozen = false, demo = false, menu = false;
  let timer, holdTimer, heldKey = null, focused = true;
  let lastActivity = performance.now(), dimmed = false;
  let chosen = scenarios[0], scenarioStep = -1;
  const nameOf = player => player === 0 ? '特工 01' : '特工 02';
  const text = (copy, top, left = 8, width = 224, style = '') => `<div class="fw-label ${style}" style="left:${left}px;top:${top}px;width:${width}px">${copy}</div>`;
  const agent = (player, cheering, left, top, animate = false) => `<img class="fw-agent ${animate ? 'fw-cheer' : ''}" src="../assets/images/time-duel/device/duel_agent_${player}${cheering ? '_win' : ''}.png" width="80" height="112" alt="" style="left:${left}px;top:${top}px">`;
  const duo = top => agent(0, false, 18, top) + agent(1, false, 142, top) + text('VS', top + 46, 100, 40, 'fw-small fw-gold');
  const targetPanel = target => `<div class="fw-target">${text('目标时长', 8, 4, 184)}${text(seconds(target, 1) + ' s', 38, 4, 184, 'fw-number fw-gold')}</div>`;

  function screenMarkup(view) {
    let body = '', hint = '', status = `第 ${view.round} 回合  ${view.score.join(' : ')}`;
    if (menu) {
      status = '网页示意';
      body = text('已返回硬件菜单', 44) + text('掐秒挑战', 94, 8, 224, 'fw-title fw-gold') + text('其他硬件功能未模拟', 160);
      hint = 'OK 进入游戏';
    } else switch (view.phase) {
      case 'home':
        status = '时间感训练';
        body = text('掐秒挑战', 0, 8, 224, 'fw-title fw-gold') + text('不比手快，比时间感', 34) + duo(60)
          + text('双人对抗 · 五局三胜', 176)
          + text('DOWN 声音' + (soundOn ? '开' : '关'), 209);
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
    return `<div class="fw-background"></div><div class="fw-header">${text(status, 5, 5, 180)}${text(previewBattery + '%', 7, 185, 50, 'fw-small')}</div><div class="fw-body">${body}</div><div class="fw-hint">${hint}</div>${text(menu ? '硬件菜单仅为示意' : '长按 OK 返回菜单', 300)}`;
  }
  function currentStep(view) {
    if (menu) return 'menu';
    if (view.phase === 'timing') return view.finished.some(Boolean) ? 'second' : 'first';
    return view.phase;
  }
  function statusLabel(view, player) {
    if (menu || view.phase === 'home') return '等待开局';
    if (['celebrate', 'result', 'final'].includes(view.phase)) return view.winner === null ? '平局 · 比分不变' : view.winner === player ? (view.phase === 'final' ? '本场训练优胜' : '本回合获胜') : '本回合落后';
    if (view.finished[player]) return '已完成 · 成绩封存';
    if (view.current !== player) return '等待自己的回合';
    return view.phase === 'timing' ? '正在挑战' : view.phase === 'handover' ? '接过终端，直接开始' : '准备开始';
  }
  function render() {
    const view = Duel.view(state), step = currentStep(view);
    element('screen').innerHTML = screenMarkup(view);
    element('screen').dataset.phase = menu ? 'menu' : view.phase;
    element('screen').classList.toggle('dimmed', dimmed);
    element('screen').classList.toggle('frozen', frozen);
    [0, 1].forEach(player => {
      const seat = element('seat-' + player);
      seat.classList.toggle('active', !menu && ['target', 'timing', 'handover'].includes(view.phase) && player === view.current);
      seat.innerHTML = `<div class="seat-avatar"><img src="../assets/images/time-duel/device/duel_agent_${player}.png" alt=""></div><div><div class="seat-name">${nameOf(player)}</div><div class="seat-status">${statusLabel(view, player)}</div><div class="score-pips" aria-label="已赢 ${view.score[player]} 回合">${[0, 1, 2].map(index => `<i class="pip ${index < view.score[player] ? 'filled' : ''}"></i>`).join('')}</div></div><div class="seat-score">${view.score[player]}</div>`;
    });
    element('up').disabled = true;
    element('down').disabled = menu || view.phase !== 'home';
    element('sound').setAttribute('aria-pressed', soundOn);
    element('sound').textContent = '声音：' + (soundOn ? '开' : '关');
    element('audio-note').textContent = !audio.available ? '浏览器音频不可用，仍可试玩' : !soundOn ? '音乐与按键音已关闭' : !audio.unlocked ? '声音默认开，首次操作后播放' : view.phase === 'timing' ? '计时中不播放背景音乐' : frozen ? '画面预览暂停播放声音' : '背景音乐与按键反馈已开启';
    element('game-round').textContent = menu ? '游戏外 · 菜单边界示意' : view.phase === 'home' ? '240 × 320 · 同机双人界面' : view.phase === 'final' ? '训练结束' : `第 ${view.round} 回合 · 五局三胜`;
    const stepIndex = steps.findIndex(([id]) => id === step);
    element('state-index').textContent = dimmed ? '闲置变暗 · 按键只唤醒，不推进' : stepIndex < 0 ? '当前状态' : `当前画面 ${String(stepIndex + 1).padStart(2, '0')} / 09`;
    element('state-title').textContent = descriptions[step][0];
    element('state-copy').textContent = descriptions[step][1];
    const facts = [['当前目标', view.target === null ? view.phase === 'home' ? '开局随机抽取' : '挑战中隐藏' : seconds(view.target, 1) + ' 秒'], ['当前比分', view.score.join(' : ')], [nameOf(0), statusLabel(view, 0)], [nameOf(1), statusLabel(view, 1)]];
    element('state-facts').innerHTML = facts.map(([label, value]) => `<div><span>${label}</span><strong>${value}</strong></div>`).join('');
    element('flow').querySelectorAll('button').forEach(button => button.setAttribute('aria-current', button.dataset.preview === step ? 'step' : 'false'));
    element('scenario-tabs').querySelectorAll('button').forEach(button => button.setAttribute('aria-pressed', button.dataset.scenario === chosen.id));
    element('demo-label').textContent = frozen ? '画面预览 · 演示数据 · 已暂停' : demo ? '演示数据 · 按实机流程播放' : '自由试玩 · 当前双人实机流程';
    element('resume').hidden = !frozen;
    element('walk-title').textContent = chosen.name + (scenarioStep >= 0 ? ` · ${scenarioStep + 1}/${chosen.states.length}` : '');
    element('walk-copy').textContent = chosen.copy;
    element('next-step').textContent = scenarioStep < 0 ? '开始演示' : scenarioStep === chosen.states.length - 1 ? '重新演示' : '下一步';
  }
  function syncSound(cue) {
    audio.update(soundOn && !dimmed && !frozen && !menu && focused && !document.hidden, state.phase !== 'timing', cue);
  }
  function orchestrate() {
    clearTimeout(timer);
    if (frozen || menu) return;
    const now = performance.now();
    const deadlines = { timing: state.started + state.target + 3000, celebrate: state.phaseStarted + 1500, final: state.phaseStarted + 3500 };
    const phaseDeadline = deadlines[state.phase] ?? Infinity;
    const idleDeadline = dimmed || state.phase === 'timing' ? Infinity : lastActivity + 60000;
    const delay = Math.min(phaseDeadline, idleDeadline) - now;
    if (!Number.isFinite(delay)) return;
    timer = setTimeout(() => {
      dispatch({ type: 'TICK', now: performance.now() });
      if (!dimmed && state.phase !== 'timing' && performance.now() - lastActivity >= 60000) {
        dimmed = true; syncSound(); render();
      }
      orchestrate();
    }, Math.max(1, delay));
  }
  function dispatch(action) {
    const before = state;
    state = Duel.reduce(state, action);
    if (state === before) return;
    let cue = action.type === 'OK' ? 'button' : undefined;
    if (state.phase === 'timing') cue = 'start';
    else if (before.phase === 'timing') cue = 'stop';
    else if (state.phase === 'final') cue = 'match';
    else if (state.phase === 'celebrate' && state.winner !== null) cue = 'win';
    syncSound(cue); render(); orchestrate();
  }
  function touch() {
    focused = true; audio.unlock(); lastActivity = performance.now();
    if (!dimmed) return false;
    dimmed = false; syncSound('button'); render(); orchestrate(); return true;
  }
  function resume() {
    touch(); frozen = false;
    const now = performance.now();
    state = { ...state, phaseStarted: now, started: state.phase === 'timing' ? now : state.started };
    syncSound(state.phase === 'timing' ? 'start' : state.phase === 'celebrate' && state.winner !== null ? 'win' : undefined);
    render(); orchestrate();
  }
  function pressOk() {
    if (touch()) return;
    if (menu) { menu = false; reset(); return; }
    const now = performance.now();
    if (frozen) {
      state = { ...state, phaseStarted: now, lastOk: -Infinity, started: state.phase === 'timing' ? now - (state.current === 0 ? 1982 : 2145) : state.started };
      frozen = false;
    }
    scenarioStep = -1;
    dispatch({ type: 'OK', now, target: randomTarget() });
    orchestrate();
  }
  function releaseHold() {
    clearTimeout(holdTimer); heldKey = null; element('ok').classList.remove('pressed');
  }
  function beginHold(key) {
    if (heldKey !== null) return;
    heldKey = key; pressOk(); element('ok').classList.add('pressed');
    holdTimer = setTimeout(() => {
      releaseHold(); clearTimeout(timer); state = Duel.create('duo');
      menu = true; frozen = false; demo = false; dimmed = false; audio.stop(); render();
    }, 1000);
  }
  function reset() {
    clearTimeout(timer); releaseHold(); touch();
    state = Duel.create('duo'); menu = false; frozen = false; demo = false; dimmed = false; scenarioStep = -1;
    syncSound('button'); render(); orchestrate();
  }
  function toggleSound() {
    if (touch()) return;
    soundOn = !soundOn; syncSound('button'); render(); orchestrate();
  }
  function preview(name) {
    clearTimeout(timer); releaseHold(); audio.stop();
    frozen = true; demo = true; menu = false; dimmed = false;
    state = Duel.create('duo');
    const timeoutScenario = name.startsWith('timeout');
    if (name !== 'home') state = Duel.prepare(state, timeoutScenario ? 6000 : 2000);
    const deciding = name.startsWith('deciding-') || name === 'final';
    if (deciding) { state.score = [2, 2]; state.round = 5; }
    if (['first', 'second', 'handover', 'timeout'].includes(name)) {
      if (name !== 'first') { state.elapsed[0] = name === 'timeout' ? 9000 : 1982; state.automatic[0] = name === 'timeout'; state.current = 1; }
      state.phase = ['handover', 'timeout'].includes(name) ? 'handover' : 'timing';
      state.started = 0;
    } else if (name === 'tie-target') { state.starter = state.current = 1; }
    else if (!['home', 'target'].includes(name)) {
      state.phase = 'sealed';
      state.elapsed = name.startsWith('tie-') ? [1980, 2020] : timeoutScenario ? [9000, 6145] : [1982, 2145];
      state.automatic = [name.startsWith('timeout-'), false]; state.current = 1;
      if (!name.endsWith('sealed')) {
        state = Duel.reduce(state, { type: 'OK', now: 0 });
        if (name === 'result' || name.endsWith('result') || name === 'final') state = Duel.reduce(state, { type: 'OK', now: 200 });
        if (name === 'final') state = Duel.reduce(state, { type: 'OK', now: 400 });
      }
    }
    render();
  }

  element('flow').innerHTML = steps.map(([id, label], index) => `<button data-preview="${id}"><span class="step-num">${String(index + 1).padStart(2, '0')}</span><span class="step-label">${label}</span></button>`).join('');
  element('scenario-tabs').innerHTML = scenarios.map(scenario => `<button data-scenario="${scenario.id}" aria-pressed="false">${scenario.name}</button>`).join('');
  element('walkthrough').innerHTML = '<div><strong id="walk-title"></strong><p id="walk-copy"></p></div><button class="primary" id="next-step">开始演示</button>';
  element('ok').addEventListener('pointerdown', event => {
    if (event.button !== 0) return;
    event.preventDefault(); element('ok').setPointerCapture(event.pointerId); beginHold('pointer');
  });
  ['pointerup', 'pointercancel', 'lostpointercapture'].forEach(event => element('ok').addEventListener(event, releaseHold));
  element('ok').addEventListener('click', event => { if (event.detail === 0) pressOk(); });
  document.addEventListener('keydown', event => {
    if (event.repeat || event.ctrlKey || event.metaKey || event.altKey || /^(INPUT|TEXTAREA|SELECT)$/.test(event.target.tagName)) return;
    if (event.target.closest('button, summary') && event.target.id !== 'ok') return;
    if (['Space', 'Enter'].includes(event.code)) { event.preventDefault(); beginHold(event.code); }
    if (event.code === 'ArrowDown' && state.phase === 'home' && !menu) {
      event.preventDefault(); element('down').click();
    }
  });
  document.addEventListener('keyup', event => { if (event.code === heldKey) releaseHold(); });
  window.addEventListener('blur', () => { focused = false; releaseHold(); audio.stop(); });
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) { releaseHold(); audio.stop(); }
    else { render(); orchestrate(); }
  });
  window.addEventListener('pagehide', () => { clearTimeout(timer); releaseHold(); audio.stop(); });
  element('flow').onclick = event => {
    const button = event.target.closest('button');
    if (button) { scenarioStep = -1; preview(button.dataset.preview); }
  };
  element('scenario-tabs').onclick = event => {
    const button = event.target.closest('button');
    if (button) { chosen = scenarios.find(scenario => scenario.id === button.dataset.scenario); scenarioStep = 0; preview(chosen.states[0]); }
  };
  element('next-step').onclick = () => { scenarioStep = (scenarioStep + 1) % chosen.states.length; preview(chosen.states[scenarioStep]); };
  element('resume').onclick = resume;
  element('reset').onclick = () => reset();
  element('sound').onclick = toggleSound;
  element('down').onclick = toggleSound;
  render(); orchestrate();
})();
