import { useTheme } from '@mui/material/styles';
import {
    appPrimaryButtonSx,
    backButtonSx,
    cardGhostButtonSx,
    filterPillSx,
    primaryPillActionButtonSx,
    primaryPillButtonSx,
    softOutlinedButtonSx,
    togglePillGroupSx,
    AUTH_PRIMARY_BUTTON_SX,
} from '../buttonStyles';

export function useButtonStyles() {
    const theme = useTheme();
    return {
        authPrimary: AUTH_PRIMARY_BUTTON_SX,
        appPrimary: appPrimaryButtonSx(theme),
        softOutlined: softOutlinedButtonSx(theme),
        back: backButtonSx(theme),
        cardGhost: cardGhostButtonSx(theme),
        primaryPill: primaryPillButtonSx(theme),
        primaryPillMedium: primaryPillButtonSx(theme, 'medium'),
        primaryPillAction: primaryPillActionButtonSx(theme),
        filterPill: (active: boolean) => filterPillSx(theme, active),
        togglePillGroup: togglePillGroupSx(),
    };
}
