import { IHistory } from './IHistory';

export interface IDashboardV2System {
    uptime: number;
    shutdown: boolean;
    boardError: number;
    overheatTemp: number;
}

export interface IDashboardV2Performance {
    hashRateTimestamp: number;
    hashRate: number;
    hashRate1m: number;
    hashRate10m: number;
    hashRate1h: number;
    hashRate1d: number;
    bestDiff: number;
    bestSessionDiff: number;
    sharesAccepted: number;
    sharesRejected: number;
    frequency: number;
    asicCount: number;
    smallCoreCount: number;
}

export interface IDashboardV2Power {
    watts: number;
    min: number;
    max: number;
    voltage: number;       // V
    voltageMin: number;
    voltageMax: number;
    currentA: number;      // A
    currentAMin: number;
    currentAMax: number;
    coreVoltageActual: number; // V
}

export interface IDashboardV2Fan {
    speed: number;
    rpm: number;
}

export interface IDashboardV2Thermal {
    asicTemp: number;
    vrTemp: number;
    vrTempInt: number;
    asicTemps: number[];
    fans: IDashboardV2Fan[];
}

export interface IDashboardV2Pool {
    host: string;
    port: number;
    user: string;
    connected: boolean;
    activeProtocol: number;
    encrypted: boolean;
    accepted: number;
    rejected: number;
    bestDiff: number;
    pingRtt: number;
    pingLoss: number;
    verifyBlocked?: string;
    poolDifficulty: number;
    networkDifficulty?: number;
    poolDiffErr?: boolean;
}

export interface IDashboardV2Stratum {
    poolMode: number;
    activePoolMode: number;
    usingFallback?: boolean;
    totalBestDiff: number;
    poolBalance?: number;
    pools: IDashboardV2Pool[];
}

export interface IDashboardV2Can {
    hasExtension: boolean;
    enabled: boolean;
    fleetPower?: number;
}

export interface IDashboardV2BlockHeader {
    pool: number;
    blockHeight: number;
    networkDifficulty: number;
    scriptSig?: string;
    coinbaseValueTotalSatoshis?: number;
    coinbaseValueUserSatoshis?: number;
    verificationOk?: boolean;
    verificationFailCount?: number;
    verificationCheckCount?: number;
}

export interface IDashboardV2CoinbasePool {
    mode: number;
    maxFee: number;
    force: boolean;
}

export interface IDashboardV2Coinbase {
    blockHeaders: IDashboardV2BlockHeader[];
    pools: IDashboardV2CoinbasePool[];
}

export interface IDashboardV2 {
    system: IDashboardV2System;
    performance: IDashboardV2Performance;
    power: IDashboardV2Power;
    thermal: IDashboardV2Thermal;
    stratum: IDashboardV2Stratum;
    can: IDashboardV2Can;
    coinbase: IDashboardV2Coinbase;
    history?: IHistory;
}
