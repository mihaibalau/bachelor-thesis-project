// Shared benchmark configuration. Override via environment variables.
export const BASE_URL = __ENV.BASE_URL || 'http://localhost:6767';
export const EMAIL = __ENV.BENCH_EMAIL || 'gentlix@benchmark.com';
export const PASSWORD = __ENV.BENCH_PASSWORD || 'admin';
export const STATEMENT_FROM = __ENV.STATEMENT_FROM || '2024-01-01';
export const STATEMENT_TO = __ENV.STATEMENT_TO || '2026-12-31';
export const PER_ACCOUNT_LIMIT = __ENV.PER_ACCOUNT_LIMIT || '200000';

export function workloadOptions(vus) {
  return {
    scenarios: {
      warmup: {
        executor: 'constant-vus',
        vus,
        duration: '15s',
        startTime: '0s',
      },
      steady: {
        executor: 'constant-vus',
        vus,
        duration: '30s',
        startTime: '15s',
      },
    },
    summaryTrendStats: ['avg', 'min', 'med', 'max', 'p(90)', 'p(95)', 'p(99)'],
    // Stop quickly when the backend is down (avoids flooding connection-refused logs).
    thresholds: {
      http_req_failed: [{ threshold: 'rate<0.05', abortOnFail: true, delayAbortEval: '3s' }],
    },
  };
}
