import { FormControl, InputLabel, MenuItem, Select, type SelectChangeEvent } from '@mui/material';
import type { Account } from '../../features/users/types';
import { formatAccountLabel, type AccountLabelFormat } from '../format';

type Props = {
    accounts: Account[];
    value: number | '';
    onChange: (accountId: number) => void;
    label?: string;
    filterCurrency?: string;
    disabled?: boolean;
    format?: AccountLabelFormat;
};

export function AccountSelect({
    accounts,
    value,
    onChange,
    label = 'Account',
    filterCurrency,
    disabled,
    format = 'compact',
}: Props) {
    const filtered = filterCurrency
        ? accounts.filter((a) => a.currency === filterCurrency)
        : accounts;

    const handleChange = (e: SelectChangeEvent<number | ''>) => {
        const v = e.target.value;
        if (v !== '') onChange(Number(v));
    };

    return (
        <FormControl fullWidth disabled={disabled || filtered.length === 0}>
            <InputLabel>{label}</InputLabel>
            <Select label={label} value={value} onChange={handleChange}>
                {filtered.map((a) => (
                    <MenuItem key={a.id} value={a.id}>
                        {formatAccountLabel(a, format)}
                    </MenuItem>
                ))}
            </Select>
        </FormControl>
    );
}
