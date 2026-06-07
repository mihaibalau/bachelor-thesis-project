import {
    Avatar,
    Box,
    Chip,
    Divider,
    List,
    ListItem,
    ListItemAvatar,
    ListItemText,
    Stack,
    Typography,
} from '@mui/material';
import { alpha, useTheme } from '@mui/material/styles';
import { formatActivityTimestamp, formatSignedRon } from '../format';
import {
    mapStatementToActivity,
    mapTransactionToActivity,
    type TransactionActivityItem,
} from '../transactionDisplay';
import { categoryIconStyle, getTransactionCategoryIcon } from '../transactionIcons';

type TxInput = {
    id: number;
    from_account_id: number;
    to_account_id: number;
    transaction_type: string;
    value_cents: number;
    recorded_on: string;
    description: string;
    /** Signed amount from GET /transactions/statement — skips from/to inference. */
    signed_value_cents?: number;
};

type Props = {
    items: TxInput[];
    accountId: number;
    showBalanceAfter?: (item: TxInput) => string | undefined;
    groupByDate?: boolean;
};

function ActivityRow({
    tx,
    balanceAfter,
}: {
    tx: TransactionActivityItem;
    balanceAfter?: string;
}) {
    const theme = useTheme();
    const isDark = theme.palette.mode === 'dark';
    const { date, time } = formatActivityTimestamp(tx.recorded_on);
    const amountColor = tx.is_income
        ? theme.palette.success.main
        : isDark
            ? alpha(theme.palette.common.white, 0.88)
            : theme.palette.text.primary;
    const iconStyle = categoryIconStyle(tx.category, isDark);

    return (
        <ListItem
            sx={{
                mb: 0.5,
                px: 1.25,
                transition: 'background-color 120ms ease',
                '&:hover': {
                    bgcolor: isDark
                        ? alpha(theme.palette.background.default, 0.9)
                        : alpha(theme.palette.grey['100'], 0.8),
                },
            }}
            secondaryAction={
                <Stack spacing={0.25} sx={{ alignItems: 'flex-end', minWidth: 96 }}>
                    <Typography variant="subtitle2" sx={{ color: amountColor, fontWeight: 600 }}>
                        {formatSignedRon(tx.amount_cents)}
                    </Typography>
                    {balanceAfter && (
                        <Typography variant="caption" color="text.secondary">
                            Bal. {balanceAfter}
                        </Typography>
                    )}
                </Stack>
            }
        >
            <ListItemAvatar>
                <Avatar
                    sx={{
                        width: 36,
                        height: 36,
                        ...iconStyle,
                    }}
                >
                    {getTransactionCategoryIcon(tx.category)}
                </Avatar>
            </ListItemAvatar>
            <ListItemText
                sx={{ pr: 12 }}
                primary={
                    <Stack direction="row" spacing={1} sx={{ alignItems: 'center' }}>
                        <Typography variant="body2" sx={{ fontWeight: 600 }}>
                            {tx.label}
                        </Typography>
                        {tx.category === 'salary' && tx.is_income && (
                            <Chip
                                size="small"
                                label="Income"
                                color="success"
                                sx={{ height: 18, fontSize: 10, borderRadius: 999 }}
                            />
                        )}
                    </Stack>
                }
                secondary={
                    <Typography variant="caption" color="text.secondary" component="span">
                        {date} · {time}
                        {tx.description ? ` · ${tx.description}` : ''}
                    </Typography>
                }
            />
        </ListItem>
    );
}

function dateKey(iso: string): string {
    return iso.slice(0, 10);
}

function formatGroupDate(iso: string): string {
    const d = new Date(iso);
    return d.toLocaleDateString('en-GB', { weekday: 'long', day: 'numeric', month: 'long', year: 'numeric' });
}

export function TransactionActivityList({ items, accountId, showBalanceAfter, groupByDate }: Props) {
    const theme = useTheme();
    const activities = items.map((tx) => ({
        raw: tx,
        activity:
            tx.signed_value_cents !== undefined
                ? mapStatementToActivity({
                    transaction_id: tx.id,
                    transaction_type: tx.transaction_type,
                    description: tx.description,
                    recorded_on: tx.recorded_on,
                    value_cents: tx.signed_value_cents,
                })
                : mapTransactionToActivity(tx, accountId),
    }));

    if (groupByDate) {
        const groups: { date: string; label: string; entries: typeof activities }[] = [];
        for (const entry of activities) {
            const key = dateKey(entry.activity.recorded_on);
            const last = groups[groups.length - 1];
            if (last?.date === key) {
                last.entries.push(entry);
            } else {
                groups.push({ date: key, label: formatGroupDate(entry.activity.recorded_on), entries: [entry] });
            }
        }

        return (
            <List dense sx={{ m: 0, p: 1, '& .MuiListItem-root': { borderRadius: 1.5 } }}>
                {groups.map((group) => (
                    <Box key={group.date}>
                        <Typography
                            variant="caption"
                            sx={{
                                display: 'block',
                                px: 1.25,
                                py: 1,
                                fontWeight: 700,
                                letterSpacing: 0.4,
                                color: 'text.secondary',
                                textTransform: 'uppercase',
                            }}
                        >
                            {group.label}
                        </Typography>
                        {group.entries.map(({ raw, activity }, index) => (
                            <div key={activity.id}>
                                {index > 0 && (
                                    <Divider sx={{ my: 0.25, borderColor: alpha(theme.palette.divider, 0.6) }} />
                                )}
                                <ActivityRow
                                    tx={activity}
                                    balanceAfter={showBalanceAfter?.(raw)}
                                />
                            </div>
                        ))}
                    </Box>
                ))}
            </List>
        );
    }

    return (
        <List dense sx={{ m: 0, p: 1, '& .MuiListItem-root': { borderRadius: 1.5 } }}>
            {activities.map(({ raw, activity }, index) => (
                <div key={activity.id}>
                    {index > 0 && (
                        <Divider sx={{ my: 0.25, borderColor: alpha(theme.palette.divider, 0.6) }} />
                    )}
                    <ActivityRow tx={activity} balanceAfter={showBalanceAfter?.(raw)} />
                </div>
            ))}
        </List>
    );
}
