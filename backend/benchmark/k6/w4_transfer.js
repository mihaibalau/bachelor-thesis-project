import http from 'k6/http';
import { check } from 'k6';
import { BASE_URL, workloadOptions } from './lib/config.js';
import { loginAndLoadAccounts } from './lib/auth.js';

export const options = workloadOptions(16);

export function setup() {
  return loginAndLoadAccounts();
}

export default function (data) {
  // Alternate direction so neither owned account is drained during the run.
  const fromAccountId =
    __ITER % 2 === 0 ? data.ronAccountId : data.savingsAccountId;
  const toAccountId =
    __ITER % 2 === 0 ? data.savingsAccountId : data.ronAccountId;

  const res = http.post(
    `${BASE_URL}/api/transactions/transfer`,
    JSON.stringify({
      from_account_id: fromAccountId,
      to_account_id: toAccountId,
      value_cents: 1,
    }),
    {
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${data.token}`,
      },
      tags: { workload: 'w4' },
    },
  );

  check(res, { 'transfer 200': (r) => r.status === 200 });
}
