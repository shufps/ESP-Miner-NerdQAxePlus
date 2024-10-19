import { eASICModel } from './enum/eASICModel';
import { IHistory } from 'src/models/IHistory';

export interface ISystemInfo {

    flipscreen: number;
    invertscreen: number;
    autoscreenoff: number;
    power: number,
    voltage: number,
    current: number,
    temp: number,
    vrTemp: number,
    hashRateTimestamp: number,
    hashRate_10m: number,
    hashRate_1h: number,
    hashRate_1d: number,
    bestDiff: string,
    bestSessionDiff: string,
    freeHeap: number,
    coreVoltage: number,
    hostname: string,
    ssid: string,
    wifiPass: string,
    wifiStatus: string,
    sharesAccepted: number,
    sharesRejected: number,
    uptimeSeconds: number,
    asicCount: number,
    smallCoreCount: number,
    ASICModel: eASICModel,
    stratumURL: string,
    stratumPort: number,
    stratumUser: string,
    frequency: number,
    version: string,
    invertfanpolarity: number,
    autofanspeed: number,
    fanspeed: number,
    fanrpm: number,
    coreVoltageActual: number,
    lastResetReason: string,

    boardtemp1?: number,
    boardtemp2?: number,
    overheat_temp: number,

    history: IHistory
}
