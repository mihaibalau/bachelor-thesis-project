import http from 'k6/http';
import { check } from 'k6';
import {
  BASE_URL,
  STATEMENT_FROM,
  STATEMENT_TO,
  workloadOptions,
} from './lib/config.js';
import { loginAndLoadAccounts } from './lib/auth.js';

export const options = workloadOptions(8);

export function setup() {
  return loginAndLoadAccounts();
}

export default function (data) {
  const url =
    `${BASE_URL}/api/transactions/statement` +
    `?account_id=${data.ronAccountId}` +
    `&from=${STATEMENT_FROM}&to=${STATEMENT_TO}&limit=100`;

  const res = http.get(url, {
    headers: { Authorization: `Bearer ${data.token}` },
    tags: { workload: 'w2' },
  });

  check(res, { 'statement 200': (r) => r.status === 200 });
}
