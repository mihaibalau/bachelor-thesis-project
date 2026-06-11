import http from 'k6/http';
import { check } from 'k6';
import { BASE_URL, EMAIL, PASSWORD } from './config.js';

export function loginAndLoadAccounts() {
  const loginRes = http.post(
    `${BASE_URL}/api/users/login`,
    JSON.stringify({ email: EMAIL, password: PASSWORD }),
    { headers: { 'Content-Type': 'application/json' }, tags: { phase: 'setup' } },
  );

  check(loginRes, { 'setup login 200': (r) => r.status === 200 });
  if (loginRes.status !== 200) {
    throw new Error(`login failed: ${loginRes.status} ${loginRes.body}`);
  }

  const { token, user_id: userId } = loginRes.json();

  const profileRes = http.get(`${BASE_URL}/api/users/${userId}`, {
    headers: { Authorization: `Bearer ${token}` },
    tags: { phase: 'setup' },
  });

  check(profileRes, { 'setup profile 200': (r) => r.status === 200 });
  if (profileRes.status !== 200) {
    throw new Error(`profile fetch failed: ${profileRes.status} ${profileRes.body}`);
  }

  const profile = profileRes.json();
  const accounts = profile.accounts || [];
  const ron = accounts.find((a) => a.account_type === 'Regular' && a.currency === 'RON');
  const savings = accounts.find((a) => a.account_type === 'Savings' && a.currency === 'RON');

  if (!ron || !savings) {
    throw new Error(
      'benchmark accounts missing — run: psql ... -f db/seeds/benchmark_seed.sql',
    );
  }

  return {
    token,
    userId,
    ronAccountId: ron.id,
    savingsAccountId: savings.id,
  };
}
