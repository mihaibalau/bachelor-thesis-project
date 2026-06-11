import http from 'k6/http';
import { check } from 'k6';
import { BASE_URL, PER_ACCOUNT_LIMIT, workloadOptions } from './lib/config.js';
import { loginAndLoadAccounts } from './lib/auth.js';

export const options = workloadOptions(8);

export function setup() {
  return loginAndLoadAccounts();
}

export default function (data) {
  const url =
    `${BASE_URL}/api/transactions/summary/monthly` +
    `?per_account_limit=${PER_ACCOUNT_LIMIT}`;

  const res = http.get(url, {
    headers: { Authorization: `Bearer ${data.token}` },
    tags: { workload: 'w3' },
  });

  check(res, { 'analytics 200': (r) => r.status === 200 });
}
