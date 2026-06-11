import http from 'k6/http';
import { check } from 'k6';
import { BASE_URL, EMAIL, PASSWORD, workloadOptions } from './lib/config.js';

export const options = workloadOptions(32);

export default function () {
  const res = http.post(
    `${BASE_URL}/api/users/login`,
    JSON.stringify({ email: EMAIL, password: PASSWORD }),
    {
      headers: { 'Content-Type': 'application/json' },
      tags: { workload: 'w1' },
    },
  );

  check(res, { 'login 200': (r) => r.status === 200 });
}
