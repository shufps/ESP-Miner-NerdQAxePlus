import { eASICModel } from './enum/eASICModel';

export interface IHistory {
    hashrate_1m: number[],
    hashrate_10m: number[],
    hashrate_1h: number[],
    hashrate_1d: number[],
    vregTemp: number[],
    asicTemp: number[],
    hasMore: boolean,
    timestamps: number[],
    timestampBase: number
}
