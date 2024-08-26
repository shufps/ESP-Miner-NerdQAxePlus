import { eASICModel } from './enum/eASICModel';

export interface IHistory {

    hashrate_10m: number[],
    hashrate_1h: number[],
    hashrate_1d: number[],
    timestamps: number[],
    timestampBase: number
}
