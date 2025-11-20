export interface IPool {
    connected: boolean,
    poolDiffErr: boolean,
    poolDifficulty: number,
    ping: number,
    accepted: number,
    rejected: number,
    bestDiff: number,
    totalBestDiff: number,
};
