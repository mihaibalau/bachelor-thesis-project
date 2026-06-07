export {
    centsToUnits,
    formatActivityTimestamp,
    formatPercent,
    formatRon,
    formatSignedRon,
    formatChartDayLabel,
} from '../../shared/format';

import { centsToUnits, formatChartDayLabel } from '../../shared/format';
import type { DashboardDailySpendingPoint } from './types';

export function prepareSpendingChartData(points: DashboardDailySpendingPoint[]): {
    labels: string[];
    values: number[];
} {
    const endOfToday = new Date();
    endOfToday.setHours(23, 59, 59, 999);

    const filtered = points.filter((p) => {
        const day = new Date(`${p.date}T12:00:00`);
        return day <= endOfToday;
    });

    const usable = filtered.length > 0 ? filtered : points.slice(0, 7);

    return {
        labels: usable.map((p) => formatChartDayLabel(p.date)),
        values: usable.map((p) => centsToUnits(p.cumulative_spending_cents)),
    };
}
