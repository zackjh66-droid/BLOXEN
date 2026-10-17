const $ = id => document.getElementById(id);
let user = null, register = false, reviewedCode = null;
const params = new URLSearchParams(location.search);
const incomingCode = params.get('user_code') || '';
let recoveryToken = new URLSearchParams(location.hash.slice(1)).get('token');
// Remove recovery credentials and the public short code from address/history immediately.
if (location.hash || location.search) history.replaceState(null, '', location.pathname);
$('user-code').value = incomingCode;
const messages = {
  email_delivery_unavailable: 'Recovery and verification email are not configured yet. Keep your password safe.',
  invalid_user_code: 'That code is invalid. Check the eight characters on your device.',
  invalid_request: 'This code is invalid or has already been used. Request a new code on the device.',
  expired_token: 'That code has expired. Choose Retry on your device for a new code.',
  code_already_used: 'That code has already been processed. Request a new one on the device.',
  rate_limited: 'Too many requests at once. Pause, then try again.',
  code_claimed_by_another_account: 'This code belongs to a different account. Cancel on the device and start again.',
  session_expired_or_revoked: 'Your session ended. Please sign in again.',
  EMAIL_NOT_VERIFIED: 'Verify your email before signing in. Check your inbox or the operator’s local outbox.'
};
function notify(message, error = false) { const box = $('notice'); box.textContent = message; box.classList.toggle('error', error); box.hidden = false; }
async function api(path, data) {
  let response;
  try { response = await fetch(path, { method: data === undefined ? 'GET' : 'POST', credentials: 'same-origin', headers: data === undefined ? {} : { 'Content-Type': 'application/json' }, ...(data === undefined ? {} : { body: JSON.stringify(data) }) }); }
  catch { throw new Error('Connection lost. Check your network, then try again.'); }
  const body = await response.json();
  if (!response.ok) throw new Error(messages[body.error] || messages[body.code] || body.message || body.error_description || 'Request failed. Try again.');
  return body;
}
function listenForm(id, handler) {
  $(id).addEventListener('submit', async event => {
    event.preventDefault(); const button = $(id).querySelector('button'); button.disabled = true;
    try { await handler(Object.fromEntries(new FormData(event.currentTarget))); }
    catch (error) { notify(error.message, true); }
    finally { button.disabled = false; }
  });
}
function setMode(next) {
  register = next; $('username-field').hidden = !next;
  $('auth-form').elements.username.required = next;
  $('auth-form').elements.password.autocomplete = next ? 'new-password' : 'current-password';
  $('auth-title').textContent = next ? 'Make yourself at home.' : 'Welcome back.';
  const arrow = document.createElement('span'); arrow.className = 'action-arrow'; arrow.setAttribute('aria-hidden', 'true');
  $('auth-submit').replaceChildren(document.createTextNode(next ? 'Create your account' : 'Sign in'), arrow);
  for (const [id, selected] of [['choose-login', !next], ['choose-register', next]]) { $(id).classList.toggle('selected', selected); $(id).setAttribute('aria-pressed', String(selected)); }
}
$('choose-login').onclick = () => setMode(false); $('choose-register').onclick = () => setMode(true);
async function loadUser() {
  try { user = (await api('/api/me')).user; } catch { user = null; }
  const recovery = location.pathname === '/recover', reset = location.pathname === '/reset', settings = location.pathname === '/settings';
  $('auth-panel').hidden = Boolean(user) || recovery || reset;
  $('signed-in').hidden = !user || recovery || reset; $('logout').hidden = !user;
  $('link-panel').hidden = !user || settings || recovery || reset;
  $('settings-panel').hidden = !user || !settings;
  $('recovery-panel').hidden = !recovery; $('reset-panel').hidden = !reset;
  if (user) {
    $('public-name').textContent = user.username; $('settings-name').value = user.username;
    $('email-status').textContent = `${user.email} · ${user.emailVerified ? 'Email verified' : 'Email not verified'}`;
    $('verify-email').hidden = user.emailVerified;
    if (settings) await loadDevices();
  }
  if (reset && !recoveryToken) { $('reset-form').hidden = true; notify('This recovery link is missing its token. Request another email.', true); }
}
listenForm('auth-form', async data => {
  const result = await api(register ? '/api/auth/sign-up/email' : '/api/auth/sign-in/email', register ? { ...data, name: data.username } : { email: data.email, password: data.password });
  $('auth-form').elements.password.value = '';
  await loadUser();
  notify(user ? 'Signed in. Review your device code to continue.' : 'Account created. Verify your email, then sign in.');
  if (user) $('user-code').focus();
});
$('logout').onclick = async () => { try { await api('/api/auth/sign-out', {}); user = null; reviewedCode = null; $('approval').hidden = true; await loadUser(); notify('Signed out. Your linked devices remain linked until revoked.'); } catch (error) { notify(error.message, true); } };
listenForm('code-form', async data => {
  reviewedCode = null; $('approval').hidden = true;
  const value = data.code.replace(/[-\s]/g, '').toUpperCase();
  const result = await api('/api/link/verify', { user_code: value });
  reviewedCode = result.user_code; $('approval-code').textContent = value.slice(0, 4) + ' ' + value.slice(4);
  $('possession').checked = false; $('approve').disabled = true; $('approval').hidden = false; $('possession').focus();
});
$('possession').onchange = () => { $('approve').disabled = !$('possession').checked; };
async function decide(approve) {
  if (!reviewedCode || (approve && !$('possession').checked)) return;
  $('approve').disabled = $('deny').disabled = true;
  try {
    await api(approve ? '/api/link/approve' : '/api/link/deny', { user_code: reviewedCode });
    reviewedCode = null; $('approval').hidden = true; $('user-code').value = '';
    notify(approve ? 'Approved. Your companion will finish linking automatically. This does not start Fortnite.' : 'Request denied. No device session was issued.');
  } catch (error) { notify(error.message, true); }
  finally { $('deny').disabled = false; $('approve').disabled = !$('possession').checked; }
}
$('approve').onclick = () => decide(true); $('deny').onclick = () => decide(false);
async function loadDevices() {
  const { devices } = await api('/api/devices'); $('devices').replaceChildren();
  if (!devices.length) { const p = document.createElement('p'); p.className = 'muted'; p.textContent = 'No linked devices yet.'; $('devices').append(p); }
  for (const device of devices) {
    const row = document.createElement('div'); row.className = 'device-row'; const info = document.createElement('div'); const title = document.createElement('strong'); title.textContent = device.label;
    const date = document.createElement('small'); date.textContent = `Linked ${new Date(device.createdAt).toLocaleString()} · Last seen ${new Date(device.lastSeenAt).toLocaleString()}`;
    info.append(title, date); const revoke = document.createElement('button'); revoke.className = 'secondary'; revoke.textContent = 'Revoke';
    revoke.onclick = async () => { revoke.disabled = true; try { await api('/api/devices/revoke', { id: device.id }); await loadDevices(); notify('Device revoked. Its session can no longer access your profile.'); } catch (error) { notify(error.message, true); revoke.disabled = false; } };
    row.append(info, revoke); $('devices').append(row);
  }
}
listenForm('name-form', async data => { await api('/api/auth/update-user', { username: data.username, name: data.username }); await loadUser(); notify('Public username updated for the companion test profile.'); });
listenForm('password-form', async data => { await api('/api/auth/change-password', { ...data, revokeOtherSessions: true }); $('password-form').reset(); await loadUser(); notify('Password changed. Other sessions have been revoked.'); });
$('verify-email').onclick = async () => { try { await api('/api/auth/send-verification-email', { email: user.email }); notify('Verification requested. Check your email.'); } catch (error) { notify(error.message, true); } };
listenForm('recovery-form', async data => { await api('/api/auth/request-password-reset', data); notify('If that account exists, a recovery message has been sent. Local development writes messages to the operator’s private data/outbox directory.'); });
listenForm('reset-form', async data => { await api('/api/auth/reset-password', { ...data, token: recoveryToken }); recoveryToken = null; $('reset-form').reset(); $('reset-form').hidden = true; notify('Password reset. Return to sign in. Previous sessions have ended.'); });
await loadUser();
try { const status = await api('/api/status'); if (status.localDevelopment) document.title = 'Hollowwake — Local development'; if (status.emailDelivery === 'unavailable') { $('verify-email').hidden = true; if (location.pathname === '/recover') { $('recovery-form').hidden = true; notify('Recovery email is not configured yet. Contact the project operator; no recovery message can be sent.', true); } } } catch { notify('Account service is unavailable. Try again when connectivity returns.', true); }
