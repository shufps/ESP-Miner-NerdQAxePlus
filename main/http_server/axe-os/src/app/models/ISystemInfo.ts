import { eASICModel } from './enum/eASICModel';
import { IHistory } from '../models/IHistory';
import { IStratum } from './IStratum';

export interface ISystemInfo {

    flipscreen: number;
    invertscreen: number;
    autoscreenoff: number;
    power: number,
    maxPower: number,
    minPower: number,
    voltage: number,
    maxVoltage: number,
    minVoltage: number,
    current: number,
    temp: number,
    vrTemp: number,
    hashRateTimestamp: number,
    hashRate: number,
    hashRate_10m: number,
    hashRate_1h: number,
    hashRate_1d: number,
    bestDiff: string,
    bestSessionDiff: string,
    freeHeap: number,
    freeHeapInt: number,
    coreVoltage: number,
    defaultCoreVoltage: number,
    hostname: string,
    hostip: string,
    macAddr: string,
    wifiRSSI: number,
    ssid: string,
    wifiPass: string,
    wifiStatus: string,
    sharesAccepted: number,
    sharesRejected: number,
    uptimeSeconds: number,
    asicCount: number,
    smallCoreCount: number,
    ASICModel: eASICModel,
    deviceModel: string,
    stratumURL: string,
    stratumPort: number,
    stratumUser: string,
    stratumEnonceSubscribe: number,
    fallbackStratumURL: string,
    fallbackStratumPort: number,
    fallbackStratumUser: string,
    fallbackStratumEnonceSubscribe: number,
    stratumDifficulty: number,
    poolDifficulty: number,
    frequency: number,
    defaultFrequency: number,
    version: string,
    invertfanpolarity: number,
    autofanpolarity: number,
    autofanspeed: number,
    fanspeed: number,
    manualFanSpeed: number,
    fanrpm: number,
    coreVoltageActual: number,
    lastResetReason: string,
    jobInterval: number,
    lastpingrtt: number,
    recentpingloss: number,
    stratum_keep: number,
    defaultVrFrequency?: number,
    vrFrequency: number,
    shutdown: boolean,

    stratum: IStratum,

    defaultTheme: string,

    boardtemp1?: number,
    boardtemp2?: number,
    overheat_temp: number,

    pidTargetTemp: number,
    pidP: number,
    pidI: number,
    pidD: number,

    asicTemps?: number[],

    history: IHistory

    otp: boolean,
}

// fields swam is using
export interface ISwarmInfo {
    power: number,
    voltage: number,
    temp: number,
    vrTemp: number,
    bestDiff: string,
    bestSessionDiff: string,
    hostname: string,
    hostip: string,
    sharesAccepted: number,
    sharesRejected: number,
    uptimeSeconds: number,
    asicCount: number,
    ASICModel: eASICModel,
    deviceModel: string,
    poolDifficulty: number,
    version: string,
}
